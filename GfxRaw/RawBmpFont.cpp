#include "../third-parties/Agg/include/agg_scanline_p.h"
#include "../third-parties/Agg/include/agg_renderer_scanline.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgba.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgb.h"
#include "../third-parties/Agg/include/agg_rasterizer_scanline_aa.h"
#include "../third-parties/Agg/include/agg_pixfmt_gray.h"
#include "../third-parties/Agg/include/agg_blur.h"
#include "GfxRaw.h"
#include "RawBmpFont.h"
#include "ImageBuffBlt.h"
#include "RawGlyphSet.hpp"
#include <cmath>


inline uint8_t RGBToGray(int r, int g, int b) {
    // 5 = 四舍五入
    return (r * 3 + g * 6 + b + 5) / 10;
}

bool inline isCharComposed(uint32_t c) {
    // The Unicode range for Devanagari is U+0900 .. U+097F.
    return (c >= 0x590 && c <= 0x6ff) || (c >= 0x0E00 && c <= 0x0E7F) || (c >= 0x0900 && c <= 0x097F);
}

void inline readUtf8Char(const char *str, string &dst) {
    while (true) {
        unsigned char firstChar = (unsigned char)(str[0]);
        int n;

        if (firstChar < 0xc0)       n = 1;
        else if (firstChar < 0xe0)  n = 2;
        else if (firstChar < 0xf0)  n = 3;
        else                        n = 1;
        dst.append(str, n);

        // Is composed char?
        uint32_t value = 0;
        for (int i = 0; str[i] != 0; i++) {
            value <<= 8;
            value |= uint8_t(str[i]);
        }
        if (!isCharComposed(value)) {
            break;
        }
        str += n;
    }
}

// Right to left string iterator
class RtlStringIterator {
public:
    typedef string        CharType;

    RtlStringIterator(cstr_t text) {
        m_text = text;
        m_nPos = 0;
        readCurChar();
    }

    void operator = (cstr_t text) {
        m_text = text;
        m_nPos = 0;
        readCurChar();
    }

    // Is end of string
    bool isEOS() { return m_text[m_nPos] == 0; }

    CharType &curChar() {
        return m_ch;
    }

    void operator ++() {
        m_nPos += m_ch.size();
        readCurChar();
    }

    int getPos() { return m_nPos; }
    void setPos(int nPos) {
        assert((uint32_t)nPos <= strlen(m_text));
        m_nPos = nPos;
        readCurChar();
    }

private:
    void readCurChar() {
        m_ch.clear();

        readUtf8Char(m_text + m_nPos, m_ch);
        //#else
        //        int n = m_nPos;
        //        m_ch += m_text[n];
        //        if (isCharComposed(m_text[n]))
        //        {
        //            n++;
        //            while (isCharComposed(m_text[n]))
        //            {
        //                m_ch += m_text[n];
        //                n++;
        //            }
        //        }
        //#endif
    }

protected:
    cstr_t                      m_text;
    int                         m_nPos;
    CharType                    m_ch;

};


template<class pixfmt>
inline bool glyphPixelHasInk(uint8_t *p) {
    if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
        return true;
    }
    return pixfmt::pix_width >= 4 && p[3] != 0;
}

template<class pixfmt>
void copyBitmpDataFromGraphBuffer_t(Glyph *pGlyph, pixfmt &bufGraph) {
    int nHeightBitmap = bufGraph.height();

    // Determine the right of Bitmap.
    int nRightBitmap = min(pGlyph->nWidth + nHeightBitmap / 2, (int)bufGraph.width());
    uint8_t *pRowSrc = bufGraph.pix_ptr(nRightBitmap - 1, 0);
    for (; nRightBitmap > 0; nRightBitmap--) {
        uint8_t *p = pRowSrc;
        int y = 0;
        for (; y < nHeightBitmap; y++) {
            if (glyphPixelHasInk<pixfmt>(p)) {
                break;
            }
            p += bufGraph.stride();
        }
        if (y != nHeightBitmap) {
            break;
        }
        pRowSrc -= pixfmt::pix_width;
    }
    assert(nRightBitmap <= min(pGlyph->nWidth + nHeightBitmap / 2, (int)bufGraph.width()));
    if (nRightBitmap == 0) {
        return;
    }

    // Determine the left of Bitmap.
    int nLeftBitmap = 0;
    pRowSrc = bufGraph.row_ptr(0);
    for (; nLeftBitmap < nRightBitmap; nLeftBitmap++) {
        uint8_t *p = pRowSrc;
        int y = 0;
        for (; y < nHeightBitmap; y++) {
            if (glyphPixelHasInk<pixfmt>(p)) {
                break;
            }
            p += bufGraph.stride();
        }
        if (y != nHeightBitmap) {
            break;
        }
        pRowSrc += pixfmt::pix_width;
    }

    // Determine the top of Bitmap.
    int nTopBitmap = 0;
    pRowSrc = bufGraph.pix_ptr(nLeftBitmap, 0);
    for (; nTopBitmap < nHeightBitmap; nTopBitmap++) {
        uint8_t *p = pRowSrc;
        int x = nLeftBitmap;
        for (; x < nRightBitmap; x++) {
            if (glyphPixelHasInk<pixfmt>(p)) {
                break;
            }
            p += pixfmt::pix_width;
        }
        if (x != nRightBitmap) {
            break;
        }
        pRowSrc += bufGraph.stride();
    }

    // Determine the bottom of Bitmap.
    int nBottomBitmap = nHeightBitmap - 1;
    pRowSrc = bufGraph.pix_ptr(nLeftBitmap, nBottomBitmap);
    for (; nBottomBitmap > nTopBitmap; nBottomBitmap--) {
        uint8_t *p = pRowSrc;
        int x = nLeftBitmap;
        for (; x < nRightBitmap; x++) {
            if (glyphPixelHasInk<pixfmt>(p)) {
                break;
            }
            p += pixfmt::pix_width;
        }
        if (x != nRightBitmap) {
            break;
        }
        pRowSrc -= bufGraph.stride();
    }

    pGlyph->leftOffset = nLeftBitmap;
    pGlyph->topOffset = nTopBitmap;
    pGlyph->widthBitmap = nRightBitmap - nLeftBitmap + 1;
    pGlyph->heightBitmap = nBottomBitmap - nTopBitmap + 1;

    pGlyph->bitmap = new uint8_t[pGlyph->widthBytes() * pGlyph->heightBitmap];

    pRowSrc = bufGraph.pix_ptr(pGlyph->leftOffset, pGlyph->topOffset);
    uint8_t *pRowDst = pGlyph->bitmap;
    for (int y = 0; y < pGlyph->heightBitmap; y++) {
        uint8_t *pDst = pRowDst;
        uint8_t *pSrc = pRowSrc;
        for (int x = 0; x < pGlyph->widthBitmap; x++) {
#ifdef _CLEAR_TYPE
            pDst[G_R] = pSrc[PixPosition::PIX_R];
            pDst[G_G] = pSrc[PixPosition::PIX_G];
            pDst[G_B] = pSrc[PixPosition::PIX_B];
#endif
            uint8_t gray = RGBToGray(pSrc[G_R], pSrc[G_G], pSrc[G_B]);
            if (pixfmt::pix_width >= 4) {
                gray = max(gray, pSrc[3]);
            }
            pDst[G_A] = gray;
            pDst += G_PIX_SIZE;
            pSrc += pixfmt::pix_width;
        }
        pRowSrc += bufGraph.stride();
        pRowDst += pGlyph->widthBytes();
    }
}

