#include "../third-parties/Agg/include/agg_scanline_p.h"
#include "../third-parties/Agg/include/agg_renderer_scanline.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgba.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgb.h"
#include "../third-parties/Agg/include/agg_rasterizer_scanline_aa.h"
#include "../third-parties/Agg/include/agg_path_storage.h"
#include "../third-parties/Agg/include/agg_scanline_u.h"
#include "../third-parties/Agg/include/agg_conv_stroke.h"
#include "../third-parties/Agg/include/agg_renderer_mclip.h"
#include "../third-parties/Agg/include/agg_rounded_rect.h"

#include "GfxRaw.h"
#include "RawGraph.h"
#include "ImageBuffBlt.h"
#include <cmath>


#ifdef WIN32
typedef agg::pixfmt_bgra32 pixfmt;
#else
typedef agg::pixfmt_rgba32 pixfmt;
#endif
typedef agg::renderer_mclip<pixfmt> renderer_mclip;

//////////////////////////////////////////////////////////////////////////

CRawGraph::CRawGraph(float scaleFactor) : m_scaleFactor(scaleFactor) {
    m_rawBmpFont = nullptr;

    m_clrText.set(RGB(255, 255, 255));
    m_nOpacityPainting = 255;
    m_ptOrigin.x = m_ptOrigin.y = 0;

    m_bAlphaChannelEnabled = true;
}

CRawGraph::~CRawGraph() {
}

bool CRawGraph::create(int cx, int cy, WindowHandle windowHandle, int nBitCount) {
    if (cx <= 0 || cy <= 0) {
        return false;
    }

    m_rcClip.setLTWH(0, 0, cx, cy);
    m_rcClipScaleMaped = scale(m_rcClip);
    m_ptOrigin.x = m_ptOrigin.y = 0;
    m_width = cx;
    m_height = cy;

    // 与 Window::toPhysical 一致用四舍五入，避免与 GetWindowRect 差 1px
    const int physCx = (int)lround(cx * m_scaleFactor);
    const int physCy = (int)lround(cy * m_scaleFactor);
    if (physCx <= 0 || physCy <= 0) {
        return false;
    }

    if (!CRawGraphData::create(physCx, physCy, windowHandle, nBitCount)) {
        return false;
    }

    m_bAlphaChannelEnabled = (nBitCount == 32);

    return true;
}

void CRawGraph::drawToWindow(int xdest, int ydest, int width, int height, int xsrc, int ysrc) {
    CRawGraphData::drawToWindow(xdest, ydest, width, height, xsrc, ysrc, m_scaleFactor);
}

void CRawGraph::line(float x1, float y1, float x2, float y2) {
    agg::scanline_p8 sl;
    agg::rasterizer_scanline_aa<> ras;

    x1 = mapAndScaleX(x1);
    x2 = mapAndScaleX(x2);
    y1 = mapAndScaleY(y1);
    y2 = mapAndScaleY(y2);

    agg::path_storage ps;
    agg::conv_stroke<agg::path_storage> pg(ps);
    agg::rgba clr(m_pen.m_clrPen.r() / (double)255,
        m_pen.m_clrPen.g() / (double)255,
        m_pen.m_clrPen.b() / (double)255,
        m_pen.m_clrPen.a() * m_nOpacityPainting / 255 / (double)255);

    pg.width(m_pen.m_nWidth);

    ps.remove_all();
    ps.move_to(x1, y1);
    ps.line_to(x2, y2);
    ras.add_path(pg);

    assert(m_imageData.bitCount == 32);

    agg::rendering_buffer buf(m_imageData.buff, m_imageData.width, m_imageData.height, m_imageData.stride);
    pixfmt pixf(buf);
    renderer_mclip rb(pixf);
    rb.add_clip_box(m_rcClipScaleMaped.left, m_rcClipScaleMaped.top, m_rcClipScaleMaped.right, m_rcClipScaleMaped.bottom);

    agg::render_scanlines_aa_solid(ras, sl, rb, clr);
}

