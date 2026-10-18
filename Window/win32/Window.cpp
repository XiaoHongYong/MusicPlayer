

#include "../WindowTypes.h"
#include "Window.h"
#include "../GfxRaw/RawGraph.h"
#include "../Desktop.h"

#include <dwmapi.h>
#include <cmath>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

#ifndef MDT_EFFECTIVE_DPI
#define MDT_EFFECTIVE_DPI 0
#endif

using PFN_SetProcessDpiAwarenessContext = BOOL (WINAPI *)(DPI_AWARENESS_CONTEXT);
using PFN_SetProcessDpiAwareness = HRESULT (WINAPI *)(int);
using PFN_GetDpiForWindow = UINT (WINAPI *)(HWND);
using PFN_GetDpiForSystem = UINT (WINAPI *)();
using PFN_GetDpiForMonitor = HRESULT (WINAPI *)(HMONITOR, int, UINT *, UINT *);

static PFN_GetDpiForWindow g_GetDpiForWindow = nullptr;
static PFN_GetDpiForSystem g_GetDpiForSystem = nullptr;
static PFN_GetDpiForMonitor g_GetDpiForMonitor = nullptr;
static bool g_dpiApisLoaded = false;

static void loadDpiApis() {
    if (g_dpiApisLoaded) {
        return;
    }
    g_dpiApisLoaded = true;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        g_GetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(user32, "GetDpiForWindow");
        g_GetDpiForSystem = (PFN_GetDpiForSystem)GetProcAddress(user32, "GetDpiForSystem");
    }
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        g_GetDpiForMonitor = (PFN_GetDpiForMonitor)GetProcAddress(shcore, "GetDpiForMonitor");
    }
}

static UINT querySystemDpi() {
    loadDpiApis();
    if (g_GetDpiForSystem) {
        UINT dpi = g_GetDpiForSystem();
        if (dpi > 0) {
            return dpi;
        }
    }
    HDC hdc = GetDC(nullptr);
    UINT dpi = hdc ? (UINT)GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) {
        ReleaseDC(nullptr, hdc);
    }
    return dpi > 0 ? dpi : 96;
}

static UINT queryDpiForPoint(int x, int y) {
    loadDpiApis();
    if (g_GetDpiForMonitor) {
        POINT pt = { x, y };
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0, dpiY = 0;
        if (mon && SUCCEEDED(g_GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX > 0) {
            return dpiX;
        }
    }
    return querySystemDpi();
}

void enableProcessDpiAwareness() {
    loadDpiApis();

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto setCtx = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }

    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        // PROCESS_PER_MONITOR_DPI_AWARE = 2
        auto setAwareness = (PFN_SetProcessDpiAwareness)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness) {
            setAwareness(2);
            return;
        }
    }

    // Vista+；低 _WIN32_WINNT 下无声明，运行时解析
    if (user32) {
        using PFN_SetProcessDPIAware = BOOL (WINAPI *)();
        auto setDpiAware = (PFN_SetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
        if (setDpiAware) {
            setDpiAware();
        }
    }
}

void showInFinder(cstr_t filename) {
    executeCmd(("explorer.exe /select, " + std::string(filename)).c_str());
}

void showInFinder(const VecStrings &files) {
    SetStrings openedPaths;

    for (auto &fn : files) {
        auto path = fileGetPath(fn.c_str());
        if (openedPaths.find(path) == openedPaths.end()) {
            openedPaths.insert(path);
            executeCmd(("explorer.exe /select, " + fn).c_str());
        }
    }
}

float getScreenScaleFactor() {
    return querySystemDpi() / 96.0f;
}

#define BASEWNDCLASSNAME    "Window"

#define WM_MOUSEWHEEL       0x020A

LRESULT CALLBACK baseWndProc(HWND hWnd, uint32_t uMsg, WPARAM wParam, LPARAM lParam) {
    // fetch out the Window *
    if (uMsg == WM_CREATE || uMsg == WM_NCCREATE) {
        CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
        assert(cs->lpCreateParams != nullptr);

        // stash pointer to self
        Window *pWnd = (Window *)cs->lpCreateParams;
        if (pWnd) {
            pWnd->attach(hWnd);
        }
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    } else if (uMsg == WM_INITDIALOG) {
        Window *pWnd = (Window *)lParam;
        if (pWnd) {
            pWnd->attach(hWnd);
        }

        // stash pointer to self
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)lParam);
    }

    // pass the messages into the BaseWnd
    Window *pBaseWnd = (Window *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (pBaseWnd == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    } else {
        // assert(pBaseWnd->getHandle());
        return pBaseWnd->wndProc(uMsg, wParam, lParam);
    }
}

