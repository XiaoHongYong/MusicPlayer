#include "../third-parties/Agg/include/agg_scanline_p.h"
#include "../third-parties/Agg/include/agg_renderer_scanline.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgba.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgb.h"
#include "../third-parties/Agg/include/agg_rasterizer_scanline_aa.h"
#include "GdiplusGraphicsLite.h"
#include "RawGlyphBuilder.h"
#include "../RawBmpFont.h"
#include "../RawGlyphSet.hpp"


#define MEM_GRAPH_BITS      24

// Windows：布局字号不变，烘焙字号减 1，使渲染结果上方留 1px
static const int kBakeHeightInset = 1;

CRawGlyphBuilder::CRawGlyphBuilder() {
    m_refAscent = 0;
    m_baselineY = 0;
}

CRawGlyphBuilder::~CRawGlyphBuilder() {
    destroy();
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

void CRawGlyphBuilder::createMemGraph(int nWidthGraph) {
    m_fontMemGraph.destroy();
    // 烘焙缓冲加高：GDI+ 中文墨迹常高于 FontHeight，先完整画出再压回字号契约
    m_fontMemGraph.create(nWidthGraph, getDrawBufferHeight(), NULL, MEM_GRAPH_BITS);

    m_fontGraph.attach(m_fontMemGraph.getHandle());

    m_fontGraph.setTextColor(CColor(RGB(255, 255, 255)));
    m_fontGraph.setBgColor(CColor(RGB(0, 0, 0)));

    // 布局仍用 m_fontInfo.height；实际 GDI 字体小 1px，上方留出 1px
    FontInfoEx bakeFont = m_fontInfo;
    if (bakeFont.height > kBakeHeightInset) {
        bakeFont.height -= kBakeHeightInset;
    }

    m_fontLatin = m_fontGraph.createFont(bakeFont, true);
    m_fontOthers = m_fontGraph.createFont(bakeFont, false);

    int ascentL = gdiplusFontAscent(m_fontLatin);
    int ascentO = gdiplusFontAscent(m_fontOthers);
    if (ascentL < 1) {
        ascentL = bakeFont.height * 4 / 5;
    }
    if (ascentO < 1) {
        ascentO = ascentL;
    }
    m_refAscent = max(ascentL, ascentO);
    if (m_refAscent < 1) {
        m_refAscent = max(1, bakeFont.height * 4 / 5);
    }
    // 基线下移 1px，配合小字号使墨迹从第 1 行开始（上方留空）
    m_baselineY = MARGIN_FONT + 1 + m_refAscent;
}

void CRawGlyphBuilder::destroy() {
    m_fontMemGraph.destroy();
}

int CRawGlyphBuilder::getDrawBufferHeight() {
    return m_fontInfo.height * 2 + MARGIN_FONT * 2;
}

int CRawGlyphBuilder::getHeightBitmap() {
    return m_fontInfo.height + MARGIN_FONT * 2;
}

void copyBitmpDataFromGraphBuffer(Glyph *pGlyph, agg::pixfmt_rgb24 &bufGraph);

Glyph *CRawGlyphBuilder::buildGlyph(string &ch) {
    utf16string ucs2Ch;
    utf8ToUCS2(ch.c_str(), ch.size(), ucs2Ch);

    const bool isLatin = isAnsiStr(ch.c_str());
    CGdiplusFontPtr font = isLatin ? m_fontLatin : m_fontOthers;
    m_fontGraph.setFont(font);

    CSize size;
    if (!m_fontGraph.getTextExtentPoint32(ucs2Ch.c_str(), ucs2Ch.size(), &size)) {
        return nullptr;
    }

    int neededW = size.cx + MARGIN_FONT * 2;
    if (neededW < getDrawBufferHeight() * 2) {
        neededW = getDrawBufferHeight() * 2;
    }
    if (neededW > m_fontMemGraph.width() || getDrawBufferHeight() > m_fontMemGraph.height()) {
        createMemGraph(neededW);
        font = isLatin ? m_fontLatin : m_fontOthers;
        m_fontGraph.setFont(font);
    }

    RawImageData *rawBuff = m_fontMemGraph.getRawBuff();
    agg::rendering_buffer bufGraph(rawBuff->buff, rawBuff->width, rawBuff->height, rawBuff->stride);

    int nHeightBitmap = m_fontMemGraph.height();

    uint8_t *pRowSrc = bufGraph.row_ptr(0);
    for (int y = 0; y < nHeightBitmap; y++) {
        memset(pRowSrc, 0, bufGraph.stride_abs());
        pRowSrc += bufGraph.stride();
    }

    // 按字体 ascent 回推绘制原点，使各字体基线都落在 m_baselineY
    int ascent = gdiplusFontAscent(font);
    if (ascent < 1) {
        ascent = m_refAscent;
    }
    int drawY = m_baselineY - ascent;
    if (drawY < 0) {
        drawY = 0;
    }

    if (!m_fontGraph.textOut(MARGIN_FONT, drawY, ucs2Ch.c_str(), (int)ucs2Ch.size())) {
        return nullptr;
    }

    Glyph *glyph = new Glyph;
    glyph->ch = ch;
    glyph->nWidth = (uint8_t)size.cx + 1; // 增加一个像素的字体间距.

    agg::pixfmt_rgb24 pixf(bufGraph);
    copyBitmpDataFromGraphBuffer(glyph, pixf);

    fitGlyphBitmapToBaseline(glyph, m_fontInfo.height, m_baselineY, m_refAscent);

    return glyph;
}
