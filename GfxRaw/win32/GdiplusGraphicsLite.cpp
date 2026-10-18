#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT	0x0500

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <ole2.h>


#include "GdiplusGraphicsLite.h"


using namespace Gdiplus;
using namespace DllExports;

ARGB makeARGB(IN uint8_t a,
    IN uint8_t r,
    IN uint8_t g,
    IN uint8_t b) {
    return (((ARGB) (b) <<  BLUE_SHIFT) |
        ((ARGB) (g) << GREEN_SHIFT) |
        ((ARGB) (r) <<   RED_SHIFT) |
        ((ARGB) (a) << ALPHA_SHIFT));
}

#define WINGDIPAPI          __stdcall

#define GDIPCONST           const

typedef Status(WINGDIPAPI* FUNGdipCreateFromHDC)(HDC hdc, GpGraphics** graphics);
typedef Status(WINGDIPAPI* FUNGdipCreateFontFromLogfontW) (HDC hdc, GDIPCONST LOGFONTW* logfont, GpFont** font);
typedef Status(WINGDIPAPI* FUNGdipCreateFontFromDC) (HDC hdc, GpFont** font);
typedef Status(WINGDIPAPI* FUNGdipCreateSolidFill) (ARGB color, GpSolidFill** brush);
typedef Status(WINGDIPAPI* FUNGdipStringFormatGetGenericTypographic) (GpStringFormat** format);
typedef Status(WINGDIPAPI* FUNGdipDeleteStringFormat) (GpStringFormat* format);
typedef Status(WINGDIPAPI* FUNGdipSetStringFormatFlags) (GpStringFormat* format, INT flags);
typedef Status(WINGDIPAPI* FUNGdipGetStringFormatFlags) (GDIPCONST GpStringFormat* format, INT* flags);
typedef Status(WINGDIPAPI* FUNGdipDeleteFont) (GpFont* font);
typedef Status(WINGDIPAPI* FUNGdipDeleteBrush) (GpBrush* brush);
typedef Status(WINGDIPAPI* FUNGdipDeleteGraphics) (GpGraphics* graphics);
typedef Status(WINGDIPAPI* FUNGdipDrawString) (GpGraphics* graphics, GDIPCONST WCHAR* string, INT length, GDIPCONST GpFont* font, GDIPCONST RectF* layoutRect, GDIPCONST GpStringFormat* stringFormat, GDIPCONST GpBrush* brush);
typedef Status(WINGDIPAPI* FUNGdipGetClipBoundsI) (GpGraphics* graphics, GpRect* rect);
typedef Status(WINGDIPAPI* FUNGdipSetClipRectI) (GpGraphics* graphics, INT x, INT y, INT width, INT height, CombineMode combineMode);
typedef Status(WINGDIPAPI* FUNGdipSetTextRenderingHint) (GpGraphics* graphics, TextRenderingHint mode);
typedef Status(WINGDIPAPI* FUNGdipMeasureString) (GpGraphics* graphics, GDIPCONST WCHAR* string, INT length, GDIPCONST GpFont* font, GDIPCONST RectF* layoutRect, GDIPCONST GpStringFormat* stringFormat, RectF* boundingBox, INT* codepointsFitted, INT* linesFilled);
typedef Status(WINGDIPAPI* FUNGdipSetStringFormatAlign) (GpStringFormat* format, StringAlignment align);
typedef Status(WINGDIPAPI* FUNGdipGetStringFormatAlign) (GDIPCONST GpStringFormat* format, StringAlignment* align);
typedef Status(WINGDIPAPI* FUNGdipSetStringFormatLineAlign) (GpStringFormat* format, StringAlignment align);
typedef Status(WINGDIPAPI* FUNGdipGetStringFormatLineAlign) (GDIPCONST GpStringFormat* format, StringAlignment* align);
typedef Status(WINGDIPAPI* FUNGdipSetStringFormatTrimming) (GpStringFormat* format, StringTrimming trimming);
typedef Status(WINAPI* FUNGdiplusStartup) (OUT ULONG_PTR* token, const GdiplusStartupInput* input, OUT GdiplusStartupOutput* output);
typedef VOID(WINAPI* FUNGdiplusShutdown) (ULONG_PTR token);

