#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include "../../third-parties/Agg/include/agg_scanline_p.h"
#include "../../third-parties/Agg/include/agg_renderer_scanline.h"
#include "../../third-parties/Agg/include/agg_pixfmt_rgba.h"
#include "../../third-parties/Agg/include/agg_pixfmt_rgb.h"
#include "../../third-parties/Agg/include/agg_rasterizer_scanline_aa.h"
#include "../GfxRaw.h"
#include "../RawBmpFont.h"
#include "../RawGlyphSet.hpp"
#include "RawGlyphBuilder.h"


#define MEM_GRAPH_BITS      32

// 布局字号不变，烘焙字号减 1，使渲染结果上方留 1px（与 Windows 一致）
static const int kBakeHeightInset = 1;

CRawGlyphBuilder::CRawGlyphBuilder() {
    m_refAscent = 0;
    m_baselineY = 0;
}

CRawGlyphBuilder::~CRawGlyphBuilder() {
    destroy();
}

void CRawGlyphBuilder::createMemGraph(int nWidthGraph) {
    m_fontMemGraph.destroy();
    // 烘焙缓冲加高：Core Text 墨迹常高于 FontHeight，先完整画出再压回字号契约
    m_fontMemGraph.create(nWidthGraph, getDrawBufferHeight(), 0, MEM_GRAPH_BITS);

    CGContextSetTextMatrix(m_fontMemGraph.getHandle(), CGAffineTransformMakeScale(1.0, -1.0));

    // 布局仍用 m_fontInfo.height；实际 CT 字体小 1px，上方留出 1px
    int bakeHeight = m_fontInfo.height;
    if (bakeHeight > kBakeHeightInset) {
        bakeHeight -= kBakeHeightInset;
    }

    m_fontLatin.create(m_fontInfo.nameLatin9.c_str(), bakeHeight, m_fontInfo.italic, m_fontInfo.weight, m_fontInfo.isUnderline);
    m_fontOthers.create(m_fontInfo.nameOthers.c_str(), bakeHeight, m_fontInfo.italic, m_fontInfo.weight, m_fontInfo.isUnderline);

    int ascentL = m_fontLatin.getAscent();
    int ascentO = m_fontOthers.getAscent();
    if (ascentL < 1) {
        ascentL = bakeHeight * 4 / 5;
    }
    if (ascentO < 1) {
        ascentO = ascentL;
    }
    m_refAscent = max(ascentL, ascentO);
    if (m_refAscent < 1) {
        m_refAscent = max(1, bakeHeight * 4 / 5);
    }
    // 基线下移 1px，配合小字号使墨迹从第 1 行开始（上方留空）
    m_baselineY = MARGIN_FONT + 1 + m_refAscent;
}

void CRawGlyphBuilder::init(const FontInfoEx &font) {
    if (m_fontMemGraph.isValid() && m_fontInfo.isSame(font)) {
        return;
    }

    m_fontInfo = font;
    createMemGraph(getDrawBufferHeight() * 2);
}

int CRawGlyphBuilder::getHeight() const {
    return m_fontInfo.getHeight();
}

void CRawGlyphBuilder::destroy() {
    m_fontMemGraph.destroy();
    m_fontLatin.destroy();
    m_fontOthers.destroy();
}

void copyBitmpDataFromGraphBuffer(Glyph *pGlyph, agg::pixfmt_rgba32 &bufGraph);

int CRawGlyphBuilder::getDrawBufferHeight() {
    return m_fontInfo.height * 2 + MARGIN_FONT * 2;
}

int CRawGlyphBuilder::getHeightBitmap() {
    return m_fontInfo.height + MARGIN_FONT * 2;
}