void copyBitmpDataFromGraphBuffer(Glyph *pGlyph, agg::pixfmt_rgb24 &bufGraph) {
    copyBitmpDataFromGraphBuffer_t(pGlyph, bufGraph);
}

void copyBitmpDataFromGraphBuffer(Glyph *pGlyph, agg::pixfmt_rgba32 &bufGraph) {
    copyBitmpDataFromGraphBuffer_t(pGlyph, bufGraph);
}

static void copyAndOutlineBuffRGBA32(uint8_t *pOut, uint8_t *pIn, int nWidth, int nHeight, int nMargin) {
    int nWidthBytesOut = (nWidth + nMargin * 2) * G_PIX_SIZE;
    int nWidthBytesIn = nWidth * G_PIX_SIZE;

    // outline
    uint8_t *pIRow = pIn;
    uint8_t *pORow = pOut + nMargin * nWidthBytesOut + nMargin * G_PIX_SIZE;
    for (int y = 0; y < nHeight; y++) {
        uint8_t *pI = pIRow, *pO = pORow;

        for (int x = 0; x < nWidth; x++) {
            for (int i = 0; i < 4; i++, pI++, pO++) {
                int c = *pI;
                if (c == 0) {
                    continue;
                }
                if (c > *(pO))                  *(pO) = c;
                if (c > *(pO + G_PIX_SIZE))     *(pO + G_PIX_SIZE) = c;
                if (c > *(pO - G_PIX_SIZE))     *(pO - G_PIX_SIZE) = c;
                if (c > *(pO + nWidthBytesOut)) *(pO + nWidthBytesOut) = c;
                if (c > *(pO - nWidthBytesOut)) *(pO - nWidthBytesOut) = c;
            }
        }
        pIRow += nWidthBytesIn;
        pORow += nWidthBytesOut;
    }
}

static void eraseBuffRGBA32(uint8_t *pOut, uint8_t *pIn, int nWidth, int nHeight, int nMargin) {
    int nWidthBytesOut = (nWidth + nMargin * 2) * G_PIX_SIZE;
    int nWidthBytesIn = nWidth * G_PIX_SIZE;

    uint8_t *pIRow = pIn;
    uint8_t *pORow = pOut + nMargin * nWidthBytesOut + nMargin * G_PIX_SIZE;
    for (int y = 0; y < nHeight; y++) {
        uint8_t *pI = pIRow, *pO = pORow;

        for (int x = 0; x < nWidth; x++) {
            if (pI[G_A] == 0xFF) {
                pO[G_A] = pO[G_R] = pO[G_G] = pO[G_B] = 0;
            }
            pI += 4;
            pO += 4;
        }
        pIRow += nWidthBytesIn;
        pORow += nWidthBytesOut;
    }
}

static void copyAndShadowBuffRGBA32(uint8_t *pOut, uint8_t *pIn, int nWidth, int nHeight, int nAlphaShadow, int nRangeShadow) {
    // copyToOutlined(pOut, pIn, nWidth, nHeight, nRangeShadow, 0);
    copyAndOutlineBuffRGBA32(pOut, pIn, nWidth, nHeight, nRangeShadow);

    agg::rendering_buffer buff(pOut, nWidth + nRangeShadow * 2, nHeight + nRangeShadow * 2, (nWidth + nRangeShadow * 2) * G_PIX_SIZE);
    agg::pixfmt_rgba32 pixf(buff);

    agg::stack_blur_rgba32(pixf, nRangeShadow, nRangeShadow);

    eraseBuffRGBA32(pOut, pIn, nWidth, nHeight, nRangeShadow);
}


void createOutlinedGlyph(Glyph *glyph, int marginOutlined, CRawBmpFont::ShadowMode shadowMode) {
    assert(glyph->bitmapOutlined == nullptr);

    glyph->marginOutlined = marginOutlined;

    int bytesBitmap = (glyph->heightBitmap + marginOutlined) * glyph->widthBytesOutlined();
    glyph->bitmapOutlined = new uint8_t[bytesBitmap];
    if (!glyph->bitmapOutlined) {
        return;
    }

    memset(glyph->bitmapOutlined, 0, bytesBitmap);

    if (shadowMode == CRawBmpFont::SM_SHADOW) {
        copyAndShadowBuffRGBA32(glyph->bitmapOutlined, glyph->bitmap, glyph->widthBitmap,
            glyph->heightBitmap, 255, marginOutlined / 2);
    } else {
        // Outline glyph
        copyAndOutlineBuffRGBA32(glyph->bitmapOutlined, glyph->bitmap, glyph->widthBitmap, glyph->heightBitmap, marginOutlined / 2);

        eraseBuffRGBA32(glyph->bitmapOutlined, glyph->bitmap, glyph->widthBitmap, glyph->heightBitmap, marginOutlined / 2);
    }
}

CRawBmpFont::CRawBmpFont() {
    m_prawGlyphSet = nullptr;
    m_glyphHeight = 0;

    m_overlayMode = OM_COLOR;
    m_imgPattern1 = nullptr;
    m_imgPattern2 = nullptr;
    m_nAlphaPattern1 = 255;
    m_nAlphaPattern2 = 255;

    m_shadowMode = SM_SHADOW;
}