void CRawGraph::rectangle(float x, float y, float width, float height) {
    typedef agg::scanline_p8 scanline_type;

    x = mapAndScaleX(x);
    y = mapAndScaleY(y);
    width *= m_scaleFactor;
    height *= m_scaleFactor;

    scanline_type sl;
    agg::rasterizer_scanline_aa<> ras;

    agg::path_storage ps;
    agg::conv_stroke<agg::path_storage> pg(ps);
    agg::rgba clr(m_pen.m_clrPen.r() / (double)255,
        m_pen.m_clrPen.g() / (double)255,
        m_pen.m_clrPen.b() / (double)255,
        m_pen.m_clrPen.a() * m_nOpacityPainting / 255 / (double)255);

    pg.width(m_pen.m_nWidth);

    ps.move_to(x, y);
    ps.line_to(x + width, y);
    ps.line_to(x + width, y + height);
    ps.line_to(x, y + height);
    ps.line_to(x, y);
    ras.add_path(pg);

    assert(m_imageData.bitCount == 32);

    agg::rendering_buffer buf(m_imageData.buff, m_imageData.width, m_imageData.height, m_imageData.stride);
    pixfmt pixf(buf);
    renderer_mclip rb(pixf);
    rb.add_clip_box(m_rcClipScaleMaped.left, m_rcClipScaleMaped.top, m_rcClipScaleMaped.right, m_rcClipScaleMaped.bottom);

    agg::render_scanlines_aa_solid(ras, sl, rb, clr);
}

void CRawGraph::roundedRect(float x, float y, float width, float height, float radius) {
    typedef agg::scanline_p8 scanline_type;

    radius *= m_scaleFactor;
    x = mapAndScaleX(x);
    y = mapAndScaleY(y);
    width *= m_scaleFactor;
    height *= m_scaleFactor;

    scanline_type sl;
    agg::rasterizer_scanline_aa<> ras;

    agg::rounded_rect rr(x, y, x + width, y + height, radius);
    rr.normalize_radius();

    agg::conv_stroke<agg::rounded_rect> pg(rr);
    pg.width(m_pen.m_nWidth);
    ras.add_path(pg);

    assert(m_imageData.bitCount == 32);

    agg::rendering_buffer buf(m_imageData.buff, m_imageData.width, m_imageData.height, m_imageData.stride);
    pixfmt pixf(buf);
    renderer_mclip rb(pixf);
    rb.add_clip_box(m_rcClipScaleMaped.left, m_rcClipScaleMaped.top, m_rcClipScaleMaped.right, m_rcClipScaleMaped.bottom);

    agg::rgba clr(m_pen.m_clrPen.r() / (double)255,
        m_pen.m_clrPen.g() / (double)255,
        m_pen.m_clrPen.b() / (double)255,
        m_pen.m_clrPen.a() * m_nOpacityPainting / 255 / (double)255);

    agg::render_scanlines_aa_solid(ras, sl, rb, clr);
}

void CRawGraph::fillRoundedRect(float x, float y, float width, float height, float radius, const CColor &clrFill) {
    typedef agg::scanline_p8 scanline_type;

    x = mapAndScaleX(x); y = mapAndScaleY(y);
    width *= m_scaleFactor; height *= m_scaleFactor;

    scanline_type sl;
    agg::rasterizer_scanline_aa<> ras;

    agg::rounded_rect rr(x, y, x + width, y + height, radius);
    rr.normalize_radius();

    ras.add_path(rr);

    assert(m_imageData.bitCount == 32);

    agg::rendering_buffer buf(m_imageData.buff, m_imageData.width, m_imageData.height, m_imageData.stride);
    pixfmt pixf(buf);
    renderer_mclip rb(pixf);
    rb.add_clip_box(m_rcClipScaleMaped.left, m_rcClipScaleMaped.top, m_rcClipScaleMaped.right, m_rcClipScaleMaped.bottom);

    agg::rgba clr(clrFill.r() / (double)255, clrFill.g() / (double)255,
        clrFill.b() / (double)255, clrFill.a() * m_nOpacityPainting / 255 / (double)255);

    agg::render_scanlines_aa_solid(ras, sl, rb, clr);
}