// 压回 FontHeight，并让基线落在单元格内的统一位置。
// 绘制时 yDst = topOffset - MARGIN，须保证 yDstEnd <= FontHeight，
// 即 topOffset + heightBitmap <= FontHeight + MARGIN（多 1px 就会裁掉底部）。
static void fitGlyphToBaseline(Glyph *glyph, int fontHeight, int bakeBaselineY, int refAscent) {
    if (!glyph || !glyph->bitmap || glyph->heightBitmap == 0 || fontHeight < 1) {
        return;
    }

    const int srcH = glyph->heightBitmap;
    const int srcW = glyph->widthBitmap;
    // 仅上方留 1px：墨迹落在视觉区域 [1, FontHeight]
    const int topPad = 1;
    const int maxBottom = fontHeight + MARGIN_FONT;
    const int minTop = MARGIN_FONT + topPad;
    const int inkHeight = fontHeight - topPad;
    int baseInCell = minTop + refAscent;
    if (baseInCell > fontHeight) {
        baseInCell = fontHeight;
    }
    if (baseInCell < minTop) {
        baseInCell = minTop;
    }

    int inkToBase = bakeBaselineY - (int)glyph->topOffset;

    if (inkHeight > 0 && srcH > inkHeight) {
        // 超高时从顶部裁掉多余行，优先保留基线以下的 descent/抗锯齿
        const int crop = srcH - inkHeight;
        const int dstH = inkHeight;
        const int dstW = srcW;
        auto *dst = new uint8_t[dstW * dstH * G_PIX_SIZE];
        memcpy(dst, glyph->bitmap + crop * srcW * G_PIX_SIZE, dstW * dstH * G_PIX_SIZE);

        delete[] glyph->bitmap;
        glyph->bitmap = dst;
        glyph->heightBitmap = (uint8_t)dstH;
        inkToBase -= crop;
    }

    int newTop = baseInCell - inkToBase;
    if (newTop < minTop) {
        newTop = minTop;
    }
    if (newTop + glyph->heightBitmap > maxBottom) {
        // 保底部：必要时上移；仍放不下则再从顶部裁行
        newTop = maxBottom - (int)glyph->heightBitmap;
        if (newTop < minTop) {
            newTop = minTop;
        }
        if (newTop + glyph->heightBitmap > maxBottom && glyph->heightBitmap > 1) {
            int overflow = newTop + (int)glyph->heightBitmap - maxBottom;
            if (overflow >= (int)glyph->heightBitmap) {
                overflow = (int)glyph->heightBitmap - 1;
            }
            int dstH = (int)glyph->heightBitmap - overflow;
            int dstW = (int)glyph->widthBitmap;
            auto *dst = new uint8_t[dstW * dstH * G_PIX_SIZE];
            memcpy(dst, glyph->bitmap + overflow * dstW * G_PIX_SIZE, dstW * dstH * G_PIX_SIZE);
            delete[] glyph->bitmap;
            glyph->bitmap = dst;
            glyph->heightBitmap = (uint8_t)dstH;
            newTop = maxBottom - dstH;
            if (newTop < minTop) {
                newTop = minTop;
            }
        }
    }
    glyph->topOffset = (uint8_t)newTop;
}

Glyph *CRawGlyphBuilder::buildGlyph(string &ch) {
    const bool isLatin = isAnsiStr(ch.c_str());
    GfxFont &font = isLatin ? m_fontLatin : m_fontOthers;

    // 单字宽度粗估：加高缓冲后宽度至少留足
    int neededW = getDrawBufferHeight() * 2;
    if (neededW > m_fontMemGraph.width() || getDrawBufferHeight() > m_fontMemGraph.height()) {
        createMemGraph(neededW);
    }

    RawImageData *rawData = m_fontMemGraph.getRawBuff();
    agg::rendering_buffer bufGraph(rawData->buff, rawData->width, rawData->height, rawData->stride);

    int nHeightBitmap = m_fontMemGraph.height();

    uint8_t *pRowSrc = bufGraph.row_ptr(0);
    for (int y = 0; y < nHeightBitmap; y++) {
        memset(pRowSrc, 0, bufGraph.stride_abs());
        pRowSrc += bufGraph.stride();
    }

    int textWidth = 0;
    CFStringRef string = CFStringCreateWithBytes(nullptr, (UInt8*)ch.c_str(), ch.size(), kCFStringEncodingUTF8, false);
    if (string) {
        // Core Text 按基线绘制，拉丁/中文共用 m_baselineY
        textWidth = font.draw(m_fontMemGraph.getHandle(), string, MARGIN_FONT, m_baselineY);
        CFRelease(string);
    } else {
        textWidth = m_fontInfo.height / 2;
    }

    Glyph *glyph = new Glyph;
    glyph->ch = ch;
    glyph->nWidth = (uint8_t)textWidth + 1; // 增加一个像素的字体间距.

    agg::pixfmt_rgba32 pixf(bufGraph);
    copyBitmpDataFromGraphBuffer(glyph, pixf);

    fitGlyphToBaseline(glyph, m_fontInfo.height, m_baselineY, m_refAscent);

    return glyph;
}
