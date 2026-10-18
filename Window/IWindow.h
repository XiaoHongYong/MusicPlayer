//
//  IWindow.h
//  MusicPlayer
//
//  Created by HongyongXiao on 2021/11/13.
//

#pragma once

#ifndef IWindow_hpp
#define IWindow_hpp

#include "WindowTypes.h"
#include "IScrollBar.h"


class Cursor;

void showInFinder(cstr_t filename);
void showInFinder(const VecStrings &files);
float getScreenScaleFactor();

// Windows：进程启动时调用，启用 Per-Monitor DPI awareness（须在创建窗口前）
void enableProcessDpiAwareness();

//
// The Window size state definition
//
enum WndSizeMode {
    WndSizeMode_Minimized,
    WndSizeMode_Maximized,
    WndSizeMode_Normal,
};

class IWindow {
public:
    //
    // Messages
    //
    IWindow();
    virtual ~IWindow();

    virtual void onActivate(bool bActived) { }
    virtual bool onClose() { return true; }
    virtual void onCommand(uint32_t id) { }
    virtual void onContexMenu(int xPos, int yPos) { }
    virtual void onCreate();
    virtual void onDestroy() { }

    virtual bool onKeyDown(uint32_t nChar, uint32_t nFlags) { return false; }
    virtual bool onKeyUp(uint32_t nChar, uint32_t nFlags) { return false; }
    virtual void onChar(uint32_t nChar) { }
    virtual void onLButtonUp(uint32_t nFlags, CPoint point) { }
    virtual void onLButtonDown(uint32_t nFlags, CPoint point) { }
    virtual void onLButtonDblClk(uint32_t nFlags, CPoint point) { }
    virtual void onRButtonUp( uint32_t nFlags, CPoint point ) { }
    virtual void onRButtonDown( uint32_t nFlags, CPoint point ) { }
    virtual void onMouseMove(CPoint point) { }
    virtual void onMouseDrag(uint32_t nFlags, CPoint point) { }
    virtual void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) { }

    virtual void onMagnify(float magnification) { }

    virtual void onMove(int x, int y) { }

    virtual void onPaint(CRawGraph *surface, CRect *rc) { }

    virtual void onKillFocus() { }
    virtual void onSetFocus() { }

    virtual void onSize(int cx, int cy) { }
    virtual void onSizeModeChanged(WndSizeMode sizeMode) { }

    virtual void onTimer(uint32_t nIDEvent) { }

    virtual void onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) { }
    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) { }

    virtual void onScaleFactorChanged(float scaleFactor);

    // Language Changed Notification message
    virtual void onLanguageChanged() { }

    virtual float getScaleFactor() = 0;

    virtual void setCursor(Cursor *cursor) = 0;

public:
    CRawGraph *getMemGraphics();

    bool recreateMemGraphics();

    void onResized(int width, int height);

    virtual WindowHandle getHandle() = 0;

protected:
    CRawGraph                   *m_pmemGraph;
    CSize                       m_wndSize;

};

#endif // IWindow_hpp
