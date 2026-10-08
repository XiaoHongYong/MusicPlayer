#pragma once

#ifndef _ML_WINDOW_H_INC_
#define _ML_WINDOW_H_INC_

#include "../IWindow.h"
#include "../Cursor.h"
#include <shellapi.h>


const int ML_WM_LANGUAGE_CHANGED = (WM_USER + 1212);
const int ML_LANGUAGE_CHANGED_PARAM = 172172;
#define ML_WM_USER                (WM_USER + 1213)

LRESULT CALLBACK baseWndProc(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);
void unSetLayeredWindow(HWND hWnd);
bool isTopmostWindow(HWND hWnd);
void topmostWindow(HWND hwnd, bool bTopmost);
bool showAsAppWindowNoRefresh(HWND hWnd);
bool showAsToolWindowNoRefresh(HWND hWnd);
bool isChildWnd(HWND hWnd);
void centerWindowToMonitor(HWND hWnd);

class Window : public IWindow {
public:
    Window();
    virtual ~Window();

public:
    virtual bool create(cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, uint32_t dwStyle = DS_NOIDLEMSG | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX, int nID = ID_UNDEFINE);
    virtual bool createEx(cstr_t szClassName, cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, uint32_t dwStyle = DS_NOIDLEMSG | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX, uint32_t dwExStyle = 0, int64_t nID = ID_UNDEFINE);

    virtual bool createForSkin(cstr_t szClassName, cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, bool bToolWindow = true, bool bTopmost = false, bool bVisible = true);

    virtual void destroy();

    void postDestroy();

public:
    //
    // Messages
    //
    virtual void onDropFiles(HDROP hDrop) { }


public:
    //
    // Operations
    //
    void activateWindow();

    void showNoActivate();
    void show();
    void hide();
    void minimize();
    void minimizeNoActivate();
    void maximize();
    void restore();

    // Windows 的皮肤窗口没有系统边框，缩放由 WndResizer 处理(其中已限制最小尺寸)，
    // 此处仅为与 mac 保持一致的接口。
    void setMinSize(uint32_t width, uint32_t height) { }
    void setMaxSize(uint32_t width, uint32_t height) { }

    std::string getDlgItemText(int nIDItem);
    bool setDlgItemText(int nIDItem, cstr_t szString);
    bool setDlgItemInt(int nIDItem, uint32_t uValue, bool bSigned) { return ::SetDlgItemInt(m_hWnd, nIDItem, uValue, bSigned); }
    bool enableDlgItem(int nIDItem, bool bEnable) { return tobool(::EnableWindow(::GetDlgItem(m_hWnd, nIDItem), bEnable)); }
    bool enableWindow(bool bEnable) { return tobool(::EnableWindow(m_hWnd, bEnable)); }
    bool showDlgItem(int nIDItem, int nCmdShow) { return tobool(::ShowWindow(::GetDlgItem(m_hWnd, nIDItem), nCmdShow)); }
    bool getDlgItemRect(int nIDItem, CRect* lpRect);

    bool setTimer(uint32_t nTimerId, uint32_t nElapse);
    void killTimer(uint32_t nTimerId);

	string getTitle();
    bool setTitle(cstr_t szText);

    bool setWndCursor(Cursor *pCursor);

    bool setFocus();

    bool setCapture();
    void releaseCapture();

    void screenToClient(CRect &rc);
    void clientToScreen(CRect &rc);

    void screenToClient(CPoint &pt);
    void clientToScreen(CPoint &pt);

    bool getWindowRect(CRect* lpRect);
    bool getClientRect(CRect* lpRect);

    float getScaleFactor() override;

    void setCursor(Cursor *cursor) override;

    void setParent(Window *pWndParent);
    Window *getParent();

    virtual void onPaint(CRawGraph *surface, CRect *rcClip) override;

    virtual bool invalidateRect(const CRect* lpRect = nullptr, bool bErase = false);

    void checkButton(int nIDButton, bool bCheck) { CheckDlgButton(m_hWnd, nIDButton, bCheck ? BST_CHECKED : BST_UNCHECKED); }
    bool isButtonChecked(int nIDButton) { return IsDlgButtonChecked(m_hWnd, nIDButton) == BST_CHECKED; }

    bool isSameWnd(Window *pWnd) { return m_hWnd == pWnd->m_hWnd; }

    bool isChild();

    bool isMouseCaptured();

    bool isIconic();
    bool isZoomed() { return tobool(::IsZoomed(m_hWnd)); }

    bool isWindow();

    bool isValid() { return m_hWnd != nullptr; }

    bool isVisible();

    bool isTopmost();
    void setTopmost(bool bTopmost);

    bool isToolWindow();
    void setToolWindow(bool bToolWindow);

    bool isForeground();
    bool setForeground();

    void setWindowPos(int x, int y);
    void setWindowPosSafely(int x, int y);

    bool moveWindow(int X, int Y, int nWidth, int nHeight, bool bRepaint = true);

    bool moveWindowSafely(int X, int Y, int nWidth, int nHeight, bool bRepaint = true);

    int messageOut(cstr_t lpText, uint32_t uType = MB_ICONINFORMATION | MB_OK, cstr_t lpCaption = nullptr);

    bool replaceChildPos(int nIDChildSrcPos, Window *pChildNew);

    void postUserMessage(int nMessageID, LPARAM param);
    void sendUserMessage(int nMessageID, LPARAM param);

    virtual void onUserMessage(int nMessageID, LPARAM param) { }

    WindowHandle getHandle() override;

    //
    // 输入法相关的处理
    //
    // 在编辑框开始/结束编辑时需要调用
    virtual void startTextInput();
    virtual void endTextInput();

    // 编辑控件的光标位置改变后需要调用此函数
    void caretPositionChanged(const CPoint &point);

    // 最终输入的文字
    virtual void onInputText(cstr_t text) { }
    // MarketText 是临时的文字，当输入其他字符时会被替代
    virtual void onInputMarkedText(cstr_t text) { }

public:
    // Translucency related APIs
    bool                        m_bTranslucencyLayered; // Is following alpha setting enabled?

    // Alpha
    int                         m_nAlpha;
    bool                        m_bClickThrough;

    virtual void setTransparent(uint8_t nAlpha, bool bClickThrough);
    virtual bool isClickThrough() { return m_bClickThrough; }

    // 窗口磨砂玻璃背景：DWM blur-behind，透明区域透出桌面/下层窗口。由皮肤 <Window Glass="true"/> 触发。
    virtual void setGlassEffect(bool bGlassEffect, cstr_t szMaterial = nullptr);

    // DWM blur-behind 作用于整窗，无圆角概念；为保持三平台接口一致留空实现。
    virtual void setGlassRadius(int radius) { (void)radius; }

    bool updateLayeredWindowUsingMemGraph(CRawGraph *canvas);

public:
    void attach(HWND hWnd);
    void detach();
    HWND getWndHandle() { return m_hWnd; }

    virtual LRESULT wndProc(uint32_t message, WPARAM wParam, LPARAM lParam);

protected:
    WndSizeMode                 m_WndSizeMode;
    HWND                        m_hWnd;
    bool                        m_isCursorSet = false;

};

#endif // !defined(Window_win32_Window_h)