void CRawGraph::fillPath(const VecPoints &points, const CColor &clrFill) {
    typedef agg::scanline_p8 scanline_type;

    agg::path_storage path;
    scanline_type sl;
    agg::rasterizer_scanline_aa<> ras;

    path.move_to(mapAndScaleX(points[0].x), mapAndScaleX(points[0].y));
    for (int i = 1; i < points.size(); i++) {
        path.line_to(mapAndScaleX(points[i].x), mapAndScaleX(points[i].y));
    }
    path.close_polygon();

    ras.add_path(path);

    assert(m_imageData.bitCount == 32);

    agg::rendering_buffer buf(m_imageData.buff, m_imageData.width, m_imageData.height, m_imageData.stride);
    pixfmt pixf(buf);
    renderer_mclip rb(pixf);
    rb.add_clip_box(m_rcClipScaleMaped.left, m_rcClipScaleMaped.top, m_rcClipScaleMaped.right, m_rcClipScaleMaped.bottom);

    agg::rgba clr(clrFill.r() / (double)255, clrFill.g() / (double)255,
        clrFill.b() / (double)255, clrFill.a() * m_nOpacityPainting / 255 / (double)255);

    agg::render_scanlines_aa_solid(ras, sl, rb, clr);
}

void CRawGraph::setPen(const CRawPen &pen) {
    m_pen = pen;
    m_pen.m_nWidth *= m_scaleFactor;
}

void CRawGraph::getClipBoundBox(CRect &rc) {
    rc = m_rcClip;
}

void CRawGraph::setClipBoundBox(const CRect &rc) {
    m_rcClip.intersect(rc);
    m_rcClipScaleMaped.intersect(scale(rc));
}

void CRawGraph::resetClipBoundBox(const CRect &rc) {
    if (rc.empty()) {
        m_rcClip.setEmpty();
        m_rcClipScaleMaped.setEmpty();
        return;
    }

    m_rcClip = rc;
    m_rcClip.offsetRect(m_ptOrigin.x, m_ptOrigin.y);
    m_rcClipScaleMaped = scale(m_rcClip);
}

void CRawGraph::clearClipBoundBox() {
    m_rcClip.setLTWH(-m_ptOrigin.x, -m_ptOrigin.y, m_width, m_height);
    m_rcClipScaleMaped = scale(m_rcClip);
}

bool CRawGraph::textOut(float x, float y, cstr_t szText, size_t nLen) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->textOut(this, x, y, m_clrText, szText, nLen, m_bAlphaChannelEnabled);
    }
    return false;
}

bool CRawGraph::drawTextClip(cstr_t szText, size_t nLen, const CRect &rcPos, float xLeftClipOffset) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->drawTextClip(this, rcPos.left, rcPos.top, rcPos.right - rcPos.left, xLeftClipOffset, m_clrText, szText, nLen, m_bAlphaChannelEnabled);
    }
    return false;
}

bool CRawGraph::textOutOutlined(float x, float y, cstr_t szText, size_t nLen, const CColor &clrText, const CColor &clrBorder) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->outlinedTextOut(this, x, y, clrText, clrBorder, szText, nLen, m_bAlphaChannelEnabled);
    }
    return false;
}

bool CRawGraph::drawTextClipOutlined(cstr_t szText, size_t nLen, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, float xLeftClipOffset) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->outlinedDrawTextClip(this, rcPos.left, rcPos.top, rcPos.right - rcPos.left, xLeftClipOffset, clrText, clrBorder, szText, nLen, m_bAlphaChannelEnabled);
    }
    return false;
}

bool CRawGraph::drawTextOutlined(cstr_t szText, size_t nLen, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, uint32_t uFormat) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->outlinedDrawTextEx(this, rcPos, clrText, clrBorder, szText, nLen, uFormat, m_bAlphaChannelEnabled);
    }

    return false;
}

bool CRawGraph::drawText(cstr_t szText, size_t nLen, const CRect &rcPos, uint32_t uFormat) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->drawTextEx(this, rcPos, m_clrText, szText, nLen, uFormat, m_bAlphaChannelEnabled);
    }

    return false;
}

bool CRawGraph::getTextExtentPoint32(cstr_t szText, size_t nLen, CSize *pSize) {
    if (nLen == -1) {
        nLen = strlen(szText);
    }

    if (m_rawBmpFont) {
        return m_rawBmpFont->getTextExtentPoint32(szText, nLen, pSize);
    }

    return false;
}

void CRawGraph::setFont(CRawBmpFont *font) {
    assert(font);
    if (!font) {
        return;
    }

    font->setScaleFactor(m_scaleFactor);
    m_rawBmpFont = font;
}

