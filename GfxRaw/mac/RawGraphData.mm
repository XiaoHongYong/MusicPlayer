#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <stdlib.h>
#include <string.h>
#include "../GfxRaw.h"
#import "../../Window/mac/ViewMacImp.h"
#include "RawGraphData.h"


CGContextRef createViewContext(WindowHandle handle) {
    CGContextRef viewContext = (CGContextRef)[[NSGraphicsContext currentContext] CGContext];
    assert(viewContext);

    CGContextSaveGState(viewContext);

    ViewMacImp *view = (ViewMacImp *)handle;
    CGContextTranslateCTM(viewContext, 0, [view frame].size.height);
    CGContextScaleCTM(viewContext, 1.0, -1.0);

    return viewContext;
}

CRawGraphData::CRawGraphData() {
}

CRawGraphData::~CRawGraphData() {
    destroy();
}

bool CRawGraphData::create(int cx, int cy, WindowHandle windowHandle, int nBitCount) {
    assert(cx > 0 && cy > 0);
    assert(nBitCount == 32);
    m_windowHandle = windowHandle;

    if (!m_imageData.create(cx, cy, nBitCount)) {
        return false;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // create the bitmap context
    m_context = CGBitmapContextCreate(m_imageData.buff, m_imageData.width, m_imageData.height, 8,
        m_imageData.absStride(), colorSpace,
        // this will give us an optimal RGBA format for the device:
        (kCGImageAlphaPremultipliedLast));
    assert(m_context);
    CGColorSpaceRelease(colorSpace);

    return m_context != nullptr;
}

void CRawGraphData::destroy() {
    if (m_context != nullptr) {
        CGContextRelease(m_context);
        m_context = nullptr;
    }

    m_imageData.free();
}

static void releaseImageData(void *info, const void *data, size_t size) {
    free((void *)data);
}

void CRawGraphData::drawToWindow(int xdest, int ydest, int width, int height, int xsrc, int ysrc, float scaleFactor) {
    assert(m_context);

    CGRect rcDest = CGRectMake(xdest, ydest, width, height);

    if (xsrc + width >= m_imageData.width) {
        width = m_imageData.width - xsrc;
    }
    if (ysrc + height >= m_imageData.height) {
        height = m_imageData.height - ysrc;
    }

    // 源矩形用“位图缓冲区行号”表示：m_imageData 的 stride 为负（自底向上存储），而
    // CGImageCreateWithImageInRect 的 rect.y 是按数据行从 0 开始计的（与缓冲区行号一致），
    // 所以这里直接按缓冲行号裁切，结果和原来的 CGImageCreateWithImageInRect 逐像素相同。
    int nStride = m_imageData.absStride();
    int nSrcX = (int)(xsrc * scaleFactor);
    int nSrcW = (int)(width * scaleFactor);
    int nSrcY = (int)(m_imageData.height - (height + ydest) * scaleFactor);
    int nSrcH = (int)(height * scaleFactor);

    // 与 CGImageCreateWithImageInRect 一样，取与图像边界的交集。
    if (nSrcX < 0) { nSrcW += nSrcX; nSrcX = 0; }
    if (nSrcY < 0) { nSrcH += nSrcY; nSrcY = 0; }
    if (nSrcX + nSrcW > m_imageData.width) { nSrcW = m_imageData.width - nSrcX; }
    if (nSrcY + nSrcH > m_imageData.height) { nSrcH = m_imageData.height - nSrcY; }
    if (nSrcW <= 0 || nSrcH <= 0) {
        return;
    }

    // 只拷贝需要的那几行。早先用 CGBitmapContextCreateImage 生成整张图，
    // 它会对整块位图做一次 COW 的 vm_copy；窗口越大这步越贵，而每帧都要做一次。
    // 拷贝不能省：绘制是延迟到 CA::CG::Queue 上执行的，必须持有独立快照。
    size_t nSize = (size_t)nStride * nSrcH;
    uint8_t *pBuff = (uint8_t *)malloc(nSize);
    if (pBuff == nullptr) {
        return;
    }

    int nCopyBytes = nSrcW * 4;
    const uint8_t *pSrc = m_imageData.buff + nSrcY * nStride + nSrcX * 4;
    uint8_t *pDst = pBuff;
    for (int i = 0; i < nSrcH; i++) {
        memcpy(pDst, pSrc, nCopyBytes);
        if (nCopyBytes < nStride) {
            // 补齐行尾，避免 CG 在缩放时读到未初始化的字节。
            memset(pDst + nCopyBytes, 0, nStride - nCopyBytes);
        }
        pSrc += nStride;
        pDst += nStride;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, pBuff, nSize, releaseImageData);
    CGImageRef partImage = provider ? CGImageCreate(nSrcW, nSrcH, 8, 32, nStride, colorSpace,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast, provider, nullptr, false, kCGRenderingIntentDefault)
        : nullptr;
    CGColorSpaceRelease(colorSpace);

    if (partImage == nullptr) {
        // 图片没建成，provider 也没被持有，释放时回调会 free 掉 pBuff。
        if (provider) {
            CGDataProviderRelease(provider);
        } else {
            free(pBuff);
        }
        return;
    }

    CGDataProviderRelease(provider); // partImage 已持有 provider，pBuff 随图片一起释放。


    CGContextRef viewContext = createViewContext(m_windowHandle);
    CGContextDrawImage(viewContext, rcDest, partImage);
    CGContextRestoreGState(viewContext);

    CGImageRelease(partImage);
}