CRawBmpFont::~CRawBmpFont() {
    destroy();
}

bool CRawBmpFont::create(const FontInfoEx &font, float scaleFactor) {
    if (m_prawGlyphSet) {
        if (m_fontInfo.isSame(font) && m_scaleFactor == scaleFactor) {
            return true;
        }

        m_prawGlyphSet->release();
    }

    m_fontInfo = font;
    m_scaleFactor = scaleFactor;

    auto scaledFont = font;
    // 非整数 DPI（如 1.25/1.5）用四舍五入，避免截断导致字号偏小发糊
    scaledFont.height = (int)lround(font.height * scaleFactor);
    if (scaledFont.height < 1) {
        scaledFont.height = 1;
    }

    m_glyphHeight = scaledFont.height + MARGIN_FONT * 2;

    m_prawGlyphSet = g_rawGlyphSetMgr.getGlyphSet(scaledFont);
    m_marginOutlined = m_glyphHeight / 8 * 2;
    // 必须保证 m_marginOutlined 是偶数，后面进行 outline/shadown 时不会出现问题.
    m_marginOutlined += m_marginOutlined % 2;
    if (m_marginOutlined <= 1) {
        m_marginOutlined = 2;
    }

    return m_prawGlyphSet != nullptr;
}

void CRawBmpFont::destroy() {
    if (m_prawGlyphSet) {
        m_prawGlyphSet->release();
        m_prawGlyphSet = nullptr;
    }
}

void CRawBmpFont::setScaleFactor(float scaleFactor) {
    assert(m_prawGlyphSet != nullptr);
    if (m_prawGlyphSet) {
        if (scaleFactor != m_scaleFactor) {
            create(m_fontInfo, scaleFactor);
        }
    }
}

int CRawBmpFont::getHeight() const {
    return m_fontInfo.height;
}

bool CRawBmpFont::textOut(CRawGraph *canvas, float x, float y, const CColor &clrText, cstr_t text, size_t len, bool bDrawAlphaChannel)
{
    return drawTextClip(canvas, x, y, canvas->width() - x, 0, clrText, text, len, bDrawAlphaChannel);
}

inline void blend_raw_font_clr(uint8_t *dst, uint8_t src, uint8_t clr) {
    dst[0] = (uint8_t)(((clr - dst[0]) * src + (dst[0] << 8)) >> 8);
}

class raw_glyph_get_alpha_255 {
public:
    inline uint8_t operator()(uint8_t alpha) {
        return alpha;
    }

};

class raw_glyph_get_alpha {
public:
    inline uint8_t operator()(uint8_t alpha) {
        return uint8_t(alpha * nGlyphAlpha / 255);
    }

    int                         nGlyphAlpha;

};

class raw_glyph_blend_alpha {
public:
    static inline void blend(uint8_t *dst, uint8_t src) {
        dst[3] = BlendAlpha(dst[3], src);
    }
};

class raw_glyph_blend_alpha_none {
public:
    static inline void blend(uint8_t *dst, uint8_t src) {
    }
};

template<class _RawFontAlphaBlender, class _RawGlyphGetAlpha>
void drawGlyphRGBA32Buff(agg::rendering_buffer &graph, int xDst, int yDst, int xDstEnd, int yDstEnd,
    agg::rendering_buffer &bufGlyph, int xSrc, int ySrc, _RawGlyphGetAlpha &getGlyphAlpha, const CColor &clrGlyph) {
    uint8_t *pRowDst = graph.row_ptr(yDst) + xDst * 4;
    uint8_t *pRowSrc = bufGlyph.row_ptr(ySrc) + xSrc * 4;
    int x, y;
    uint8_t clr[3];

    clr[PixPosition::PIX_B] = clrGlyph.b();
    clr[PixPosition::PIX_G] = clrGlyph.g();
    clr[PixPosition::PIX_R] = clrGlyph.r();

    for (y = yDst; y < yDstEnd; y++) {
        uint8_t *pDst = pRowDst;
        uint8_t *pSrc = pRowSrc;
        for (x = xDst; x < xDstEnd; x++) {
            if (pSrc[3] != 0) {
                blend_raw_font_clr(pDst, getGlyphAlpha(pSrc[0]), clr[0]);
                blend_raw_font_clr(pDst + 1, getGlyphAlpha(pSrc[1]), clr[1]);
                blend_raw_font_clr(pDst + 2, getGlyphAlpha(pSrc[2]), clr[2]);
                /*                    pDst[0] = (uint8_t)(((clr[0] - pDst[0]) * pSrc[0] + (pDst[0] << 8)) >> 8);
                 pDst[1] = (uint8_t)(((clr[1] - pDst[1]) * pSrc[1] + (pDst[1] << 8)) >> 8);
                 pDst[2] = (uint8_t)(((clr[2] - pDst[2]) * pSrc[2] + (pDst[2] << 8)) >> 8);*/
                _RawFontAlphaBlender::blend(pDst, getGlyphAlpha(pSrc[3]));
            }
            pDst += 4;
            pSrc += G_PIX_SIZE;
        }
        pRowDst += graph.stride();
        pRowSrc += bufGlyph.stride();
    }
}