void CRawGraph::resetAlphaChannel(uint8_t alpha, const CRect &rcDst) {
    if (!m_bAlphaChannelEnabled) {
        return;
    }

    assert(m_imageData.bitCount == 32);

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    int strideModify = (xMax - x) * PIX_SIZE;

    // Modify alpha channel directly
    uint8_t *row = m_imageData.pixPtr(x, y) + PIX_A;
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p += PIX_SIZE) {
            *p = alpha;
        }
        row += m_imageData.stride;
    }
}

void CRawGraph::multiplyAlpha(uint8_t alpha, const CRect &rcDst) {
    if (!m_bAlphaChannelEnabled) {
        return;
    }

    assert(m_imageData.bitCount == 32);

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    // PIX_SIZE is the pixel size(rgba).
    int strideModify = (xMax - x) * PIX_SIZE;

    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p ++) {
            *p = (uint8_t)((*p * alpha) / 256);
        }
        row += m_imageData.stride;
    }
}

void CRawGraph::vertAlphaFadeOut(const CRect &rcDst, bool bTop) {
    if (!m_bAlphaChannelEnabled) {
        return;
    }

    assert(m_imageData.bitCount == 32);

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    int strideModify = (xMax - x) * PIX_SIZE;

    float fStep = (float)1 / scale(rcDst.height()), fStart;
    if (bTop) {
        fStart = 0;
    } else {
        fStep = -fStep;
        fStart = 1;
    }

    fStart += fStep * (y - mapAndScaleY(rcDst.top));
    assert(fStart <= 1.0 && fStart >= 0.0);

    // PIX_SIZE is the pixel size(rgba).
    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p ++) {
            *p = (uint8_t)(*p * fStart);
        }
        row += m_imageData.stride;
        fStart += fStep;
    }
}

void CRawGraph::vertFadeOut(const CRect &rcDst, const CColor &clrBg, bool bTop) {
    assert(m_imageData.bitCount == 32);
    if (rcDst.empty()) {
        return;
    }

    int a = clrBg.getAlpha() * m_nOpacityPainting / 255;
    int r = clrBg.r() * a / 255;
    int g = clrBg.g() * a / 255;
    int b = clrBg.b() * a / 255;

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }

    int strideModify = (xMax - x) * PIX_SIZE;

    int nFadeRange = scale(rcDst.height());
    int nFadeStart = y - mapAndScaleY(rcDst.top);

    if (!bTop) {
        nFadeStart = nFadeRange - nFadeStart;
    }

    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p += PIX_SIZE) {
            p[PIX_B] = (uint8_t)((p[PIX_B] - b) * nFadeStart / nFadeRange + b);
            p[PIX_G] = (uint8_t)((p[PIX_G] - g) * nFadeStart / nFadeRange + g);
            p[PIX_R] = (uint8_t)((p[PIX_R] - r) * nFadeStart / nFadeRange + r);
            p[PIX_A] = (uint8_t)((p[PIX_A] - a) * nFadeStart / nFadeRange + a);
        }
        row += m_imageData.stride;
        if (bTop) {
            nFadeStart++;
        } else {
            nFadeStart--;
        }
    }
}


void CRawGraph::horzAlphaFadeOut(const CRect &rcDst, bool bLeft) {
    if (!m_bAlphaChannelEnabled) {
        return;
    }

    assert(m_imageData.bitCount == 32);

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    int strideModify = (xMax - x) * PIX_SIZE;

    float fStep = (float)1 / scale(rcDst.width()), fStart;
    if (bLeft) {
        fStart = 0;
    } else {
        fStep = -fStep;
        fStart = 1;
    }

    fStart += fStep * (x - mapAndScaleX(rcDst.left));
    assert(fStart <= 1.0 && fStart >= 0.0);

    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        float f = fStart;

        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p += PIX_SIZE) {
            p[PIX_B] = (uint8_t)(p[PIX_B] * f);
            p[PIX_G] = (uint8_t)(p[PIX_G] * f);
            p[PIX_R] = (uint8_t)(p[PIX_R] * f);
            p[PIX_A] = (uint8_t)(p[PIX_A] * f);
            f += fStep;
        }
        row += m_imageData.stride;
    }
}


