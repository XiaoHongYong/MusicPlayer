#include "../WindowLib.h"
#include <cmath>

#ifndef MDT_EFFECTIVE_DPI
#define MDT_EFFECTIVE_DPI 0
#endif

using PFN_GetDpiForMonitor = HRESULT (WINAPI *)(HMONITOR, int, UINT *, UINT *);
using PFN_GetDpiForSystem = UINT (WINAPI *)();

static float scaleFactorAtPoint(int x, int y) {
    static PFN_GetDpiForMonitor getDpiForMonitor = nullptr;
    static PFN_GetDpiForSystem getDpiForSystem = nullptr;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        HMODULE shcore = LoadLibraryW(L"Shcore.dll");
        if (shcore) {
            getDpiForMonitor = (PFN_GetDpiForMonitor)GetProcAddress(shcore, "GetDpiForMonitor");
        }
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            getDpiForSystem = (PFN_GetDpiForSystem)GetProcAddress(user32, "GetDpiForSystem");
        }
    }

    UINT dpi = 96;
    if (getDpiForMonitor) {
        POINT pt = { x, y };
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0, dpiY = 0;
        if (mon && SUCCEEDED(getDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX > 0) {
            dpi = dpiX;
        }
    } else if (getDpiForSystem) {
        dpi = getDpiForSystem();
    } else {
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(nullptr, hdc);
        }
    }
    if (dpi < 72) {
        dpi = 96;
    }
    return dpi / 96.0f;
}

static int toLogicalCoord(int physical, float scale) {
    if (scale <= 0.f) {
        return physical;
    }
    return (int)lround(physical / scale);
}

void openUrl(Window *pWnd, cstr_t szUrl) {
    ShellExecuteW(pWnd->getWndHandle(), L"open", utf8ToUCS2(szUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool isModifierKeyPressed(int nKey, uint32_t nFlags) {
    assert(nKey == MK_SHIFT || nKey == MK_CONTROL || nKey == MK_ALT);
    if (nKey == MK_SHIFT) {
        nKey = VK_SHIFT;
    } else if (nKey == MK_CONTROL) {
        nKey = VK_CONTROL;
    } else if (nKey == MK_ALT) {
        nKey = VK_MENU;
    }

    return (GetKeyState(nKey) & 0x8000) == 0x8000;
}

CPoint getCursorPos() {
    CPoint pt;
    ::GetCursorPos(&pt);
    const float scale = scaleFactorAtPoint(pt.x, pt.y);
    return { toLogicalCoord(pt.x, scale), toLogicalCoord(pt.y, scale) };
}

bool getMonitorRestrictRect(const CRect &rcIn, CRect &rcRestrict) {
    // rcIn 为逻辑坐标；先按近似 DPI 还原物理矩形以定位显示器
    const float approxScale = scaleFactorAtPoint(0, 0);
    RECT rcPhys = {
        (LONG)lround(rcIn.left * approxScale),
        (LONG)lround(rcIn.top * approxScale),
        (LONG)lround(rcIn.right * approxScale),
        (LONG)lround(rcIn.bottom * approxScale),
    };

    HMONITOR mon = MonitorFromRect(&rcPhys, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!mon || !GetMonitorInfo(mon, &mi)) {
        const HWND hDesktop = GetDesktopWindow();
        RECT desktop;
        GetWindowRect(hDesktop, &desktop);
        const float s = scaleFactorAtPoint(desktop.left, desktop.top);
        rcRestrict.setLTRB(
            toLogicalCoord(desktop.left, s),
            toLogicalCoord(desktop.top, s),
            toLogicalCoord(desktop.right, s),
            toLogicalCoord(desktop.bottom, s));
        return true;
    }

    const float s = scaleFactorAtPoint(mi.rcWork.left, mi.rcWork.top);
    rcRestrict.setLTRB(
        toLogicalCoord(mi.rcWork.left, s),
        toLogicalCoord(mi.rcWork.top, s),
        toLogicalCoord(mi.rcWork.right, s),
        toLogicalCoord(mi.rcWork.bottom, s));

    return true;
}

bool copyTextToClipboard(cstr_t text) {
    if (!OpenClipboard(NULL)) {
        return false;
    }

    auto len = strlen(text);
    auto data = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(char));
    if (data == NULL) {
        CloseClipboard();
        return false;
    }

    auto buf = (char *)GlobalLock(data);
    memcpy(buf, text, len * sizeof(char));
    buf[len] = '\0';
    GlobalUnlock(data);

    SetClipboardData(CF_TEXT, data);

    return true;
}

bool getClipBoardText(string &str) {
    if (!IsClipboardFormatAvailable(CF_TEXT)) {
        return false;
    }

    if (!OpenClipboard(NULL)) {
        return false;
    }

    bool succeeded = false;
    auto data = GetClipboardData(CF_TEXT);
    if (data != NULL) {
        auto text = (char *)GlobalLock(data);
        if (text != NULL) {
            str.assign(text);
            GlobalUnlock(data);
            succeeded = true;
        }
    }
    CloseClipboard();

    return succeeded;
}
