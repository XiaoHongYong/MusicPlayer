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

protected:
    FontInfoEx                  m_fontInfo;
    GfxFont                     m_fontLatin;
    GfxFont                     m_fontOthers;
    CRawGraphData               m_fontMemGraph;
    int                         m_refAscent;    // 拉丁/中文 ascent 取大，作公共基线参考
    int                         m_baselineY;    // 烘焙缓冲中的公共基线 Y

};

#endif // _RAW_GLYPH_BUILDER_H_
