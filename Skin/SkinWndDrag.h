#ifndef Skin_SkinWndDrag_h
#define Skin_SkinWndDrag_h

#pragma once

#include "../Window/WindowLib.h"


class ISkinWndDragHost {
public:
    virtual void getWndDragAutoCloseTo(vector<Window *> &vWnd) = 0;
    virtual void getWndDragTrackMove(vector<Window *> &vWnd) = 0;
    // 拖拽结束（吸附落稳后），用于刷新一体边框等
    virtual void onWndDragEnded() { }

};

class CSkinWndDrag : public WndDrag {
public:
    CSkinWndDrag();
    virtual ~CSkinWndDrag();

    void init(Window *pWnd, ISkinWndDragHost *pSkinWndDragHost);

    void onDrag(uint32_t fwKeys, CPoint pt);

    virtual bool autoCloseToWindows(int &nOffx, int &nOffy, bool bMoveWindow);

protected:
    void setWindowPosSafely(int xOld, int yOld, int nOffsetx, int nOffsety);

protected:
    ISkinWndDragHost            *m_pSkinWndDragHost;
    vector<Window               *>    m_vStickedWnds;

};

class CSkinWndResizer : public WndResizer {
public:
    CSkinWndResizer();
    virtual ~CSkinWndResizer();

    void init(Window *pWnd, ISkinWndDragHost *pSkinWndDragHost);

    virtual void autoCloseToWindows(int &nOffx, int &nOffy);

protected:
    ISkinWndDragHost            *m_pSkinWndDragHost;

};

#endif // !defined(Skin_SkinWndDrag_h)