typedef Status(WINGDIPAPI* FUNGdipGetStringFormatTrimming) (GpStringFormat* format, StringTrimming* trimming);

static FUNGdipCreateFromHDC                     g_funGdipCreateFromHDC = nullptr;
static FUNGdipCreateFontFromLogfontW            g_funGdipCreateFontFromLogfontW = nullptr;
static FUNGdipCreateFontFromDC                  g_funGdipCreateFontFromDC = nullptr;
static FUNGdipCreateSolidFill                   g_funGdipCreateSolidFill = nullptr;
static FUNGdipStringFormatGetGenericTypographic g_funGdipStringFormatGetGenericTypographic = nullptr;
static FUNGdipDeleteStringFormat                g_funGdipDeleteStringFormat = nullptr;
static FUNGdipSetStringFormatFlags              g_funGdipSetStringFormatFlags = nullptr;
static FUNGdipGetStringFormatFlags              g_funGdipGetStringFormatFlags = nullptr;
static FUNGdipDeleteFont                        g_funGdipDeleteFont = nullptr;
static FUNGdipDeleteBrush                       g_funGdipDeleteBrush = nullptr;
static FUNGdipDeleteGraphics                    g_funGdipDeleteGraphics = nullptr;
static FUNGdipDrawString                        g_funGdipDrawString = nullptr;
static FUNGdipGetClipBoundsI                    g_funGdipGetClipBoundsI = nullptr;
static FUNGdipSetClipRectI                      g_funGdipSetClipRectI = nullptr;
static FUNGdipSetTextRenderingHint              g_funGdipSetTextRenderingHint = nullptr;
static FUNGdipMeasureString                     g_funGdipMeasureString = nullptr;
static FUNGdipSetStringFormatAlign              g_funGdipSetStringFormatAlign = nullptr;
static FUNGdipGetStringFormatAlign              g_funGdipGetStringFormatAlign = nullptr;
static FUNGdipSetStringFormatLineAlign          g_funGdipSetStringFormatLineAlign = nullptr;
static FUNGdipGetStringFormatLineAlign          g_funGdipGetStringFormatLineAlign = nullptr;
static FUNGdipSetStringFormatTrimming           g_funGdipSetStringFormatTrimming = nullptr;
static FUNGdipGetStringFormatTrimming           g_funGdipGetStringFormatTrimming = nullptr;
static FUNGdiplusStartup                        g_funGdiplusStartup = nullptr;
static FUNGdiplusShutdown                       g_funGdiplusShutdown = nullptr;


class CGdiplusFont {
public:
    CGdiplusFont() {
        m_font = nullptr;
    }
    ~CGdiplusFont() {
        destroy();
    }

    bool create(CGdiplusGraphicsLite *canvas, LOGFONTW &logFont) {
        Status ret;

        destroy();

        ret = g_funGdipCreateFontFromLogfontW(canvas->getHandle(), &logFont, &m_font);
        if (ret == NotTrueTypeFont) {
            // 非 TrueType 时依次尝试常见 UI 字体（含中文雅黑）
            static const wchar_t *kFallbackFaces[] = { L"Microsoft YaHei", L"Tahoma" };
            for (auto face : kFallbackFaces) {
                LOGFONTW logfontNew = logFont;
                wcscpy_s(logfontNew.lfFaceName, CountOf(logfontNew.lfFaceName), face);
                ret = g_funGdipCreateFontFromLogfontW(canvas->getHandle(), &logfontNew, &m_font);
                if (ret == Ok) {
                    break;
                }
            }
        }

        return ret == Ok;
    }

    void destroy() {
        if (m_font) {
            g_funGdipDeleteFont(m_font);
            m_font = nullptr;
        }
    }

    GpFont *getHandle() { return m_font; }

protected:
    GpFont                      *m_font;

};


