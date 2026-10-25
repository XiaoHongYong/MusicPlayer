#ifndef _RAW_GLYPH_BUILDER_H_
#define _RAW_GLYPH_BUILDER_H_

#pragma once

#include "GfxFont.h"


class Glyph;

class CRawGlyphBuilder {
public:
    CRawGlyphBuilder();
    virtual ~CRawGlyphBuilder();

    void init(const FontInfoEx &font);

    int getHeight() const;

    void destroy();

    Glyph *buildGlyph(string &ch);

protected:
    int getHeightBitmap();
    int getDrawBufferHeight();
    void createMemGraph(int nWidthGraph);
    // 「花」的墨迹顶到基线的行数。用来把满高的字顶边贴住字号盒子。
    int measureEmInkToBase();

protected:
    FontInfoEx                  m_fontInfo;
    GfxFont                     m_fontLatin;
    GfxFont                     m_fontOthers;
    CRawGraphData               m_fontMemGraph;
    int                         m_refAscent;    // 拉丁/中文 ascent 取大，作公共基线参考
    int                         m_baselineY;    // 烘焙缓冲中的公共基线 Y
    int                         m_emInkToBase;  // 满高汉字从墨迹顶到基线的行数，0 表示还没量到

};

#endif // _RAW_GLYPH_BUILDER_H_
