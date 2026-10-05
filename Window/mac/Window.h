//
//  Window.h
//
//  Created by Hongyong Xiao on 11/15/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#pragma once

#include "../IWindow.h"
#include "Constants.h"


typedef int LPARAM;

class Cursor;

class Window : public IWindow {
public:
    Window();
    virtual ~Window();

    virtual bool createForSkin(cstr_t szClassName, cstr_t szCaption, int x, int y, int nWidth, int nHeight, Window *pWndParent, bool bToolWindow = true, bool bTopmost = false, bool bVisible = true);

    virtual void destroy();

    void postDestroy();

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

    void setMinSize(uint32_t width, uint32_t height);
    void setMaxSize(uint32_t width, uint32_t height);

    bool setTimer(uint32_t nTimerId, uint32_t nElapse);
    void killTimer(uint32_t nTimerId);

    string getTitle();
    void setTitle(cstr_t szText);

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

    bool isSameWnd(Window *pWnd);

    bool isChild();

    bool isMouseCaptured();

    bool isIconic();
    bool isZoomed();

    bool isWindow();

    bool isValid();

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

    // 直接设置窗口 frame，不做 moveWindow 中 "origin.y 不变时保持上边缘不动" 的调整
    void setFrameExact(int x, int y, int nWidth, int nHeight, bool bRepaint = true);

    int messageOut(cstr_t lpText, uint32_t uType = MB_ICONINFORMATION | MB_OK, cstr_t lpCaption = nullptr);

    bool replaceChildPos(int nIDChildSrcPos, Window *pChildNew);

    void postUserMessage(int nMessageID, LPARAM param);

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

    bool                        m_bMouseCaptured;

    float                       m_scaleFactor;

    // For Mac
    struct WindowHandleHolder *getWindowHandleHolder() const { return m_handleHolder; }
    void setHasShadow(bool hasShadow);

#ifdef _MAC_OS
    // Mac has a known bug: when click system menubar button, the floating
    // lyrics window will become active.
    bool                        m_ignoreOneActivate = false;
#endif

protected:
    WndSizeMode                 m_WndSizeMode;
    Window                      *m_parent = nullptr;

    struct WindowHandleHolder   *m_handleHolder = nullptr;

};
