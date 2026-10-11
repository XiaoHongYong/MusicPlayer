#pragma once

#if !defined(_RAW_BMP_FONT_H_)
#define _RAW_BMP_FONT_H_

#include "FontInfoEx.h"
#include "RawImage.h"

class CRawGraph;
class CRawImage;
class CRawGlyphSet;

enum {
    MARGIN_FONT                 = 1,
};

/**
 * 为了能够做出特效字体显示，实现将字体转存到内存中.
 *
 * * 调用的坐标都是 Skin View 上的，当写入到 RawGraph 的内存时需要转换到内存地址坐标.
 */
class CRawBmpFont {
public:
    enum OverlayMode {
        OM_COLOR,
        OM_PATTERN,
        OM_DUAL_PATTERN,
        OM_PATTERN_COLOR,
    };

    enum ShadowMode {
        SM_NONE,
        SM_OUTLINE,
        SM_SHADOW,
    };

public:
    CRawBmpFont();
    virtual ~CRawBmpFont();

    bool create(const FontInfoEx &font, float scaleFactor);
    void destroy();

    const FontInfoEx &getFontInfo() const { return m_fontInfo; }
    int getHeight() const;

    void setScaleFactor(float scaleFactor);

    bool textOut(CRawGraph *canvas, float x, float y, const CColor &clrText, cstr_t text, size_t len, bool bDrawAlphaChannel);
    bool drawTextClip(CRawGraph *canvas, float x, float y, float width, float xLeftClipOffset, const CColor &clrText, cstr_t text, size_t len, bool bDrawAlphaChannel);
    bool drawTextEx(CRawGraph *canvas, const CRect &rcPos, const CColor &clrText, cstr_t text, size_t len, uint32_t uFormat, bool bDrawAlphaChannel);

    bool outlinedTextOut(CRawGraph *canvas, float x, float y, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, bool bDrawAlphaChannel);
    bool outlinedDrawTextClip(CRawGraph *canvas, float x, float y, float width, float xLeftClipOffset, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, bool bDrawAlphaChannel);
    bool outlinedDrawTextEx(CRawGraph *canvas, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, cstr_t text, size_t len, uint32_t uFormat, bool bDrawAlphaChannel);

    bool getTextExtentPoint32(cstr_t text, size_t len, CSize *pSize);

    // void OutlineText
    void setClipBox(CRect &rc);

    void setOverlayPattern(const RawImageDataPtr &imgPattern);
    void setOverlayPattern(const RawImageDataPtr &imgPattern1, const RawImageDataPtr &imgPattern2, int nAlphaPattern1, int nAlphaPattern2);
    void setOverlayPattern(const RawImageDataPtr &imgPattern1, int nAlphaPattern1, const CColor &clrPattern2, int nAlphaPattern2);
    void useColorOverlay();

    void setShadowMode(ShadowMode shadowMode);

protected:
    inline void getDrawTextExPosition(cstr_t text, size_t len, const CRect &rcPos, uint32_t uFormat, float &x, float &y, float &width, float &xLeftClipOffset);

    void splitToMultiLine(const CRect &rcPos, cstr_t text, size_t len, uint32_t uFormat, VecStrings &vLines);

    bool shouldDrawTextEllipsis(cstr_t text, size_t len, float nWidthMax, string &strEllipsis);

    // See DT_PREFIX_TEXT declaration
    bool shouldDrawTextPrefix(cstr_t text, size_t len, string &strPrfix, float &nXPrefix, float &nWidthPrefix);

    // 获取文字的宽度，返回的值为逻辑宽度.
    float getTextWidth(cstr_t text, size_t len);

protected:
    CRawGlyphSet                *m_prawGlyphSet;
    FontInfoEx                  m_fontInfo;
    float                       m_scaleFactor;
    float                       m_glyphHeight;

    RawImageDataPtr             m_imgPattern1, m_imgPattern2;
    CColor                      m_clrPattern2;
    int                         m_nAlphaPattern1, m_nAlphaPattern2;
    OverlayMode                 m_overlayMode;
    ShadowMode                  m_shadowMode;
    int                         m_marginOutlined;

};

#endif // !defined(_RAW_BMP_FONT_H_)