void CRawGraph::horzFadeOut(const CRect &rcDst, const CColor &clrBg, bool bLeft) {
    assert(m_imageData.bitCount == 32);

    int a = clrBg.getAlpha();
    int r = clrBg.r() * a / 255;
    int g = clrBg.g() * a / 255;
    int b = clrBg.b() * a / 255;

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    int strideModify = (xMax - x) * PIX_SIZE;

    float fStep = (float)1 / scale(rcDst.width()), fStart;
    if (bLeft) {
        fStart = 0;
    } else {
        fStep = -fStep;
        fStart = 1;
    }

    fStart += fStep * (x - mapAndScaleX(rcDst.left));
    assert(fStart <= 1.0 && fStart >= 0.0);

    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        float f = fStart;

        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p += PIX_SIZE) {
            p[PIX_B] = (uint8_t)((p[PIX_B] - b) * f + b);
            p[PIX_G] = (uint8_t)((p[PIX_G] - g) * f + g);
            p[PIX_R] = (uint8_t)((p[PIX_R] - r) * f + r);
            p[PIX_A] = (uint8_t)((p[PIX_A] - a) * f + a);
            f += fStep;
        }
        row += m_imageData.stride;
    }
}

void CRawGraph::fillRectXOR(const CRect &rcDst, const CColor &clrFill) {
    assert(m_imageData.bitCount == 32);

    uint8_t r = clrFill.r(), g = clrFill.g(), b = clrFill.b();

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }
    int strideModify = (xMax - x) * PIX_SIZE;

    uint8_t *row = m_imageData.pixPtr(x, y);
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        for (uint8_t *p = row; p < end; p += PIX_SIZE) {
            p[PIX_B] = p[PIX_B] ^ b;
            p[PIX_G] = p[PIX_G] ^ g;
            p[PIX_R] = p[PIX_R] ^ r;
        }
        row += m_imageData.stride;
    }
}


template<class _PixRGBBlender, class _PixAlphaBlender>
void fillRectMask(RawImageData *pImage, int x, int y, int xMax, int yMax, RawImageData *pImageMask, int xMask, int yMask, uint8_t *srcPixel) {
    int nPixSizeMask;

    assert(pImage->bitCount == 32);
    const int nPixSize = 4;

    if (pImageMask->bitCount == 32) {
        nPixSizeMask = 4;
    } else if (pImageMask->bitCount == 24) {
        nPixSizeMask = 3;
    } else {
        assert(0);
        return;
    }

    int strideModify = (xMax - x) * nPixSize;

    uint8_t *row = pImage->pixPtr(x, y);
    uint8_t *pRowMask = pImageMask->pixPtr(xMask, yMask);
    for (; y < yMax; y++) {
        uint8_t *end = row + strideModify;
        uint8_t *pMask = pRowMask;
        for (uint8_t *p = row; p < end; p += nPixSize, pMask += nPixSizeMask) {
            if (pMask[3] != 0) {
                _PixRGBBlender::blend(p, srcPixel, pMask);
                _PixAlphaBlender::blend(p, srcPixel, pMask);
            }
        }
        row += pImage->stride;
        pRowMask += pImageMask->stride;
    }
}

void CRawGraph::fillRect(const CRect &rcDst, CRawImage &imageMask, const CRect &rcMask, const CColor &clrFill, BlendPixMode bpm) {
    assert(m_imageData.bitCount == 32);

    uint8_t srcPixel[4];

    srcPixel[PIX_R] = clrFill.r() * clrFill.getAlpha() / 255;
    srcPixel[PIX_G] = clrFill.g() * clrFill.getAlpha() / 255;
    srcPixel[PIX_B] = clrFill.b() * clrFill.getAlpha() / 255;
    srcPixel[PIX_A] = clrFill.getAlpha();

    if (!imageMask.isValid()) {
        return;
    }

    RawImageData *maskImageData = imageMask.getRawImageData(getScaleFactor()).get();
    if (maskImageData->pixFormat != PF_RGBA32) {
        return;
    }

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }

    CRect realRcMask = scale(rcMask);

    if (!m_bAlphaChannelEnabled) {
        bpm &= ~BPM_CHANNEL_ALPHA;
    }

    // Fill region must be within mask image
    if (xMax - x > realRcMask.width()) {
        xMax = realRcMask.width() + x;
    }
    if (yMax - y > realRcMask.height()) {
        yMax = realRcMask.height() + y;
    }

    int xSrc = realRcMask.left, ySrc = realRcMask.top;

    if (isFlagSet(bpm, BPM_OP_BLEND)) {
        // blend
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_blend_rgb, mask_blend_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            } else {
                ::fillRectMask<mask_blend_rgb, mask_blend_copy_none>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_blend_copy_none, mask_blend_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        }
    } else if (isFlagSet(bpm, BPM_OP_MULTIPLY)) {
        // Multiply
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_multiply_rgb, mask_multiply_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            } else {
                ::fillRectMask<mask_multiply_rgb, mask_blend_copy_none>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_blend_copy_none, mask_multiply_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        }
    } else {
        // copy
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_copy_rgb, mask_copy_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            } else {
                ::fillRectMask<mask_copy_rgb, mask_blend_copy_none>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRectMask<mask_blend_copy_none, mask_copy_alpha>(&m_imageData, x, y, xMax, yMax, maskImageData, xSrc, ySrc, srcPixel);
            }
        }
    }
}