#define g_funGdipCreateFontFromLogfont g_funGdipCreateFontFromLogfontW

#ifdef _WIN32
ULONG_PTR g_gdiplusToken = NULL;
#endif

CGdiplusGraphicsLite::CGdiplusGraphicsLite() {
}

CGdiplusGraphicsLite::~CGdiplusGraphicsLite() {
    if (stringFormat) {
        g_funGdipDeleteStringFormat(stringFormat);
    }
    if (brush) {
        g_funGdipDeleteBrush(brush);
    }
    if (griphics) {
        g_funGdipDeleteGraphics(griphics);
    }

    if (m_hdcBk) {
        DeleteDC(m_hdcBk);
    }
}

void CGdiplusGraphicsLite::attach(HDC hdc) {
    if (!g_funGdipCreateFromHDC) {
        //
        // 载入地址
        HMODULE hModule;

        hModule = loadGdiplusDll();
        if (!hModule) {
            DBG_LOG0("load Gdiplus Dll FAILED!");
            return;
        }

        g_funGdipCreateFromHDC = (FUNGdipCreateFromHDC )GetProcAddress(hModule, "GdipCreateFromHDC");
        g_funGdipCreateFontFromLogfontW = (FUNGdipCreateFontFromLogfontW )GetProcAddress(hModule, "GdipCreateFontFromLogfontW");
        g_funGdipCreateFontFromDC = (FUNGdipCreateFontFromDC )GetProcAddress(hModule, "GdipCreateFontFromDC");
        g_funGdipCreateSolidFill = (FUNGdipCreateSolidFill )GetProcAddress(hModule, "GdipCreateSolidFill");
        //        g_funGdipStringFormatGetGenericDefault = (FUNGdipStringFormatGetGenericDefault    )GetProcAddress(hModule, "GdipStringFormatGetGenericDefault");
        g_funGdipStringFormatGetGenericTypographic = (FUNGdipStringFormatGetGenericTypographic )GetProcAddress(hModule, "GdipStringFormatGetGenericTypographic");
        g_funGdipDeleteStringFormat = (FUNGdipDeleteStringFormat )GetProcAddress(hModule, "GdipDeleteStringFormat");
        g_funGdipSetStringFormatFlags = (FUNGdipSetStringFormatFlags )GetProcAddress(hModule, "GdipSetStringFormatFlags");
        g_funGdipGetStringFormatFlags = (FUNGdipGetStringFormatFlags )GetProcAddress(hModule, "GdipGetStringFormatFlags");
        g_funGdipDeleteFont = (FUNGdipDeleteFont )GetProcAddress(hModule, "GdipDeleteFont");
        g_funGdipDeleteBrush = (FUNGdipDeleteBrush )GetProcAddress(hModule, "GdipDeleteBrush");
        g_funGdipDeleteGraphics = (FUNGdipDeleteGraphics )GetProcAddress(hModule, "GdipDeleteGraphics");
        g_funGdipDrawString = (FUNGdipDrawString )GetProcAddress(hModule, "GdipDrawString");
        g_funGdipGetClipBoundsI = (FUNGdipGetClipBoundsI )GetProcAddress(hModule, "GdipGetClipBoundsI");
        g_funGdipSetClipRectI = (FUNGdipSetClipRectI )GetProcAddress(hModule, "GdipSetClipRectI");
        g_funGdipSetTextRenderingHint = (FUNGdipSetTextRenderingHint )GetProcAddress(hModule, "GdipSetTextRenderingHint");
        g_funGdipMeasureString = (FUNGdipMeasureString )GetProcAddress(hModule, "GdipMeasureString");
        g_funGdipSetStringFormatAlign = (FUNGdipSetStringFormatAlign )GetProcAddress(hModule, "GdipSetStringFormatAlign");
        g_funGdipGetStringFormatAlign = (FUNGdipGetStringFormatAlign )GetProcAddress(hModule, "GdipGetStringFormatAlign");
        g_funGdipSetStringFormatLineAlign = (FUNGdipSetStringFormatLineAlign )GetProcAddress(hModule, "GdipSetStringFormatLineAlign");
        g_funGdipGetStringFormatLineAlign = (FUNGdipGetStringFormatLineAlign )GetProcAddress(hModule, "GdipGetStringFormatLineAlign");
        g_funGdipSetStringFormatTrimming = (FUNGdipSetStringFormatTrimming )GetProcAddress(hModule, "GdipSetStringFormatTrimming");
        g_funGdipGetStringFormatTrimming = (FUNGdipGetStringFormatTrimming )GetProcAddress(hModule, "GdipGetStringFormatTrimming");
        if (!g_funGdipCreateFromHDC || !g_funGdipCreateFontFromLogfontW
            || !g_funGdipCreateFontFromDC || !g_funGdipCreateSolidFill
            || !g_funGdipStringFormatGetGenericTypographic || !g_funGdipDeleteStringFormat
            || !g_funGdipGetStringFormatFlags || !g_funGdipSetStringFormatFlags
            || !g_funGdipDeleteFont || !g_funGdipDeleteBrush
            || !g_funGdipDeleteGraphics || !g_funGdipSetTextRenderingHint
            || !g_funGdipSetTextRenderingHint || !g_funGdipDrawString
            || !g_funGdipGetClipBoundsI || !g_funGdipSetClipRectI
            || !g_funGdipSetTextRenderingHint || !g_funGdipMeasureString
            || !g_funGdipSetStringFormatAlign || !g_funGdipGetStringFormatAlign
            || !g_funGdipSetStringFormatLineAlign || !g_funGdipGetStringFormatLineAlign
            || !g_funGdipSetStringFormatTrimming || !g_funGdipGetStringFormatTrimming) {
            return;
        }
    }

    m_hdc = hdc;

    Status ret = g_funGdipCreateFromHDC(hdc, &griphics);
    if (ret != Ok) {
        DBG_LOG1("g_funGdipCreateFromHDC FAILED: %d!", ret);
        return;
    }
    ret = g_funGdipStringFormatGetGenericTypographic(&stringFormat);
    if (ret != Ok) {
        DBG_LOG1("g_funGdipStringFormatGetGenericTypographic FAILED: %d!", ret);
        return;
    }
    INT flag = 0;
    ret = g_funGdipGetStringFormatFlags(stringFormat, &flag);
    flag &= ~StringFormatFlagsNoClip;
    ret = g_funGdipSetStringFormatFlags(stringFormat, flag | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
    ret = g_funGdipSetStringFormatTrimming(stringFormat, StringTrimmingNone);
    //    ret = GdipCreateFontFromDC(hdc, &font);
    //    if (ret != Ok)
    //        return;
    //    clrText = GetTextColor(hdc);
    //    ret = GdipCreateSolidFill(makeARGB(255, getRValue(clrText), getGValue(clrText), getBValue(clrText)), &brush);
    //    if (ret != Ok)
    //        return;
    bInitOK = true;
}

bool CGdiplusGraphicsLite::textOut(int x, int y, cwstr_t szText, int nLen) {
    RectF rect((float)x, (float)y, (float)2000, (float)300);

    if (!bInitOK) {
        return false;
    }

    Status ret = g_funGdipSetTextRenderingHint(griphics, TextRenderingHintAntiAlias);
    if (ret != Ok) {
        DBG_LOG1("g_funGdipSetTextRenderingHint FAILED: %d!", ret);
        return false;
    }

    ret = g_funGdipDrawString(griphics, szText, nLen, m_font->getHandle(), &rect, stringFormat, brush);
    //     if (ret != Ok)
    //     {
    //         DBG_LOG1("g_funGdipDrawString FAILED: %d!", ret);
    //         return false;
    //     }

    return true;
}

bool CGdiplusGraphicsLite::getTextExtentPoint32(cwstr_t szText, int nLen, CSize *pSize) {
    //    RectF        rcLayout(0, 0, 2000, 300);
    //    RectF        rcBoundBox;
    if (!bInitOK) {
        pSize->cx = 0;
        pSize->cy = 0;
        return false;
    }

    RectF rcLayout(0.0f, 0.0f, 0.0f, 0.0f);
    RectF rcBoundBox;

    if (g_funGdipMeasureString(griphics, szText, nLen, m_font->getHandle(), &rcLayout, stringFormat, &rcBoundBox, nullptr, nullptr) != Ok) {
        return false;
    }

    pSize->cx = (INT)rcBoundBox.Width;
    pSize->cy = (INT)rcBoundBox.Height;

    return true;
}

void CGdiplusGraphicsLite::setTextColor(const CColor &color) {
    Status ret;

    if (brush) {
        g_funGdipDeleteBrush(brush);
        brush = nullptr;
    }

    ret = g_funGdipCreateSolidFill(makeARGB(255, GetRValue(color.get()), GetGValue(color.get()), GetBValue(color.get())), &brush);
    if (ret != Ok) {
        DBG_LOG1("g_funGdipCreateSolidFill FAILED: %d!", ret);
        bInitOK = false;
    }
}

void CGdiplusGraphicsLite::setBkMode(bool bTransparent) {
    ::SetBkMode(m_hdc, bTransparent ? TRANSPARENT : OPAQUE);
}

void CGdiplusGraphicsLite::setBgColor(const CColor &color) {
    ::SetBkColor(m_hdc, color.get());
}

bool CGdiplusGraphicsLite::canSupportGdiplus() {
    HMODULE hModule;

    hModule = loadGdiplusDll();
    if (!hModule) {
        return false;
    }

    FreeLibrary(hModule);
    return true;
}

HMODULE CGdiplusGraphicsLite::loadGdiplusDll() {
    HMODULE hModule;

    hModule = LoadLibraryA("Gdiplus.dll");
    if (!hModule) {
        string fn = getAppResourceFile("Gdiplus.dll");
        hModule = LoadLibraryA(fn.c_str());
    }
    return hModule;
}

bool CGdiplusGraphicsLite::startup() {
    // init gdiplus
    OleInitialize(nullptr);

    GdiplusStartupInput gdiplusStartupInput;

    if (!g_funGdiplusStartup || !g_funGdiplusShutdown) {
        HMODULE hModule;
        hModule = loadGdiplusDll();
        if (!hModule) {
            return false;
        }
        g_funGdiplusStartup = (FUNGdiplusStartup)GetProcAddress(hModule, "GdiplusStartup");
        g_funGdiplusShutdown = (FUNGdiplusShutdown)GetProcAddress(hModule, "GdiplusShutdown");
    }

    // Initialize GDI+.
    if (g_funGdiplusStartup && g_gdiplusToken == NULL) {
        return g_funGdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) == Ok;
    }
    return false;
}

void CGdiplusGraphicsLite::shutdown() {
    if (g_funGdiplusShutdown) {
        g_funGdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = NULL;
    }

    OleUninitialize();
}

CGdiplusFontPtr CGdiplusGraphicsLite::createFont(FontInfoEx &font, bool isLatin) {
    LOGFONTW logFont = { 0 };
    logFont.lfHeight = font.getSize();
    logFont.lfWeight = font.getWeight();
    logFont.lfItalic = font.getItalic();
    logFont.lfUnderline = font.isUnderline;

    // FontName 管拉丁字符，FontNameOthers 管中文等。Others 为空时若落到系统默认，
    // 中文 Windows 常选宋体，小字号标题栏/菜单会发糊；改用微软雅黑。
    cstr_t faceName = isLatin ? font.getName() : font.getNameOthers();
    if (isEmptyString(faceName)) {
        faceName = isLatin ? "Tahoma" : "Microsoft YaHei";
    }
    wcscpy_s(logFont.lfFaceName, CountOf(logFont.lfFaceName), utf8ToUCS2(faceName).c_str());

    auto f = std::make_shared<CGdiplusFont>();
    f->create(this, logFont);

    return f;
}
