
#ifndef GfxRaw_win32_GdiplusGraphicsLite_h
#define GfxRaw_win32_GdiplusGraphicsLite_h

#pragma once

#include "FontCollection.h"


#ifndef __int3264
typedef uint32_t * ULONG_PTR;
#endif

namespace Gdiplus
{
    class GpFont;
    class GpGraphics;
    class GpSolidFill;
    class GpStringFormat;
};
using namespace Gdiplus;


class CGdiplusFont;
using CGdiplusFontPtr = std::shared_ptr<CGdiplusFont>;

// 不在头文件暴露 LOGFONT/windows.h，避免 winsock.h 与 winsock2 冲突
int gdiplusFontAscent(const CGdiplusFontPtr &font);
int gdiplusFontDescent(const CGdiplusFontPtr &font);

//
// GDI Plus方式的画图
//
class CGdiplusGraphicsLite {
public:
    // typedef bool (*FUNTextOut)(int nXStart, int nYStart, cstr_t lpString, int cbString);
    CGdiplusGraphicsLite();
    ~CGdiplusGraphicsLite();

    void attach(HDC hdc);
    HDC getHandle() const { return m_hdc; }

    // layoutHeight>0 时按该高度布局并裁剪（字形烘焙应对齐 FontHeight）
    bool textOut(int x, int y, cwstr_t szText, int nLen, int layoutHeight = 0);

    bool getTextExtentPoint32(cwstr_t szText, int nLen, CSize *pSize);

    void setTextColor(const CColor &color);

    void setBkMode(bool bTransparent);
    void setBgColor(const CColor &color);

    void setFont(const CGdiplusFontPtr &font) { m_font = font; }

    CGdiplusFontPtr createFont(FontInfoEx &font, bool isLatin);

public:
    static bool canSupportGdiplus();
    static bool startup();
    static void shutdown();

protected:
    static HMODULE loadGdiplusDll();

public:
    GpGraphics                      *griphics = NULL;
    GpSolidFill                     *brush = NULL;
    GpStringFormat                  *stringFormat = NULL;
    bool                            bInitOK = NULL;
    HDC                             m_hdc = NULL;
    HDC                             m_hdcBk = NULL;
    HPEN                            m_hPenOld = NULL;
    CGdiplusFontPtr                 m_font;

};

#endif // !defined(GfxRaw_win32_GdiplusGraphicsLite_h)