// graph is 32 bpp, glyph is 32 bpp
void drawGlyphRGBA32(agg::rendering_buffer &graph, int xDst, int yDst, CRect &rcClip,
    bool bDrawOutlined, Glyph *pGlyph, int nAlphaGlyph, const CColor &clrGlyph, bool bDrawAlphaChannel) {
    assert(G_PIX_SIZE == 4);

    uint8_t clr[3];

    clr[PixPosition::PIX_B] = clrGlyph.b();
    clr[PixPosition::PIX_G] = clrGlyph.g();
    clr[PixPosition::PIX_R] = clrGlyph.r();

    int xSrc = 0, ySrc = 0;

    xDst -= MARGIN_FONT;
    yDst -= MARGIN_FONT;

    xDst += pGlyph->leftOffset;
    yDst += pGlyph->topOffset;
    int xDstEnd = xDst + pGlyph->widthBitmap;
    int yDstEnd = yDst + pGlyph->heightBitmap;

    // Set outlined glyph parameters
    agg::rendering_buffer bufGlyph;
    if (bDrawOutlined) {
        bufGlyph.attach(pGlyph->bitmapOutlined, pGlyph->widthOutlined(), pGlyph->heightOutlined(), pGlyph->widthBytesOutlined());
        xDst -= pGlyph->marginOutlined / 2;
        yDst -= pGlyph->marginOutlined / 2;
        xDstEnd += pGlyph->marginOutlined / 2;
        yDstEnd += pGlyph->marginOutlined / 2;
    } else {
        bufGlyph.attach(pGlyph->bitmap, pGlyph->widthBitmap, pGlyph->heightBitmap, pGlyph->widthBytes());
    }

    // Set clip area
    if (xDstEnd > rcClip.right) {
        xDstEnd = rcClip.right;
    }
    if (yDstEnd > rcClip.bottom) {
        yDstEnd = rcClip.bottom;
    }
    if (xDst < rcClip.left) {
        xSrc = rcClip.left - xDst;
        xDst = rcClip.left;
    }
    if (yDst < rcClip.top) {
        ySrc = rcClip.top - yDst;
        yDst = rcClip.top;
    }

    // Fill with color
    if (nAlphaGlyph == 255) {
        raw_glyph_get_alpha_255 getGlyphAlpha;
        if (bDrawAlphaChannel) {
            drawGlyphRGBA32Buff<raw_glyph_blend_alpha>(graph, xDst, yDst, xDstEnd, yDstEnd, bufGlyph, xSrc, ySrc, getGlyphAlpha, clrGlyph);
        } else {
            drawGlyphRGBA32Buff<raw_glyph_blend_alpha_none>(graph, xDst, yDst, xDstEnd, yDstEnd, bufGlyph, xSrc, ySrc, getGlyphAlpha, clrGlyph);
        }
    } else {
        raw_glyph_get_alpha getGlyphAlpha;
        getGlyphAlpha.nGlyphAlpha = nAlphaGlyph;
        if (bDrawAlphaChannel) {
            drawGlyphRGBA32Buff<raw_glyph_blend_alpha>(graph, xDst, yDst, xDstEnd, yDstEnd, bufGlyph, xSrc, ySrc, getGlyphAlpha, clrGlyph);
        } else {
            drawGlyphRGBA32Buff<raw_glyph_blend_alpha_none>(graph, xDst, yDst, xDstEnd, yDstEnd, bufGlyph, xSrc, ySrc, getGlyphAlpha, clrGlyph);
        }
    }
}

inline void getDrawGlyphClipPos(int &xDst, int &yDst, int &xDstEnd, int &yDstEnd, int &xSrc, int &ySrc, Glyph *pGlyph, CRect &rcClip) {
    xSrc = 0; ySrc = 0;

    xDst += pGlyph->leftOffset;
    yDst += pGlyph->topOffset;
    xDstEnd = xDst + pGlyph->widthBitmap;
    yDstEnd = yDst + pGlyph->heightBitmap;

    // Set clip area
    if (xDstEnd > rcClip.right) {
        xDstEnd = rcClip.right;
    }
    if (yDstEnd > rcClip.bottom) {
        yDstEnd = rcClip.bottom;
    }
    if (xDst < rcClip.left) {
        xSrc = rcClip.left - xDst;
        xDst = rcClip.left;
    }
    if (yDst < rcClip.top) {
        ySrc = rcClip.top - yDst;
        yDst = rcClip.top;
    }
}

// graph is 32 bpp, glyph is 32 bpp
// pattern is 24 bpp.
void drawGlyphRGBA32(agg::rendering_buffer &graph, int xDst, int yDst, CRect &rcClip,
    Glyph *pGlyph, int nAlphaGlyph, RawImageData *imgPattern)
{
    xDst -= MARGIN_FONT;
    yDst -= MARGIN_FONT;

    int xDstEnd, yDstEnd, xSrc, ySrc;
    getDrawGlyphClipPos(xDst, yDst, xDstEnd, yDstEnd, xSrc, ySrc, pGlyph, rcClip);

    assert(imgPattern->bitCount == 24);

    int xPtStart = xDst % imgPattern->width;
    int yPtStart = ySrc + pGlyph->topOffset;
    if (yPtStart >= imgPattern->height) {
        yPtStart = 0;
    }

    uint8_t *pRowPt = imgPattern->rowPtr(yPtStart);
    pRowPt += xPtStart * 3;

    agg::rendering_buffer bufGlyph(pGlyph->bitmap, pGlyph->widthBitmap, pGlyph->heightBitmap, pGlyph->widthBytes());

    uint8_t *pRowDst = graph.row_ptr(yDst) + xDst * 4;
    uint8_t *pRowSrc = bufGlyph.row_ptr(ySrc) + xSrc * 4;

    for (int y = yDst; y < yDstEnd; y++) {
        uint8_t *pDst = pRowDst;
        uint8_t *pSrc = pRowSrc;
        uint8_t *pPt = pRowPt;
        int xPtStartNew = xPtStart;
        for (int x = xDst; x < xDstEnd; x++) {
            if (pSrc[3] != 0) {
                int a;
                for (int i = 0; i < 3; i++) {
                    a = pSrc[i] * nAlphaGlyph / 255;
                    pDst[i] = (uint8_t)(((pPt[i] - pDst[i]) * a + (pDst[i] << 8)) >> 8);
                }
                a = pSrc[G_A] * nAlphaGlyph / 255;
                pDst[G_A] = (uint8_t)((a + pDst[G_A]) - ((a * pDst[G_A] + 0xFF) >> 8));
                //                 int        a = pSrc[0] * nAlphaGlyph / 255;
                //                 pDst[0] = (uint8_t)(((pPt[0] - pDst[0]) * a + (pDst[0] << 8)) >> 8);
                //                 pDst[1] = (uint8_t)(((pPt[1] - pDst[1]) * a + (pDst[1] << 8)) >> 8);
                //                 pDst[2] = (uint8_t)(((pPt[2] - pDst[2]) * a + (pDst[2] << 8)) >> 8);
                //                 pDst[3] = (uint8_t)((a + pDst[3]) - ((a * pDst[3] + 0xFF) >> 8));
            }
            pDst += 4;
            pSrc += G_PIX_SIZE;

            xPtStartNew++;
            if (xPtStartNew >= imgPattern->width) {
                xPtStartNew = 0;
                pPt = imgPattern->rowPtr(yPtStart);
            } else {
                pPt += 3;
            }
        }
        pRowDst += graph.stride();
        pRowSrc += bufGlyph.stride();
        yPtStart++;
        if (yPtStart >= imgPattern->height) {
            yPtStart = 0;
            pRowPt = imgPattern->rowPtr(0);
            pRowPt += xPtStart * 3;
        } else {
            pRowPt += imgPattern->stride;
        }
    }
}