template<class _PixRGBBlender, class _PixAlphaBlender, class _PixRGBBlender_Opacity, class _PixAlphaBlender_Opacity>
void fillRect(RawImageData *pImage, int x, int y, int xMax, int yMax, uint8_t *srcPixel, uint8_t alpha) {
    assert(pImage->bitCount == 32);
    const int nPixSize = 4;

    int strideModify = (xMax - x) * nPixSize;
    uint8_t *row = pImage->pixPtr(x, y);

    if (alpha == 255) {
        for (; y < yMax; y++) {
            uint8_t *end = row + strideModify;
            for (uint8_t *p = row; p < end; p += nPixSize) {
                _PixRGBBlender::blend(p, srcPixel);
                _PixAlphaBlender::blend(p, srcPixel);
            }
            row += pImage->stride;
        }
    } else {
        for (; y < yMax; y++) {
            uint8_t *end = row + strideModify;
            for (uint8_t *p = row; p < end; p += nPixSize) {
                _PixRGBBlender_Opacity::blend(p, srcPixel, alpha);
                _PixAlphaBlender_Opacity::blend(p, srcPixel, alpha);
            }
            row += pImage->stride;
        }
    }
}

void fillRect(RawImageData *pImage, int x, int y, int xMax, int yMax, const CColor &clrFill, BlendPixMode bpm, int nOpacityFill) {
    assert(pImage);
    assert(pImage->width >= xMax && pImage->height >= yMax);

    uint8_t srcPixel[4];
    srcPixel[PIX_R] = clrFill.r() * clrFill.getAlpha() / 255;
    srcPixel[PIX_G] = clrFill.g() * clrFill.getAlpha() / 255;
    srcPixel[PIX_B] = clrFill.b() * clrFill.getAlpha() / 255;
    srcPixel[PIX_A] = clrFill.getAlpha();

    if (isFlagSet(bpm, BPM_OP_BLEND)) {
        // blend
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<blend_rgb, blend_alpha, blend_rgb_opacity, blend_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            } else {
                ::fillRect<blend_rgb, blend_copy_none, blend_rgb_opacity, blend_copy_none_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<blend_copy_none, blend_alpha, blend_copy_none_opacity, blend_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            }
        }
    } else if (isFlagSet(bpm, BPM_OP_MULTIPLY)) {
        // Multiply
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<multiply_rgb, multiply_alpha, multiply_rgb_opacity, multiply_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            } else {
                ::fillRect<multiply_rgb, blend_copy_none, multiply_rgb_opacity, blend_copy_none_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<blend_copy_none, multiply_alpha, blend_copy_none_opacity, multiply_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            }
        }
    } else {
        // copy
        if (isFlagSet(bpm, BPM_CHANNEL_RGB)) {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<copy_rgb, copy_alpha, copy_rgb_opacity, copy_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            } else {
                ::fillRect<copy_rgb, blend_copy_none, copy_rgb_opacity, blend_copy_none_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
                // FillRectCopyRGB(pImage, x, y, xMax, yMax, srcPixel);
            }
        } else {
            if (isFlagSet(bpm, BPM_CHANNEL_ALPHA)) {
                ::fillRect<blend_copy_none, copy_alpha, blend_copy_none_opacity, copy_alpha_opacity>(pImage, x, y, xMax, yMax, srcPixel, nOpacityFill);
            }
        }
    }
}

