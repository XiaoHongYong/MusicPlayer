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
    m_emInkToBase = 0;
}

CRawGlyphBuilder::~CRawGlyphBuilder() {
    destroy();
}

void CRawGlyphBuilder::createMemGraph(int nWidthGraph) {
    m_fontMemGraph.destroy();
    // 烘焙缓冲加高：Core Text 墨迹常高于 FontHeight，先完整画出再压回字号契约
    const int bufferHeight = getDrawBufferHeight();
    m_fontMemGraph.create(nWidthGraph, bufferHeight, 0, MEM_GRAPH_BITS);

    // 翻成左上角原点、y 向下，字形才是正的。agg 行号仍是 CG 设备坐标（底边为 0，
    // 负 stride），与这里的用户 y 方向相反。fit 时要把 m_baselineY 换成 agg 行号，
    // 否则基线会落在墨迹上半部，中文下半截被裁掉。
    CGContextRef ctx = m_fontMemGraph.getHandle();
    CGContextTranslateCTM(ctx, 0, bufferHeight);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);

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
    m_baselineY = MARGIN_FONT + m_refAscent;
    m_emInkToBase = measureEmInkToBase();
}

int CRawGlyphBuilder::measureEmInkToBase() {
    if (!m_fontMemGraph.isValid()) {
        return 0;
    }

    RawImageData *rawData = m_fontMemGraph.getRawBuff();
    agg::rendering_buffer bufGraph(rawData->buff, rawData->width, rawData->height, rawData->stride);
    const int nHeight = m_fontMemGraph.height();
    uint8_t *row = bufGraph.row_ptr(0);
    for (int y = 0; y < nHeight; y++) {
        memset(row, 0, bufGraph.stride_abs());
        row += bufGraph.stride();
    }

    // 用一个满高汉字量「墨迹顶到基线」，后面所有字共用这一条基线。
    const char kEm[] = "花";
    CFStringRef string = CFStringCreateWithBytes(nullptr, (const UInt8 *)kEm, sizeof(kEm) - 1, kCFStringEncodingUTF8, false);
    if (!string) {
        return 0;
    }
    m_fontOthers.draw(m_fontMemGraph.getHandle(), string, MARGIN_FONT, m_baselineY);
    CFRelease(string);

    int first = -1;
    const int width = m_fontMemGraph.width();
    for (int y = 0; y < nHeight && first < 0; y++) {
        uint8_t *p = bufGraph.row_ptr(y);
        for (int x = 0; x < width; x++, p += 4) {
            if (p[0] | p[1] | p[2] | p[3]) {
                first = y;
                break;
            }
        }
    }
    if (first < 0) {
        return 0;
    }
    int baselineRow = nHeight - 1 - m_baselineY;
    int ink = baselineRow - first;
    return ink > 0 ? ink : 0;
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

    // 用户 y（顶向下）→ agg 行号（底边为 0）。见 createMemGraph 里的 CTM 翻转。
    int baselineRow = m_fontMemGraph.height() - 1 - m_baselineY;
    if (baselineRow < 0) {
        baselineRow = 0;
    }
    // 按「花」的墨迹高度上移基线，顶边贴住字号盒子，空位留在盒子里面。
    // 量不到时退回大约 1/8 字号，避免基线又贴死底边。
    int bottomPad = max(2, (m_fontInfo.height + 7) / 8);
    if (m_emInkToBase > 0) {
        bottomPad = m_fontInfo.height - m_emInkToBase - MARGIN_FONT;
        if (bottomPad < 0) {
            bottomPad = 0;
        }
    }
    fitGlyphBitmapToBaseline(glyph, m_fontInfo.height, baselineRow, m_refAscent, bottomPad, 0);

    return glyph;
}