// graph is 32 bpp, glyph is 32 bpp
// pattern is 24 bpp.
void drawGlyphRGBA32(agg::rendering_buffer &graph, int xDst, int yDst, CRect &rcClip,
    Glyph *pGlyph, int nAlphaGlyph, RawImageData *imgPattern, int nAlphaPattern,
    const CColor &clrPt, int nAlphaClrPt)
{
    uint8_t pclrPt[3];

    pclrPt[PixPosition::PIX_B] = clrPt.b();
    pclrPt[PixPosition::PIX_G] = clrPt.g();
    pclrPt[PixPosition::PIX_B] = clrPt.r();

    xDst -= MARGIN_FONT;
    yDst -= MARGIN_FONT;

    int xDstEnd, yDstEnd, xSrc, ySrc;
    getDrawGlyphClipPos(xDst, yDst, xDstEnd, yDstEnd, xSrc, ySrc, pGlyph, rcClip);

    assert(imgPattern->bitCount == 24);

    int xPtStart = xDst % imgPattern->width;
    int yPtStart = ySrc + pGlyph->topOffset;
    if (yPtStart >= imgPattern->height) {
        yPtStart = 0;
    }

    agg::rendering_buffer bufGlyph(pGlyph->bitmap, pGlyph->widthBitmap, pGlyph->heightBitmap, pGlyph->widthBytes());

    uint8_t *pRowDst = graph.row_ptr(yDst) + xDst * 4;
    uint8_t *pRowSrc = bufGlyph.row_ptr(ySrc) + xSrc * 4;
    uint8_t *pRowPt = imgPattern->rowPtr(yPtStart) + xPtStart * 3;

    for (int y = yDst; y < yDstEnd; y++) {
        uint8_t *pDst = pRowDst;
        uint8_t *pSrc = pRowSrc;
        uint8_t *pPt = pRowPt;
        int xPtStartNew = xPtStart;
        for (int x = xDst; x < xDstEnd; x++) {
            if (pSrc[3] != 0) {
                int a, c;
                for (int i = 0; i < 3; i++) {
                    a = pSrc[i] * nAlphaGlyph / 255;
                    c = (pPt[i] * nAlphaPattern + pclrPt[i] * (255 - nAlphaPattern)) / 255;
                    pDst[i] = (uint8_t)(((c - pDst[i]) * a + (pDst[i] << 8)) >> 8);
                }
                a = pSrc[3] * nAlphaGlyph / 255;
                pDst[3] = (uint8_t)((a + pDst[3]) - ((a * pDst[3] + 0xFF) >> 8));
            }
            pDst += 4;
            pSrc += G_PIX_SIZE;

            xPtStartNew++;
            if (xPtStartNew >= imgPattern->width) {
                xPtStartNew = 0;
                pPt = imgPattern->rowPtr(yPtStart);
            } else {
                pPt += 3;
            }
        }
        pRowDst += graph.stride();
        pRowSrc += bufGlyph.stride();
        yPtStart++;
        if (yPtStart >= imgPattern->height) {
            yPtStart = 0;
            pRowPt = imgPattern->rowPtr(0);
        } else {
            pRowPt += imgPattern->stride;
        }
    }
}

// graph is 32 bpp, glyph is 32 bpp
// pattern is 24 bpp.
void drawGlyphRGBA32(agg::rendering_buffer &graph, int xDst, int yDst, CRect &rcClip, Glyph *pGlyph, int nAlphaGlyph, RawImageData *imgPattern1, int nAlphaPt1, RawImageData *imgPattern2, int nAlphaPt2)
{
    xDst -= MARGIN_FONT;
    yDst -= MARGIN_FONT;

    int xDstEnd, yDstEnd, xSrc, ySrc;
    getDrawGlyphClipPos(xDst, yDst, xDstEnd, yDstEnd, xSrc, ySrc, pGlyph, rcClip);

    // pattern 1
    assert(imgPattern1->bitCount == 24);

    int xPt1Start = xDst % imgPattern1->width;
    int yPt1Start = ySrc + pGlyph->topOffset;
    if (yPt1Start >= imgPattern1->height) {
        yPt1Start = 0;
    }

    // pattern 2
    assert(imgPattern2->bitCount == 24);

    int xPt2Start = xDst % imgPattern2->width;
    int yPt2Start = ySrc + pGlyph->topOffset;
    if (yPt2Start >= imgPattern2->height) {
        yPt2Start = 0;
    }

    agg::rendering_buffer bufGlyph(pGlyph->bitmap, pGlyph->widthBitmap, pGlyph->heightBitmap, pGlyph->widthBytes());

    uint8_t *pRowDst = graph.row_ptr(yDst) + xDst * 4;
    uint8_t *pRowSrc = bufGlyph.row_ptr(ySrc) + xSrc * 4;
    uint8_t *pRowPt1 = imgPattern1->rowPtr(yPt1Start) + xPt1Start * 3;
    uint8_t *pRowPt2 = imgPattern2->rowPtr(yPt2Start) + xPt2Start * 3;

    for (int y = yDst; y < yDstEnd; y++) {
        uint8_t *pDst = pRowDst;
        uint8_t *pSrc = pRowSrc;
        uint8_t *pPt1 = pRowPt1;
        uint8_t *pPt2 = pRowPt2;
        int xPt1StartNew = xPt1Start;
        int xPt2StartNew = xPt2Start;
        for (int x = xDst; x < xDstEnd; x++) {
            if (pSrc[3] != 0) {
                int a, c;
                for (int i = 0; i < 3; i++) {
                    a = pSrc[i] * nAlphaGlyph / 255;
                    c = (pPt1[i] * nAlphaPt1 + pPt2[i] * nAlphaPt2) / 255;
                    pDst[i] = (uint8_t)(((c - pDst[i]) * a + (pDst[i] << 8)) >> 8);
                }
                a = pSrc[G_A] * nAlphaGlyph / 255;
                pDst[G_A] = (uint8_t)((a + pDst[G_A]) - ((a * pDst[G_A] + 0xFF) >> 8));
            }
            pDst += 4;
            pSrc += G_PIX_SIZE;

            xPt1StartNew++;
            if (xPt1StartNew >= imgPattern1->width) {
                xPt1StartNew = 0;
                pPt1 = imgPattern1->rowPtr(yPt1Start);
            } else {
                pPt1 += 3;
            }

            xPt2StartNew++;
            if (xPt2StartNew >= imgPattern2->width) {
                xPt2StartNew = 0;
                pPt2 = imgPattern2->rowPtr(yPt2Start);
            } else {
                pPt2 += 3;
            }
        }
        pRowDst += graph.stride();
        pRowSrc += bufGlyph.stride();

        yPt1Start++;
        if (yPt1Start >= imgPattern1->height) {
            yPt1Start = 0;
            pRowPt1 = imgPattern1->rowPtr(0);
        } else {
            pRowPt1 += imgPattern1->stride;
        }

        yPt2Start++;
        if (yPt2Start >= imgPattern2->height) {
            yPt2Start = 0;
            pRowPt2 = imgPattern2->rowPtr(0);
        } else {
            pRowPt2 += imgPattern2->stride;
        }
    }
}