bool registerWndClass(HINSTANCE hInstance, cstr_t className) {
    auto u16ClassName = utf8ToUCS2(className);
    WNDCLASSEXW wc;

    wc.cbSize = sizeof(wc);

    if (GetClassInfoExW(hInstance, u16ClassName.c_str(), &wc)) {
        return true;
    }

    // regiszter pane class
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = (WNDPROC)baseWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = nullptr;
    wc.hIconSm = nullptr;
    wc.hCursor = LoadCursor(nullptr, MAKEINTRESOURCE(IDC_ARROW));
    wc.hbrBackground= (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName= u16ClassName.c_str();

    int r = RegisterClassExW(&wc);
    if (r == 0) {
        int res = GetLastError();
        if (res != ERROR_CLASS_ALREADY_EXISTS) {
            // LOG1("Failed to register class, err %d", res);
            return false;
        }
    }

    return true;
}

bool isTopmostWindow(HWND hWnd) {
    auto dwStyleEx = (DWORD)::GetWindowLong(hWnd, GWL_EXSTYLE);

    return ((dwStyleEx & WS_EX_TOPMOST) == WS_EX_TOPMOST);
}

void topmostWindow(HWND hwnd, bool bTopmost) {
    if (bTopmost) {
        auto dwStyleEx = (DWORD)::GetWindowLong(hwnd, GWL_EXSTYLE);
        dwStyleEx |= WS_EX_TOPMOST;
        ::SetWindowLong(hwnd, GWL_EXSTYLE, dwStyleEx);
        ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |SWP_NOACTIVATE);
    } else {
        auto dwStyleEx = (DWORD)::GetWindowLong(hwnd, GWL_EXSTYLE);
        dwStyleEx &= ~WS_EX_TOPMOST;
        ::SetWindowLong(hwnd, GWL_EXSTYLE, dwStyleEx);
        ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |SWP_NOACTIVATE);
    }
}

bool showAsAppWindowNoRefresh(HWND hWnd) {
    // 仅给「纯 TOOLWINDOW」临时最大化用：去掉 TOOLWINDOW 以便 SW_SHOWMAXIMIZED。
    // 常驻「进任务栏」请用 showAsAppWindow（保留 TOOLWINDOW|APPWINDOW）。
    auto dwStyleEx = (uint32_t)GetWindowLong(hWnd, GWL_EXSTYLE);
    if (isFlagSet(dwStyleEx, WS_EX_TOOLWINDOW) && !isFlagSet(dwStyleEx, WS_EX_APPWINDOW)) {
        dwStyleEx &= ~(WS_EX_TOOLWINDOW | WS_EX_PALETTEWINDOW);
        dwStyleEx |= WS_EX_APPWINDOW;
        SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        return true;
    }
    return false;
}

bool showAsToolWindowNoRefresh(HWND hWnd) {
    // 目标：纯 TOOLWINDOW（无 APPWINDOW）。用于副窗临时最大化还原。
    auto dwStyleEx = (uint32_t)GetWindowLong(hWnd, GWL_EXSTYLE);
    if (!isFlagSet(dwStyleEx, WS_EX_TOOLWINDOW) || isFlagSet(dwStyleEx, WS_EX_APPWINDOW)
        || isFlagSet(dwStyleEx, WS_EX_PALETTEWINDOW)) {
        dwStyleEx |= WS_EX_TOOLWINDOW;
        dwStyleEx &= ~(WS_EX_APPWINDOW | WS_EX_PALETTEWINDOW);
        SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        return true;
    }
    return false;
}

HWND getParentOrSelf(HWND wnd) {
    HWND parent = wnd;
    while (parent) {
        auto tmp = GetParent(parent);
        if (!tmp) {
            break;
        }
        parent = tmp;
    }

    return parent;
}

