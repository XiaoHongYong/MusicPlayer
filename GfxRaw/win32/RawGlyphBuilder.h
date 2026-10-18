#ifndef _RAW_GLYPH_BUILDER_H_
#define _RAW_GLYPH_BUILDER_H_

#pragma once

#include "GdiplusGraphicsLite.h"
#include "RawGraphData.h"


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
    CGdiplusGraphicsLite        m_fontGraph;
    CRawGraphData               m_fontMemGraph;

    CGdiplusFontPtr             m_fontLatin;
    CGdiplusFontPtr             m_fontOthers;
    int                         m_refAscent;    // 拉丁/中文 ascent 取大，作公共基线参考
    int                         m_baselineY;    // 烘焙缓冲中的公共基线 Y

};

#endif // _RAW_GLYPH_BUILDER_H_