bool CRawBmpFont::drawTextEx(CRawGraph *canvas, const CRect &rcPos, const CColor &clrText, cstr_t text, size_t len, uint32_t uFormat, bool bDrawAlphaChannel) {
    assert(m_prawGlyphSet);
    if (!m_prawGlyphSet) {
        return false;
    }

    if (!isFlagSet(uFormat, DT_SINGLELINE)) {
        // Multiple line
        VecStrings vLines;
        CRect rc = rcPos;

        splitToMultiLine(rcPos, text, len, uFormat, vLines);

        if (isFlagSet(uFormat, DT_VCENTER)) {
            int height = (int)vLines.size() * (getHeight() + 2) - 2;
            rc.top = (rcPos.top + rcPos.bottom - height) / 2;
        }

        uFormat &= ~DT_VCENTER;
        for (int i = 0; i < (int)vLines.size(); i++) {
            drawTextEx(canvas, rc, clrText, vLines[i].c_str(), (int)vLines[i].size(), uFormat | DT_SINGLELINE, bDrawAlphaChannel);
            rc.top += getHeight() + 2;
            if (rc.top >= rc.bottom) {
                break;
            }
        }

        return true;
    } else if (isFlagSet(uFormat, DT_END_ELLIPSIS)) {
        string str;
        uFormat &= ~DT_END_ELLIPSIS;

        if (shouldDrawTextEllipsis(text, len, rcPos.right - rcPos.left, str)) {
            return drawTextEx(canvas, rcPos, clrText, str.c_str(), (int)str.size(), uFormat, bDrawAlphaChannel);
        }
    }

    if (isFlagSet(uFormat, DT_PREFIX_TEXT)) {
        // 绘制文字时，绘制对应的下划线: A&bc&&d ==> Abc&d
        string str;
        float xPrefix, prefixWidth;

        uFormat &= ~DT_PREFIX_TEXT;

        if (shouldDrawTextPrefix(text, len, str, xPrefix, prefixWidth)) {
            drawTextEx(canvas, rcPos, clrText, str.c_str(), str.size(), uFormat, bDrawAlphaChannel);

            if (prefixWidth > 0) {
                float x, y, width, xLeftClipOffset;

                getDrawTextExPosition(str.c_str(), (int)str.size(), rcPos, uFormat, x, y, width, xLeftClipOffset);
                x += xPrefix - 1;
                y += getHeight() - 3;
                if (x > xLeftClipOffset) {
                    CRect rc(x, y + 1, x + prefixWidth, y + 2);
                    rc.intersect(rcPos);
                    canvas->fillRect(rc, clrText, BPM_CHANNEL_RGB | BPM_OP_COPY);
                }
            }

            return true;
        }
    }

    float x, y, width, xLeftClipOffset;

    getDrawTextExPosition(text, len, rcPos, uFormat, x, y, width, xLeftClipOffset);

    x -= 1;
    y -= 1;

    return drawTextClip(canvas, x, y, width, xLeftClipOffset, clrText, text, len, bDrawAlphaChannel);
}

bool CRawBmpFont::drawTextClip(CRawGraph *canvas, float x, float y, float width, float xLeftClipOffset, const CColor &clrText, cstr_t text, size_t len, bool bDrawAlphaChannel)
{
    uint8_t textAlpha = canvas->getOpacityPainting();
    RawImageData *pGraphRaw = canvas->getRawBuff();
    agg::rendering_buffer bufGraph(pGraphRaw->buff, pGraphRaw->width, pGraphRaw->height, pGraphRaw->stride);

    CRect rcClip;
    canvas->getClipBoundBox(rcClip);

    if (x + xLeftClipOffset > rcClip.right ||
        y >= rcClip.bottom || y + m_glyphHeight <= rcClip.top) {
        return true;
    }

    rcClip.left = max((float)rcClip.left, x + xLeftClipOffset - m_marginOutlined / 2);
    rcClip.right = min((float)rcClip.right, x + width + m_marginOutlined / 2);

    // 开始实际的绘制，转换到内存坐标
    canvas->mapAndScale(rcClip);
    x = canvas->mapAndScaleX(x);
    y = canvas->mapAndScaleX(y);

    RtlStringIterator strIterator(text);
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        if (x >= rcClip.right) {
            return true;
        }

        if (x + glyph->nWidth <= rcClip.left) {
            x += glyph->nWidth;
            continue;
        }

        if (m_overlayMode == OM_PATTERN) {
            drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get());
        } else if (m_overlayMode == OM_COLOR) {
            drawGlyphRGBA32(bufGraph, x, y, rcClip, false, glyph, textAlpha, clrText, bDrawAlphaChannel);
        } else if (m_overlayMode == OM_DUAL_PATTERN) {
            drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get(), m_nAlphaPattern1, m_imgPattern2.get(), m_nAlphaPattern2);
        } else if (m_overlayMode == OM_PATTERN_COLOR) {
            drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get(), m_nAlphaPattern1, m_clrPattern2, m_nAlphaPattern2);
        }

        x += glyph->nWidth;
    }

    return true;
}

bool CRawBmpFont::outlinedTextOut(CRawGraph *canvas, float x, float y, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, bool bDrawAlphaChannel)
{
    return outlinedDrawTextClip(canvas, x, y, canvas->width() - x, 0, clrText, clrBorder, text, len, bDrawAlphaChannel);
}