bool showAsToolWindow(HWND hWnd) {
    // 目标：纯 TOOLWINDOW（无任务栏）。可从「无 TOOLWINDOW」或「TOOLWINDOW|APPWINDOW」转入。
    auto dwStyleEx = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
    if (!isFlagSet(dwStyleEx, WS_EX_TOOLWINDOW) || isFlagSet(dwStyleEx, WS_EX_APPWINDOW)
        || isFlagSet(dwStyleEx, WS_EX_PALETTEWINDOW)) {
        ShowWindow(hWnd, SW_HIDE);
        dwStyleEx |= WS_EX_TOOLWINDOW;
        dwStyleEx &= ~(WS_EX_APPWINDOW | WS_EX_PALETTEWINDOW);
        SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        ShowWindow(hWnd, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}

// 进任务栏：APPWINDOW + 保留 TOOLWINDOW。
// Win11 对「仅 APPWINDOW」的 per-pixel layered 主窗会套 DWMWCP_ROUNDSMALL（约 2–3px），
// 磁吸开边后仍在 TL/TR 裁出白月牙；副窗自带 TOOLWINDOW 故接缝正常。DWMWA_DONOTROUND 对本类分层窗无效。
bool showAsAppWindow(HWND hWnd) {
    auto dwStyleEx = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
    const bool needApp = !isFlagSet(dwStyleEx, WS_EX_APPWINDOW);
    const bool needTool = !isFlagSet(dwStyleEx, WS_EX_TOOLWINDOW);
    const bool dropPalette = isFlagSet(dwStyleEx, WS_EX_PALETTEWINDOW);
    if (needApp || needTool || dropPalette) {
        ShowWindow(hWnd, SW_HIDE);
        dwStyleEx &= ~WS_EX_PALETTEWINDOW;
        dwStyleEx |= WS_EX_TOOLWINDOW | WS_EX_APPWINDOW;
        SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        ShowWindow(hWnd, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}

bool isChildWnd(HWND hWnd) {
    auto dwStyleEx = (DWORD)::GetWindowLong(hWnd, GWL_STYLE);
    return (dwStyleEx & WS_CHILD) == WS_CHILD;
}

bool hasCaption(HWND hWnd) {
    auto dwStyleEx = (DWORD)::GetWindowLong(hWnd, GWL_STYLE);
    return (dwStyleEx & WS_CAPTION) == WS_CAPTION;
}

void removeCaption(HWND hWnd) {
    auto dwStyleEx = (DWORD)::GetWindowLong(hWnd, GWL_STYLE);
    dwStyleEx &= ~WS_CAPTION;
    ::SetWindowLong(hWnd, GWL_STYLE, dwStyleEx);
    ::SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void addCaption(HWND hWnd) {
    auto dwStyleEx = (DWORD)::GetWindowLong(hWnd, GWL_STYLE);
    if ((dwStyleEx & WS_CAPTION) != WS_CAPTION) {
        dwStyleEx |= WS_CAPTION;
        ::SetWindowLong(hWnd, GWL_STYLE, dwStyleEx);
        ::SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

bool moveWindowSafely(HWND hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint) {
    if (isChildWnd(hWnd)) {
        POINT    pt;
        HWND    hWndParent;
        hWndParent = GetParent(hWnd);
        if (hWndParent) {
            pt.x = X;
            pt.y = Y;
            ScreenToClient(hWndParent, &pt);
            X = pt.x;
            Y = pt.y;
        }
    }

    bool bHasCap = hasCaption(hWnd);
    if (bHasCap)
        removeCaption(hWnd);

    bool bRet = MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);

    if (bHasCap)
        addCaption(hWnd);

    return bRet;
}

void centerWindowToMonitor(HWND hWnd) {
    // 全程物理像素，避免与逻辑坐标 API 混用
    RECT rc = {};
    GetWindowRect(hWnd, &rc);
    HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!mon || !GetMonitorInfo(mon, &mi)) {
        return;
    }

    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
    const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

    SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool isLayeredWndSupported()
{
    OSVERSIONINFO        version;

    memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);

    if (!GetVersionEx(&version))
    {
        //        LOGOUT1(LOG_LVL_ERROR, _T("GetVersionEx FAILED! Error Id: %d"), GetLastError);
        return FALSE;
    }

    // Major version: 5, windows 2000
    if (version.dwMajorVersion < 5)
        return FALSE;

    return TRUE;
}

//    COMMENT:
//        sets the opacity and transparency color key of a layered window
//    INPUT:
//        crKey,        specifies the color key
//        bAlpha,        value for the blend function, When bAlpha is 0, 
//                    the window is completely transparent. 
//                    When bAlpha is 255, the window is opaque. 
//        dwFlags        Specifies an action to take. This parameter can be one or 
//                    more of the following values. Value Meaning 
//                    LWA_COLORKEY Use crKey as the transparency color.  
//                    LWA_ALPHA Use bAlpha to determine the opacity of the layered window 
bool setLayeredWindow(HWND hWnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags) {
    DWORD dwStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    dwStyle |= WS_EX_LAYERED;
    SetWindowLong(hWnd, GWL_EXSTYLE, dwStyle);

    return SetLayeredWindowAttributes(hWnd, crKey, bAlpha, dwFlags);
}

void unSetLayeredWindow(HWND hWnd) {
    DWORD dwStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    dwStyle &= ~WS_EX_LAYERED;
    SetWindowLong(hWnd, GWL_EXSTYLE, dwStyle);
}


Window::Window() {
    m_hWnd = nullptr;
    m_WndSizeMode = (WndSizeMode)-1;
    m_scaleFactor = getScreenScaleFactor();
}

Window::~Window() {

}

void Window::updateScaleFactorFromDpi(UINT dpi) {
    if (dpi < 72) {
        dpi = 96;
    }
    m_scaleFactor = dpi / 96.0f;
    if (m_scaleFactor < 0.5f) {
        m_scaleFactor = 1.0f;
    }
}

void Window::updateScaleFactorFromWindow() {
    loadDpiApis();
    if (m_hWnd && g_GetDpiForWindow) {
        UINT dpi = g_GetDpiForWindow(m_hWnd);
        if (dpi > 0) {
            updateScaleFactorFromDpi(dpi);
            return;
        }
    }
    updateScaleFactorFromDpi(querySystemDpi());
}

int Window::toPhysical(int logical) const {
    return (int)lround(logical * m_scaleFactor);
}

int Window::toLogical(int physical) const {
    if (m_scaleFactor <= 0.f) {
        return physical;
    }
    return (int)lround(physical / m_scaleFactor);
}

float Window::toPhysicalF(float logical) const {
    return logical * m_scaleFactor;
}

float Window::toLogicalF(float physical) const {
    if (m_scaleFactor <= 0.f) {
        return physical;
    }
    return physical / m_scaleFactor;
}

void Window::toPhysical(CRect &rc) const {
    rc.left = toPhysical(rc.left);
    rc.top = toPhysical(rc.top);
    rc.right = toPhysical(rc.right);
    rc.bottom = toPhysical(rc.bottom);
}

void Window::toLogical(CRect &rc) const {
    rc.left = toLogical(rc.left);
    rc.top = toLogical(rc.top);
    rc.right = toLogical(rc.right);
    rc.bottom = toLogical(rc.bottom);
}

void Window::toPhysical(CPoint &pt) const {
    pt.x = toPhysical(pt.x);
    pt.y = toPhysical(pt.y);
}

void Window::toLogical(CPoint &pt) const {
    pt.x = toLogical(pt.x);
    pt.y = toLogical(pt.y);
}

bool Window::create(cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, uint32_t dwStyle, int nID) {
    return createEx(BASEWNDCLASSNAME, szCaption, x, y, nWidth, nHeight, pWndParent, dwStyle, nID);
}

bool Window::createEx(cstr_t szClassName, cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, uint32_t dwStyle, uint32_t dwExStyle, int64_t nID) {
    if (!registerWndClass(getAppInstance(), szClassName)) {
        return false;
    }

    assert(m_hWnd == nullptr);

    // DPI-aware 下 CreateWindow 使用物理像素；皮肤传入的是逻辑尺寸（同 Mac points）
    updateScaleFactorFromDpi(queryDpiForPoint(x, y));
    const int physX = toPhysical(x);
    const int physY = toPhysical(y);
    const int physW = toPhysical(nWidth);
    const int physH = toPhysical(nHeight);

    // 创建窗口
    m_hWnd = CreateWindowExA(
        dwExStyle,              // extend styles
        szClassName,            // window class name
        szCaption,              // window title
        dwStyle,                // window style
        physX, physY,           // screen position (physical)
        physW, physH,           // width & height (physical)
        pWndParent ? pWndParent->m_hWnd : nullptr,    // parent window
        (HMENU)nID,             // menu or window id
        getAppInstance(),       // hInstance
        (void *)this);          // this for window creation data

    if (m_hWnd) {
        updateScaleFactorFromWindow();
    }

    return m_hWnd != nullptr;
}

bool Window::createForSkin(cstr_t szClassName, cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, bool bToolWindow, bool bTopmost, bool bVisible) {
    // 不使用系统原生标题栏：chrome 由皮肤样式（Caption / Caption.mac 等）自绘。
    uint32_t dwStyle = DS_NOIDLEMSG | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_MAXIMIZEBOX;
    uint32_t dwExStyle = 0;
    if (bTopmost) {
        dwExStyle |= WS_EX_TOPMOST;
    }
    if (bToolWindow) {
        dwExStyle |= WS_EX_TOOLWINDOW;
    } else {
        // 主窗：进任务栏，同时带 TOOLWINDOW，避免 Win11 对纯 APPWINDOW 分层窗套 ROUNDSMALL。
        // Mac 忽略 bToolWindow，圆角只由皮肤 WindowImage / EnableEdgeMerge 控制。
        dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_APPWINDOW;
    }
    if (bVisible) {
        dwStyle |= WS_VISIBLE;
    }

    // WS_THICKFRAME will cause window flashing on vista
    if (!createEx(szClassName, szCaption,
        x, y, nWidth, nHeight,
        pWndParent,
        dwStyle, dwExStyle)) {
        return false;
    }

    return true;
}

void Window::destroy() {
    ::DestroyWindow(m_hWnd);
}

void Window::postDestroy() {
    PostMessage(m_hWnd, WM_CLOSE, 0, 0);
}

void Window::onPaint(CRawGraph *canvas, CRect *rcClip) {
}

void Window::activateWindow() {
    uint32_t dwTimeOutPrev = 0;

    SendMessage(m_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    ::ShowWindow(m_hWnd, SW_SHOW);

    if (!isTopmost()) {
        ::SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        ::SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
    // save old
    SystemParametersInfo(0x2000, 0, (void *)&dwTimeOutPrev, 0);
    // set new
    SystemParametersInfo(0x2001, 0, (void *)0, SPIF_SENDWININICHANGE);

    if (::GetForegroundWindow() != m_hWnd) {
        SetForegroundWindow(m_hWnd);
    }
    if (IsWindowEnabled(m_hWnd)) {
        SetActiveWindow(m_hWnd);
    }

    // restore old
    SystemParametersInfo(0x2001, dwTimeOutPrev, (void *)0, SPIF_SENDWININICHANGE);
}

void Window::showNoActivate() {
    ::ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
}

void Window::show() {
    ::ShowWindow(m_hWnd, SW_SHOW);
}

void Window::hide() {
    ::ShowWindow(m_hWnd, SW_HIDE);
}

void Window::minimize() {
    ::ShowWindow(m_hWnd, SW_MINIMIZE);
}

void Window::maximize() {
    ::ShowWindow(m_hWnd, SW_MAXIMIZE);
}

void Window::restore() {
    if (isZoomed()) {
        maximize();
    } else {
        minimize();
    }
}

void Window::minimizeNoActivate() {
    ::ShowWindow(m_hWnd, SW_SHOWMINNOACTIVE);
}

string Window::getDlgItemText(int nIDItem) {
    auto len= SendDlgItemMessage(m_hWnd, nIDItem, WM_GETTEXTLENGTH, 0, 0);
    if (len> 0) {
        utf16string buf;
        buf.resize(len+ 1);
        len = ::GetDlgItemTextW(m_hWnd, nIDItem, (WCHAR *)buf.c_str(), buf.capacity());

        return ucs2ToUtf8(buf.c_str(), len);
    }

    return string();
}

bool Window::setDlgItemText(int nIDItem, cstr_t szString) {
    return ::SetDlgItemTextW(m_hWnd, nIDItem, utf8ToUCS2(szString).c_str());
}

bool Window::getDlgItemRect(int nIDItem, CRect* lpRect) {
    HWND hWnd = ::GetDlgItem(m_hWnd, nIDItem);
    if (!hWnd) {
        return false;
    }

    if (!::GetWindowRect(hWnd, lpRect)) {
        return false;
    }
    toLogical(*lpRect);
    return true;
}

void Window::screenToClient(CRect &rc) {
    toPhysical(rc);
    ::ScreenToClient(m_hWnd, (LPPOINT)&rc.left);
    ::ScreenToClient(m_hWnd, (LPPOINT)&rc.right);
    toLogical(rc);
}

void Window::clientToScreen(CRect &rc) {
    toPhysical(rc);
    ::ClientToScreen(m_hWnd, (LPPOINT)&rc.left);
    ::ClientToScreen(m_hWnd, (LPPOINT)&rc.right);
    toLogical(rc);
}

void Window::screenToClient(CPoint &pt) {
    toPhysical(pt);
    ::ScreenToClient(m_hWnd, &pt);
    toLogical(pt);
}

void Window::clientToScreen(CPoint &pt) {
    toPhysical(pt);
    ::ClientToScreen(m_hWnd, &pt);
    toLogical(pt);
}

bool Window::getWindowRect(CRect* lpRect) {
    if (!::GetWindowRect(m_hWnd, lpRect)) {
        return false;
    }
    toLogical(*lpRect);
    return true;
}

bool Window::getClientRect(CRect* lpRect) {
    if (!::GetClientRect(m_hWnd, lpRect)) {
        return false;
    }
    toLogical(*lpRect);
    return true;
}

float Window::getScaleFactor() {
    return m_scaleFactor;
}

void Window::setCursor(Cursor *cursor) {
    m_isCursorSet = true;
    cursor->set();
    // setWndCursor(cursor);
}

bool Window::setTimer(uint32_t nTimerId, uint32_t nElapse) {
    return tobool(::SetTimer(m_hWnd, nTimerId, nElapse, nullptr));
}

void Window::killTimer(uint32_t nTimerId) {
    ::KillTimer(m_hWnd, nTimerId);
}

string Window::getTitle() {
    string str;
    int len = SendMessage(m_hWnd, WM_GETTEXTLENGTH, 0, 0);
    if (len > 0) {
        utf16string buf;
        buf.resize(len + 1);
        len = ::GetWindowTextW(m_hWnd, (WCHAR *)buf.c_str(), str.capacity());

        str.resize(len);
    }

    return str;
}

bool Window::setTitle(cstr_t szText) {
    return tobool(::SetWindowTextW(m_hWnd, utf8ToUCS2(szText).c_str()));
}

bool Window::setWndCursor(Cursor *pCursor) {
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)pCursor->getHandle());
    return true;
}

bool Window::setFocus() {
    return ::SetFocus(m_hWnd) != nullptr;
}

bool Window::setCapture() {
    ::SetCapture(m_hWnd);

    return true;
}

void Window::releaseCapture() {
    ::ReleaseCapture();
}

bool Window::invalidateRect(const CRect *lpRect, bool bErase) {
    if (!lpRect) {
        return tobool(::InvalidateRect(m_hWnd, nullptr, bErase));
    }
    CRect rcPhys = *lpRect;
    toPhysical(rcPhys);
    return tobool(::InvalidateRect(m_hWnd, &rcPhys, bErase));
}

bool Window::isChild() {
    return ::GetParent(m_hWnd) != NULL;
}

bool Window::isMouseCaptured() {
    return GetCapture() == m_hWnd;
}

bool Window::isIconic() {
    return tobool(::IsIconic(m_hWnd));
}

bool Window::isWindow() {
    return tobool(::IsWindow(m_hWnd));
}

bool Window::isVisible() {
    return tobool(::IsWindowVisible(m_hWnd));
}

bool Window::isTopmost() {
    auto hWnd = getParentOrSelf(m_hWnd);

    uint32_t dwStyleEx = (uint32_t)::GetWindowLong(hWnd, GWL_EXSTYLE);

    return ((dwStyleEx & WS_EX_TOPMOST) == WS_EX_TOPMOST);
}

void Window::setTopmost(bool bTopmost) {
    auto hWnd = getParentOrSelf(m_hWnd);

    uint32_t dwStyleEx;
    if (bTopmost) {
        dwStyleEx = (uint32_t)::GetWindowLong(hWnd, GWL_EXSTYLE);
        dwStyleEx |= WS_EX_TOPMOST;
        ::SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        ::SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |SWP_NOACTIVATE);
    } else {
        dwStyleEx = (uint32_t)::GetWindowLong(hWnd, GWL_EXSTYLE);
        dwStyleEx &= ~WS_EX_TOPMOST;
        ::SetWindowLong(hWnd, GWL_EXSTYLE, dwStyleEx);
        ::SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |SWP_NOACTIVATE);
    }
}

bool Window::isToolWindow() {
    // 「纯工具窗」：有 TOOLWINDOW 且无 APPWINDOW（托盘-only / 不进任务栏）。
    // 主窗常驻 TOOLWINDOW|APPWINDOW 以规避 Win11 DWM 圆角，此时不算 tool window（最小化进任务栏）。
    DWORD dwStyleEx = (DWORD)GetWindowLong(m_hWnd, GWL_EXSTYLE);
    return (dwStyleEx & WS_EX_TOOLWINDOW) == WS_EX_TOOLWINDOW
        && (dwStyleEx & WS_EX_APPWINDOW) != WS_EX_APPWINDOW;
}

void Window::setToolWindow(bool bToolWindow) {
    if (bToolWindow) {
        showAsToolWindow(m_hWnd);
    } else {
        showAsAppWindow(m_hWnd);
    }
}

bool Window::isForeground() {
    return ::GetForegroundWindow() == m_hWnd;
}

bool Window::setForeground() {
    return tobool(::SetForegroundWindow(m_hWnd));
}

void Window::setWindowPos(int x, int y) {
    ::SetWindowPos(m_hWnd, nullptr, toPhysical(x), toPhysical(y), 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREPOSITION);
}

void Window::setWindowPosSafely(int x, int y) {
    int physX = toPhysical(x);
    int physY = toPhysical(y);
    if (isChild()) {
        HWND hWndParent;
        hWndParent = ::GetParent(m_hWnd);
        if (hWndParent) {
            CPoint pt;
            pt.x = physX;
            pt.y = physY;
            ::ScreenToClient(hWndParent, &pt);
            physX = pt.x;
            physY = pt.y;
        }
    }
    ::SetWindowPos(m_hWnd, nullptr, physX, physY, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREPOSITION);
}

bool Window::moveWindow(int X, int Y, int nWidth, int nHeight, bool bRepaint) {
    return tobool(::MoveWindow(m_hWnd, toPhysical(X), toPhysical(Y),
        toPhysical(nWidth), toPhysical(nHeight), bRepaint));
}

bool Window::moveWindowSafely(int X, int Y, int nWidth, int nHeight, bool bRepaint) {
    return tobool(::moveWindowSafely(m_hWnd, toPhysical(X), toPhysical(Y),
        toPhysical(nWidth), toPhysical(nHeight), bRepaint));
}

int Window::messageOut(cstr_t text, uint32_t uType, cstr_t caption) {
    utf16string u16Cap;
    if (caption == nullptr) {
        WCHAR buf[512] = { 0 };
        auto len = GetWindowTextW(m_hWnd, buf, CountOf(buf));
        u16Cap.assign(buf, len);
    } else {
        u16Cap = utf8ToUCS2(caption);
    }

    return MessageBoxW(m_hWnd, utf8ToUCS2(text).c_str(), u16Cap.c_str(), uType);
}

bool Window::replaceChildPos(int nIDChildSrcPos, Window *pChildNew) {
    CRect rcPos;

    getDlgItemRect(nIDChildSrcPos, &rcPos);
    screenToClient(rcPos);

    pChildNew->moveWindow(rcPos.left, rcPos.top, rcPos.width(), rcPos.height(), true);

    return true;
}

void Window::postUserMessage(int nMessageID, LPARAM param) {
    PostMessage(m_hWnd, ML_WM_USER, nMessageID, param);
}

void Window::sendUserMessage(int nMessageID, LPARAM param) {
    SendMessage(m_hWnd, ML_WM_USER, nMessageID, param);
}

LRESULT Window::wndProc(uint32_t message, WPARAM wParam, LPARAM lParam) {
    assert(m_hWnd);

    auto clientPtFromLParam = [this](LPARAM lp) {
        CPoint pt((short)LOWORD(lp), (short)HIWORD(lp));
        toLogical(pt);
        return pt;
    };
    auto screenPtFromLParam = [this](LPARAM lp) {
        CPoint pt((short)LOWORD(lp), (short)HIWORD(lp));
        toLogical(pt);
        return pt;
    };

    switch (message) {
    case ML_WM_LANGUAGE_CHANGED:
        onLanguageChanged();
        break;
    case ML_WM_USER:
        onUserMessage(wParam, lParam);
        break;
    case WM_ACTIVATE:
        {
            uint16_t wActived;
            // bool        bMinimized;
            // HWND        hWndOld;

            wActived = LOWORD(wParam);
            // bMinimized = HIWORD(wParam);
            // hWndOld = (HWND)lParam;

            onActivate(tobool(wActived));
        }
        break;
    case WM_CREATE:
        updateScaleFactorFromWindow();
        onCreate();
        return 0;
    case WM_CLOSE:
        if (onClose()) {
            destroy();
        }
        return 0;
    case WM_COMMAND:
        {
            uint32_t uId = LOWORD(wParam);
            onCommand(uId);
        }
        break;
    case WM_CONTEXTMENU:
        {
            if (lParam == -1) {
                CPoint pt;
                GetCursorPos(&pt);
                toLogical(pt);
                onContexMenu(pt.x, pt.y);
            } else {
                CPoint pt = screenPtFromLParam(lParam);
                onContexMenu(pt.x, pt.y);
            }
        }
        break;
    case WM_DESTROY:
        {
            onDestroy();
            if (m_hWnd) {
                SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)0);
                DefWindowProc(m_hWnd, message, wParam, lParam);
                m_hWnd = nullptr;
            }
        }
        return 0;
    case WM_DROPFILES:
        onDropFiles((HDROP)wParam);
        break;
    case WM_HSCROLL:
        onHScroll(LOWORD(wParam), HIWORD(wParam), nullptr);
        break;
    case WM_KEYDOWN:
        onKeyDown(wParam, lParam);
        break;
    case WM_KEYUP:
        onKeyUp(wParam, lParam);
        break;
    case WM_CHAR:
        //    case WM_UNICHAR:
        onChar(wParam);
        break;
    case WM_LBUTTONDOWN:
        onLButtonDown((uint32_t)wParam, clientPtFromLParam(lParam));
        break;
    case WM_LBUTTONDBLCLK:
        onLButtonDblClk((uint32_t)wParam, clientPtFromLParam(lParam));
        break;
    case WM_LBUTTONUP:
        onLButtonUp((uint32_t)wParam, clientPtFromLParam(lParam));
        break;
    case WM_RBUTTONDOWN:
        onRButtonDown((uint32_t)wParam, clientPtFromLParam(lParam));
        break;
    case WM_RBUTTONUP:
        onRButtonUp((uint32_t)wParam, clientPtFromLParam(lParam));
        break;
    case WM_MOUSEMOVE:
        {
            m_isCursorSet = false;

            uint32_t nFlags = uint32_t(wParam);
            if (nFlags & MK_LBUTTON) {
                onMouseDrag((uint32_t)wParam, clientPtFromLParam(lParam));
            } else {
                onMouseMove(clientPtFromLParam(lParam));
            }
        }
        break;
    case WM_MOUSEWHEEL:
        {
            onMouseWheel(short(HIWORD(wParam)) / -60, LOWORD(wParam), screenPtFromLParam(lParam));
        }
        break;
    case WM_SETCURSOR:
        // m_isCursorSet flag is used to fix cursor flashing problem.
        if (!m_isCursorSet) {
            return DefWindowProc(m_hWnd, message, wParam, lParam);
        }
        return true;
    case WM_MOVE:
        onMove(toLogical((short)LOWORD(lParam)), toLogical((short)HIWORD(lParam)));
        return 0;
    case WM_KILLFOCUS:
        onKillFocus();
        break;
    case WM_SETFOCUS:
        onSetFocus();
        break;
    case WM_SIZE:
        if (wParam == SIZE_RESTORED) {
            auto cx = toLogical((int)LOWORD(lParam));
            auto cy = toLogical((int)HIWORD(lParam));
            if (cx > 0 && cy > 0) {
                onResized(cx, cy);
            }
            if (m_WndSizeMode != WndSizeMode_Normal) {
                m_WndSizeMode = WndSizeMode_Normal;
                onSizeModeChanged(m_WndSizeMode);
            }
        } else if (wParam == SIZE_MINIMIZED) {
            m_WndSizeMode = WndSizeMode_Minimized;
            onSizeModeChanged(m_WndSizeMode);
        } else if (wParam == SIZE_MAXIMIZED) {
            onResized(toLogical((int)LOWORD(lParam)), toLogical((int)HIWORD(lParam)));
            m_WndSizeMode = WndSizeMode_Maximized;
            onSizeModeChanged(m_WndSizeMode);
        }
        break;
    case WM_DPICHANGED:
        {
            const float oldScale = m_scaleFactor;
            updateScaleFactorFromDpi(LOWORD(wParam));
            auto *const prcNew = (RECT *)lParam;
            if (prcNew) {
                // 建议矩形已是物理像素，直接交给系统
                ::SetWindowPos(m_hWnd, nullptr, prcNew->left, prcNew->top,
                    prcNew->right - prcNew->left, prcNew->bottom - prcNew->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            if (oldScale != m_scaleFactor) {
                onScaleFactorChanged(m_scaleFactor);
                invalidateRect(nullptr, true);
            }
        }
        return 0;
    case WM_VSCROLL:
        {
            onVScroll(LOWORD(wParam), HIWORD(wParam), nullptr);
        }
        break;
    case WM_TIMER:
        onTimer((uint32_t)wParam);
        break;
    default:
        return DefWindowProc(m_hWnd, message, wParam, lParam);
    }
    return 0;
}


void Window::setTransparent(uint8_t nAlpha, bool bClickThrough) {
    // If it's child old, ignore this.
    if (::GetParent(m_hWnd)) {
        return;
    }

    m_nAlpha = nAlpha;
    m_bClickThrough = bClickThrough;

    if (m_bTranslucencyLayered) {
        invalidateRect(nullptr, true);
        return;
    }

    COLORREF clrTrans = RGB(0, 0, 0);
    uint32_t dwFlags = 0;

    if (nAlpha < 255) {
        dwFlags = LWA_ALPHA;
    }

    /*    if (bTransparentClr)
    {
        clrTrans = clrTransparent;
        dwFlags |= LWA_COLORKEY;
    }*/

    if (isClickThrough()) {
        SetWindowLong(m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    } else {
        SetWindowLong(m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
    }

    if (dwFlags != 0 || (isClickThrough())) {
        ::setLayeredWindow(m_hWnd, clrTrans, nAlpha, dwFlags);
    } else {
        ::unSetLayeredWindow(m_hWnd);
    }
}

// Win10+ 未公开的窗口合成属性：用 Accent Policy 做磨砂/Acrylic。
// DwmEnableBlurBehindWindow 在 WS_EX_LAYERED + UpdateLayeredWindow 下已失效（只剩半透明无 blur）。
enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19,
};

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4, // Win10 1803+
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor; // 0xAABBGGRR
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using PFN_SetWindowCompositionAttribute =
    BOOL (WINAPI *)(HWND, WINDOWCOMPOSITIONATTRIBDATA *);

static PFN_SetWindowCompositionAttribute getSetWindowCompositionAttribute() {
    static PFN_SetWindowCompositionAttribute pfn = nullptr;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            pfn = (PFN_SetWindowCompositionAttribute)GetProcAddress(
                user32, "SetWindowCompositionAttribute");
        }
    }
    return pfn;
}

void Window::applyGlassAccentPolicy() {
    if (m_hWnd == nullptr) {
        return;
    }

    auto pSet = getSetWindowCompositionAttribute();
    if (pSet) {
        ACCENT_POLICY policy = {};
        if (m_bGlassAccentOn) {
            // Acrylic：透明/半透明像素透出磨砂；浅色 tint 贴近 Glass 皮肤 #E8EAF5。
            // GradientColor = 0xAABBGGRR（A 控制 tint 强度，皮肤 bg.png 再叠一层着色）。
            // 与 UpdateLayeredWindow 可共存；旧 DwmEnableBlurBehindWindow 在 layered 下无效。
            policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
            policy.AccentFlags = 0;
            policy.GradientColor = 0x40F5EAE8;
        } else {
            policy.AccentState = ACCENT_DISABLED;
        }

        WINDOWCOMPOSITIONATTRIBDATA data = {};
        data.Attrib = WCA_ACCENT_POLICY;
        data.pvData = &policy;
        data.cbData = sizeof(policy);
        pSet(m_hWnd, &data);
        return;
    }

    // 无 SetWindowCompositionAttribute 时（极旧系统）再退 DWM blur-behind。
    DWM_BLURBEHIND bb;
    memset(&bb, 0, sizeof(bb));
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = m_bGlassAccentOn ? TRUE : FALSE;
    bb.fTransitionOnMaximized = TRUE;
    DwmEnableBlurBehindWindow(m_hWnd, &bb);
}

void Window::setGlassEffect(bool bGlassEffect, cstr_t szMaterial) {
    // szMaterial 仅 mac 区分 NSVisualEffect 材质；Win 未区分。
    (void)szMaterial;
    m_bGlassAccentOn = bGlassEffect;
    if (m_hWnd == nullptr) {
        return;
    }
    applyGlassAccentPolicy();
}

bool Window::updateLayeredWindowUsingMemGraph(CRawGraph *canvas) {
    HDC hdc = GetDC(m_hWnd);
    HDC hdcSrc = canvas->getHandle();

    DWORD dwStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
    DWORD dwStyleNew = dwStyle | WS_EX_LAYERED;
    if (isClickThrough()) {
        dwStyleNew |= WS_EX_TRANSPARENT;
    } else {
        dwStyleNew &= ~WS_EX_TRANSPARENT;
    }
    const bool becameLayered = (dwStyle != dwStyleNew);
    if (becameLayered) {
        SetWindowLong(m_hWnd, GWL_EXSTYLE, dwStyleNew);
    }

    // UpdateLayeredWindow 需要物理像素尺寸，与 memGraph（logical * scale）一致
    RECT rcPhys = {};
    ::GetWindowRect(m_hWnd, &rcPhys);

    BLENDFUNCTION blend;
    blend.AlphaFormat = AC_SRC_ALPHA;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = m_nAlpha;

    CPoint ptSrc, ptDest(rcPhys.left, rcPhys.top);
    CSize sizeWnd(rcPhys.right - rcPhys.left, rcPhys.bottom - rcPhys.top);

    bool bRet = UpdateLayeredWindow(m_hWnd, hdc, &ptDest, &sizeWnd, hdcSrc, &ptSrc, 0, &blend, ULW_ALPHA);

    ReleaseDC(m_hWnd, hdc);

    // Accent Policy 需在 WS_EX_LAYERED 生效后再写一次，否则磨砂挂不到 per-pixel 透明窗上。
    if (becameLayered && m_bGlassAccentOn) {
        applyGlassAccentPolicy();
    }

    return bRet;
}

void Window::attach(HWND hWnd) {
    m_hWnd = hWnd;
    updateScaleFactorFromWindow();
}

void Window::detach() {
    m_hWnd = nullptr;
}

WindowHandle Window::getHandle() {
    return (WindowHandle)m_hWnd;
}

// 在编辑框开始/结束编辑时需要调用
void Window::startTextInput() {

}

void Window::endTextInput() {

}

// 编辑控件的光标位置改变后需要调用此函数
void Window::caretPositionChanged(const CPoint &point) {

}