void CRawGraph::fillRect(const CRect &rcDst, const CColor &clrFill, BlendPixMode bpm) {
    assert(m_imageData.bitCount == 32);

    int x, y, xMax, yMax;
    if (!clipMapScaleRect(rcDst, x, y, xMax, yMax)) {
        return;
    }

    if (!m_bAlphaChannelEnabled) {
        bpm &= ~BPM_CHANNEL_ALPHA;
    }

    ::fillRect(&m_imageData, x, y, xMax, yMax, clrFill, bpm, m_nOpacityPainting);
}

void CRawGraph::fillRectPhysical(int x, int y, int xMax, int yMax, const CColor &clrFill, BlendPixMode bpm) {
    assert(m_imageData.bitCount == 32);

    x = max(x, (int)m_rcClipScaleMaped.left);
    y = max(y, (int)m_rcClipScaleMaped.top);
    xMax = min(xMax, (int)m_rcClipScaleMaped.right);
    yMax = min(yMax, (int)m_rcClipScaleMaped.bottom);
    if (x >= xMax || y >= yMax) {
        return;
    }

    if (!m_bAlphaChannelEnabled) {
        bpm &= ~BPM_CHANNEL_ALPHA;
    }

    ::fillRect(&m_imageData, x, y, xMax, yMax, clrFill, bpm, m_nOpacityPainting);
}

void CRawGraph::bltImage(int xDest, int yDest, int widthDest, int heightDest, RawImageData *image, int xSrc, int ySrc, BlendPixMode bpm, int nOpacitySrc)
{
    assert(image);

    // 逻辑 → 物理：与 create()/toPhysical 一致用四舍五入，避免非整型 DPI 下截断少一截。
    xDest = (int)lround((xDest + m_ptOrigin.x) * m_scaleFactor);
    yDest = (int)lround((yDest + m_ptOrigin.y) * m_scaleFactor);
    widthDest = (int)lround(widthDest * m_scaleFactor);
    heightDest = (int)lround(heightDest * m_scaleFactor);
    xSrc = (int)lround(xSrc * m_scaleFactor);
    ySrc = (int)lround(ySrc * m_scaleFactor);

    widthDest = min(widthDest, (int)image->width);
    heightDest = min(heightDest, (int)image->height);

    CRect rcDst = m_rcClipScaleMaped;

    // get the clip box to copy
    rcDst.right = min((int)rcDst.right, xDest + widthDest);
    rcDst.bottom = min((int)rcDst.bottom, yDest + heightDest);

    // determine the start position of Graph, image, mask.
    if (xDest >= rcDst.left) {
        rcDst.left = xDest;
    } else {
        xSrc += rcDst.left - xDest;
    }
    if (yDest >= rcDst.top) {
        rcDst.top = yDest;
    } else {
        ySrc += rcDst.top - yDest;
    }

    if (rcDst.empty()) {
        return;
    }

    if (!getEnableAlphaChannel()) {
        bpm &= ~BPM_CHANNEL_ALPHA;
    }

    bltRawImage(getRawBuff(), rcDst, image, xSrc, ySrc, bpm, getOpacityPainting());
}

CPoint CRawGraph::resetOrigin(const CPoint &ptOrg) {
    // 重置 origin
    auto ptOrgOld = m_ptOrigin;

    // 先恢复 clipBox 的 origin
    m_rcClip.offsetRect(-m_ptOrigin.x, -m_ptOrigin.y);

    // 设置新的
    m_rcClip.offsetRect(ptOrg.x, ptOrg.y);
    m_rcClipScaleMaped = scale(m_rcClip);

    m_ptOrigin = ptOrg;

    return ptOrgOld;
}

void CRawGraph::mapAndScale(CRect &r) const {
    r.left = mapAndScaleX(r.left);
    r.top = mapAndScaleY(r.top);
    r.right = mapAndScaleX(r.right);
    r.bottom = mapAndScaleY(r.bottom);
}

bool CRawGraph::clipMapScaleRect(const CRect &rc, int &x, int &y, int &xMax, int &yMax) {
    x = max(rc.left, m_rcClip.left);
    y = max(rc.top, m_rcClip.top);

    xMax = min(rc.right, m_rcClip.right);
    if (xMax < x) {
        xMax = x;
    }

    yMax = min(rc.bottom, m_rcClip.bottom);
    if (yMax < y) {
        yMax = y;
    }

    x = mapAndScaleX(x);
    y = mapAndScaleY(y);
    xMax = mapAndScaleX(xMax);
    yMax = mapAndScaleY(yMax);

    return x < xMax && y < yMax;
}