bool CRawBmpFont::outlinedDrawTextEx(CRawGraph *canvas, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, uint32_t uFormat, bool bDrawAlphaChannel)
{
    assert(m_prawGlyphSet);
    if (!m_prawGlyphSet) {
        return false;
    }

    if (isFlagSet(uFormat, DT_END_ELLIPSIS)) {
        string str;

        uFormat &= ~DT_END_ELLIPSIS;

        if (shouldDrawTextEllipsis(text, len, rcPos.right - rcPos.left, str)) {
            return outlinedDrawTextEx(canvas, rcPos, clrText, clrBorder, str.c_str(), (int)str.size(), uFormat, bDrawAlphaChannel);
        }
    } else if (isFlagSet(uFormat, DT_PREFIX_TEXT)) {
        string str;
        float xPrefix, prefixWidth;

        uFormat &= ~DT_PREFIX_TEXT;

        if (shouldDrawTextPrefix(text, len, str, xPrefix, prefixWidth)) {
            outlinedDrawTextEx(canvas, rcPos, clrText, clrBorder, str.c_str(), (int)str.size(), uFormat, bDrawAlphaChannel);

            if (prefixWidth > 0) {
                float x, y, width, xLeftClipOffset;

                getDrawTextExPosition(str.c_str(), (int)str.size(), rcPos, uFormat, x, y, width, xLeftClipOffset);
                x += xPrefix - 1;
                y += getHeight() - 3;
                if (x > xLeftClipOffset) {
                    CRect rc(x, y + 1, x + prefixWidth, y + 2);
                    canvas->fillRect(rc, clrText, BPM_CHANNEL_RGB | BPM_OP_COPY);
                }
            }

            return true;
        }
    }


    float x, y, width, xLeftClipOffset;

    getDrawTextExPosition(text, len, rcPos, uFormat, x, y, width, xLeftClipOffset);

    x -= 1;
    y -= 1;

    return outlinedDrawTextClip(canvas, x, y, width, xLeftClipOffset, clrText, clrBorder, text, len, bDrawAlphaChannel);
}

bool CRawBmpFont::getTextExtentPoint32(cstr_t text, size_t len, CSize *size) {
    if (!m_prawGlyphSet) {
        return false;
    }
    size->cx = getTextWidth(text, len);
    size->cy = getHeight();
    return true;
}

bool CRawBmpFont::outlinedDrawTextClip(CRawGraph *canvas, float x, float y, float width, float xLeftClipOffset, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, bool bDrawAlphaChannel)
{
    uint8_t textAlpha = canvas->getOpacityPainting();

    RawImageData *pGraphRaw = canvas->getRawBuff();
    agg::rendering_buffer bufGraph(pGraphRaw->buff, pGraphRaw->width, pGraphRaw->height, pGraphRaw->stride);

    CRect rcClip;
    canvas->getClipBoundBox(rcClip);

    if (x + xLeftClipOffset > rcClip.right ||
        y >= rcClip.bottom  || y + m_glyphHeight + m_marginOutlined / 2 <= rcClip.top) {
        return true;
    }

    rcClip.left = max((float)rcClip.left, x + xLeftClipOffset - m_marginOutlined / 2);
    rcClip.right = min((float)rcClip.right, x + width + m_marginOutlined / 2);

    // 开始实际的绘制，转换到内存坐标
    canvas->mapAndScale(rcClip);
    x = canvas->mapAndScaleX(x);
    y = canvas->mapAndScaleX(y);

    RtlStringIterator strIterator(text);
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        if (x >= rcClip.right) {
            return true;
        }

        if (x + glyph->nWidth <= rcClip.left) {
            x += glyph->nWidth;
            continue;
        }

        if (!glyph->bitmapOutlined && glyph->bitmap) {
            createOutlinedGlyph(glyph, m_marginOutlined, m_shadowMode);
        }

        if (glyph->bitmapOutlined) {
            //
            // Draw outlined border
            //
            drawGlyphRGBA32(bufGraph, x, y, rcClip, true, glyph, textAlpha, clrBorder, bDrawAlphaChannel);
        }

        if (glyph->bitmap) {
            //
            // Draw inner text
            //
            if (m_overlayMode == OM_PATTERN) {
                drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get());
            } else if (m_overlayMode == OM_COLOR) {
                drawGlyphRGBA32(bufGraph, x, y, rcClip, false, glyph, textAlpha, clrText, bDrawAlphaChannel);
            } else if (m_overlayMode == OM_DUAL_PATTERN) {
                drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get(), m_nAlphaPattern1, m_imgPattern2.get(), m_nAlphaPattern2);
            } else if (m_overlayMode == OM_PATTERN_COLOR) {
                drawGlyphRGBA32(bufGraph, x, y, rcClip, glyph, textAlpha, m_imgPattern1.get(), m_nAlphaPattern1, m_clrPattern2, m_nAlphaPattern2);
            }
        }

        x += glyph->nWidth;
    }
    return true;
}

void CRawBmpFont::setOverlayPattern(const RawImageDataPtr &imgPattern) {
    assert(imgPattern);
    m_imgPattern1 = imgPattern;
    m_overlayMode = OM_PATTERN;
    m_imgPattern2 = nullptr;
}

void CRawBmpFont::setOverlayPattern(const RawImageDataPtr &imgPattern1, const RawImageDataPtr &imgPattern2, int nAlphaPattern1, int nAlphaPattern2) {
    assert(imgPattern1);
    assert(imgPattern2);
    m_imgPattern1 = imgPattern1;
    m_imgPattern2 = imgPattern2;
    m_nAlphaPattern1 = nAlphaPattern1;
    m_nAlphaPattern2 = nAlphaPattern2;
    m_overlayMode = OM_DUAL_PATTERN;
}

void CRawBmpFont::setOverlayPattern(const RawImageDataPtr &imgPattern1, int nAlphaPattern1, const CColor &clrPattern2, int nAlphaPattern2) {
    assert(imgPattern1);
    m_imgPattern1 = imgPattern1;
    m_clrPattern2 = clrPattern2;
    m_nAlphaPattern1 = nAlphaPattern1;
    m_nAlphaPattern2 = nAlphaPattern2;
    m_overlayMode = OM_PATTERN_COLOR;
    m_imgPattern2 = nullptr;
}

void CRawBmpFont::useColorOverlay() {
    m_imgPattern1 = nullptr;
    m_imgPattern2 = nullptr;
    m_overlayMode = OM_COLOR;
}

bool isWordSplitChar(cstr_t szChar) {
    // 必须先按 unsigned char 取值：signed char 的 UTF-8 高字节（如中文）
    // 转成 uint32_t 会变成 0xFFFFFFEx，再传给 CRT isalpha 会在 Debug 下触发断言崩溃。
    const unsigned char c = static_cast<unsigned char>(*szChar);

    if (!(isAlpha(c) || isDigit(c) || c >= 127
        || c == '\'' || c == '\"' || c == '_' || c == '-')) {
        return true;
    }

    return false;
}

/**
 * 根据 @uFormat 的标志，获取 @text 绘制的位置，返回值在: @x, @y, @width, @xLeftClipOffset 中.
 *
 * - 所有的坐标都是逻辑坐标值.
 *
 * @xLeftClipOffset 是被剪裁的文字的起始值.
 */
void CRawBmpFont::getDrawTextExPosition(cstr_t text, size_t len, const CRect &rcPos,
        uint32_t uFormat, float &x, float &y, float &width, float &xLeftClipOffset)
{
    if (!m_prawGlyphSet) {
        return;
    }

    xLeftClipOffset = 0;

    if (isFlagSet(uFormat, DT_CENTER)) {
        // Align at center
        int nWidthText = getTextWidth(text, len);

        x = (rcPos.right + rcPos.left - nWidthText) / 2;
    } else if (isFlagSet(uFormat, DT_RIGHT)) {
        // Align at right
        int nWidthText = getTextWidth(text, len);

        x = rcPos.right - nWidthText;
    } else {
        x = rcPos.left;
    }

    if (isFlagSet(uFormat, DT_VCENTER)) {
        // Align at vertical center
        y = (rcPos.top + rcPos.bottom - getHeight()) / 2;
    } else if (isFlagSet(uFormat, DT_BOTTOM)) {
        // Align at bottom
        y = rcPos.bottom - getHeight();
    } else {
        y = rcPos.top;
    }

    if (x < rcPos.left) {
        xLeftClipOffset = rcPos.left - x;
    }
    width = rcPos.right - x;
}

/**
 * 将 @text 分割为多行进行绘制
 *
 * - 坐标值为逻辑坐标.
 */
void CRawBmpFont::splitToMultiLine(const CRect &rcPos, cstr_t text, size_t len, uint32_t uFormat, VecStrings &vLines) {
    int w = 0;
    int nWidthMax = rcPos.right - rcPos.left;

    RtlStringIterator strIterator(text);
    string str;
    int nBegin = 0, nLastWordSplitPos = 0;
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        if (isWordSplitChar(glyph->ch.c_str())) {
            nLastWordSplitPos = strIterator.getPos();
        }

        if (glyph->ch[0] == '\r' || glyph->ch[0] == '\n') {
            w = nWidthMax + 1;
        }
        w += glyph->nWidth / m_scaleFactor;
        if (w > nWidthMax) {
            int nEnd = strIterator.getPos();
            if (nBegin < nLastWordSplitPos) {
                nEnd = nLastWordSplitPos;
            } else if (nEnd <= nBegin) {
                nEnd++;
            }

            str.assign(text + nBegin, text + nEnd);
            vLines.push_back(str);

            while (text[nEnd] == ' ' || text[nEnd] == '\r' || text[nEnd] == '\n') {
                nEnd++;
            }

            w = 0;
            nBegin = nEnd;
            strIterator.setPos(nEnd - 1); // The for ++strIterator, will seek to next.
        }
    }

    if (nBegin < len) {
        str.clear();
        str.append(text + nBegin);
        vLines.push_back(str);
    }
}

/**
 * 如果绘制的文本 @text 长度超过 nWidthMax，则需要将 text 截断，增加 '...'
 *
 * - 坐标值为逻辑坐标.
 */
bool CRawBmpFont::shouldDrawTextEllipsis(cstr_t text, size_t len, float nWidthMax, string &strEllipsis) {
    int w = 0;

    RtlStringIterator strIterator(text);
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        w += glyph->nWidth / m_scaleFactor;
        if (w > nWidthMax) {
            break;
        }
    }

    if (strIterator.getPos() >= len) {
        return false;
    }

    len = strIterator.getPos();

    static string dotStr = ".";
    Glyph *glyph = m_prawGlyphSet->getGlyph(dotStr);
    if (!glyph) {
        return false;
    }
    int wMax = nWidthMax - glyph->nWidth * 3;

    w = 0;
    for (strIterator.setPos(0); strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        w += glyph->nWidth / m_scaleFactor;
        if (w > wMax) {
            break;
        }

        strEllipsis += glyph->ch;
    }
    strEllipsis += "...";

    return true;
}

/**
 * 在 windows 下的菜点项，使用 '&File' 的方式，需要将 '&F' 显示为带下划线的 'F'.
 * shouldDrawTextPrefix 用于判断是否有这样的转换，并且返回需要增加下划线的位置和长度.
 */
bool CRawBmpFont::shouldDrawTextPrefix(cstr_t text, size_t len, string &strPrfix, float &xPrefix, float &nWidthPrefix) {
    xPrefix = 0;
    nWidthPrefix = 0;

    RtlStringIterator strIterator(text);
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        if (glyph->ch[0] == '&') {
            // 找到了需要转换为 '_' 的位置
            strPrfix.assign(text, len);
            strPrfix.erase(strIterator.getPos(), 1);
            ++strIterator;
            if (strIterator.getPos() < len) {
                glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
                if (glyph) {
                    nWidthPrefix = glyph->nWidth / m_scaleFactor;
                    xPrefix /= m_scaleFactor;
                }
            }
            return true;
        }

        xPrefix += glyph->nWidth;
    }

    return false;
}


float CRawBmpFont::getTextWidth(cstr_t text, size_t len) {
    float w = 0;

    RtlStringIterator strIterator(text);
    for (; strIterator.getPos() < len; ++strIterator) {
        Glyph *glyph = m_prawGlyphSet->getGlyph(strIterator.curChar());
        if (!glyph) {
            continue;
        }

        w += glyph->nWidth;
    }

    return w / m_scaleFactor;
}
