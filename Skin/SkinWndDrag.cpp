#include "SkinTypes.h"
#include "../Window/WindowLib.h"
#include "SkinWndDrag.h"


//////////////////////////////////////////////////////////////////////

bool isStickedWindows(Window *pWnd1, Window *pWnd2);
void rectCloseToRect(CRect &rcs, CRect &rcd, int &offx, int &offy, int nCloseExt);
void rectCloseToWindow(CRect &rcs, Window *pWnd, int nCloseExt, int &offx, int &offy);

#ifdef _WIN32
void getTaskBarRect(CRect &rc) {
    ::GetWindowRect(::FindWindowA("Shell_TrayWnd",nullptr), &rc);
}
#endif

CSkinWndDrag::CSkinWndDrag() {

}

CSkinWndDrag::~CSkinWndDrag() {

}

void CSkinWndDrag::init(Window *pWnd, ISkinWndDragHost *pSkinWndDragHost) {
    WndDrag::init(pWnd);

    m_pSkinWndDragHost = pSkinWndDragHost;
}

typedef vector<Window *> V_WNDS;

static void vWnds_Add(V_WNDS &vWnds, Window *pWnd) {
    for (int i = 0; i < (int)vWnds.size(); i++) {
        if (vWnds[i]->isSameWnd(pWnd)) {
            return;
        }
    }
    vWnds.push_back(pWnd);
}

static void vWnds_Remove(V_WNDS &vWnds, Window *pWnd) {
    for (int i = 0; i < (int)vWnds.size(); i++) {
        if (vWnds[i]->isSameWnd(pWnd)) {
            vWnds.erase(vWnds.begin() + i);
            return;
        }
    }
}

void getStickedWindows(Window *pMainWnd, V_WNDS &vAllWnds, V_WNDS &vStickedWnds) {
    bool bFoundSticked;

    vStickedWnds.push_back(pMainWnd);

    // find all sticked windows, save in vStickedWnds
    do {
        bFoundSticked = false;

        for (int i = 0; i < (int)vAllWnds.size(); i++) {
            Window *pWnd = vAllWnds[i];

            for (int n = 0; n < (int)vStickedWnds.size(); n++) {
                if (!pWnd->isSameWnd(vStickedWnds[n]) && isStickedWindows(pWnd, vStickedWnds[n])) {
                    vWnds_Add(vStickedWnds, pWnd);
                    vAllWnds.erase(vAllWnds.begin() + i);
                    i--;
                    bFoundSticked = true;
                    break;
                }
            }
        }
    }
    while (bFoundSticked);
}

// input:
//        nOffx, nOffy: offset of window by mouse drag
// output:
//        nOffx, nOffy: offset of window by auto close to
bool CSkinWndDrag::autoCloseToWindows(int &nOffx, int &nOffy, bool bMoveWindow) {
    Window *pWnd = nullptr;
    V_WNDS vWnds;
    vector<CRect> vRcSticked, vRcToClosedTo;
    CRect rc;

    if (m_pSkinWndDragHost) {
        m_pSkinWndDragHost->getWndDragAutoCloseTo(vWnds);
    }

    // enum all windows that should auto close to
    for (size_t i = 0; i < m_vWndCloseTo.size(); i ++) {
        if (m_vWndCloseTo[i].pWnd) {
            pWnd = m_vWndCloseTo[i].pWnd;
        } else {
            const char *szClass = m_vWndCloseTo[i].strClass.c_str();
            if (isEmptyString(szClass)) {
                szClass = nullptr;
            }
            const char *szWnd = m_vWndCloseTo[i].strWndName.c_str();
            if (isEmptyString(szWnd)) {
                szWnd = nullptr;
            }
            pWnd = findWindow(szClass, szWnd);
        }
        if (pWnd) {
            vWnds_Add(vWnds, pWnd);
        }
    }

    // offset the rect of all sticked windows.
    for (size_t i = 0; i < m_vStickedWnds.size(); i++) {
        vWnds_Remove(vWnds, m_vStickedWnds[i]);

        m_vStickedWnds[i]->getWindowRect(&rc);
        rc.offsetRect(nOffx, nOffy);
        vRcSticked.push_back(rc);
    }

    // make the sticked windows auto be close to vWnds.
    int nOffXTemp, nOffYTemp;
    nOffx = 0; nOffy = 0;
    for (size_t i = 0; i < vWnds.size(); i ++) {
        pWnd = vWnds[i];
        for (int k = 0; k < (int)m_vStickedWnds.size(); k++) {
            Window *pWndToMove = m_vStickedWnds[k];
            if (pWnd && !pWnd->isSameWnd(pWndToMove)) {
                rectCloseToWindow(vRcSticked[k], pWnd, 10, nOffXTemp, nOffYTemp);

                if (nOffXTemp != 0) {
                    nOffx = nOffXTemp;
                }
                if (nOffYTemp != 0) {
                    nOffy = nOffYTemp;
                }

                if (nOffx != 0 && nOffy != 0) {
                    i = vWnds.size();
                    break;
                }
            }
        }
    }

    if (nOffx == 0 && nOffy == 0) {
        // close to display edge
        getMonitorRestrictRect(vRcSticked[0], rc);
        vRcToClosedTo.push_back(rc);

#ifdef _WIN32
        // close to taskbar
        getTaskBarRect(rc);
        vRcToClosedTo.push_back(rc);
#endif

        // make the sticked windows auto be close to vRcToClosedTo.
        for (int i =0; i < (int)vRcToClosedTo.size(); i++) {
            for (int k = 0; k < (int)m_vStickedWnds.size(); k++) {
                rectCloseToRect(vRcSticked[k], vRcToClosedTo[i], nOffXTemp, nOffYTemp, 10);

                if (nOffXTemp != 0) {
                    nOffx = nOffXTemp;
                }
                if (nOffYTemp != 0) {
                    nOffy = nOffYTemp;
                }

                if (nOffx != 0 && nOffy != 0) {
                    i = (int)vRcToClosedTo.size();
                    break;
                }
            }
        }
    }

    // vRcSticked[0] is the rect of m_pWnd
    vRcSticked[0].offsetRect(nOffx, nOffy);

    m_pWnd->getWindowRect(&rc);
    if (rc.equal(vRcSticked[0])) {
        // no move
    } else {
        if (bMoveWindow) {
            // m_vStickedWnds[0] is m_pWnd, and vRcSticked[0] is the new pos now.
            m_pWnd->setWindowPosSafely(vRcSticked[0].left, vRcSticked[0].top);

            // move  all sticked window from m_vStickedWnds[1]
            for (int k = 1; k < (int)m_vStickedWnds.size(); k++) {
                Window *pWndToMove = m_vStickedWnds[k];

                pWndToMove->setWindowPosSafely(vRcSticked[k].left + nOffx, vRcSticked[k].top + nOffy);
            }
        }

        return true;
    }

    return false;

}

void CSkinWndDrag::onDrag(uint32_t fwKeys, CPoint pt) {
    bool bDragStatOld = m_bDragWnd;

    WndDrag::onDrag(fwKeys, pt);

    if (bDragStatOld != m_bDragWnd) {
        if (m_bDragWnd) {
            // begin drag
            V_WNDS vTrackMoveWnds;

            if (m_pSkinWndDragHost) {
                m_pSkinWndDragHost->getWndDragTrackMove(vTrackMoveWnds);
            }

            // get all windows sticked with m_pWnd
            getStickedWindows(m_pWnd, vTrackMoveWnds, m_vStickedWnds);
        } else {
            // end drag
            // clear drag status
            m_vStickedWnds.clear();

            if (m_pSkinWndDragHost) {
                m_pSkinWndDragHost->onWndDragEnded();
            }
        }
    }
}

void CSkinWndDrag::setWindowPosSafely(int xOld, int yOld, int nOffsetx, int nOffsety) {
    WndDrag::setWindowPosSafely(xOld, yOld, nOffsetx, nOffsety);
}

//////////////////////////////////////////////////////////////////////////
//

CSkinWndResizer::CSkinWndResizer() {
    m_pSkinWndDragHost = nullptr;
}

CSkinWndResizer::~CSkinWndResizer() {

}

void CSkinWndResizer::init(Window *pWnd, ISkinWndDragHost *pSkinWndDragHost) {
    WndResizer::init(pWnd);

    m_pSkinWndDragHost = pSkinWndDragHost;
}

void vertLineCloseToRect(int srcY1, int srcY2, int srcX, CRect &rcd, int &offx, int nCloseExt);
void horzLineCloseToRect(int srcX1, int srcX2, int srcY, CRect &rcd, int &offy, int nCloseExt);


// input:
//        nOffx, nOffy: offset of window by mouse drag
// output:
//        nOffx, nOffy: offset of window by auto close to
void CSkinWndResizer::autoCloseToWindows(int &nOffx, int &nOffy) {
    V_WNDS vWnds;
    uint32_t i;
    Window *pWnd;
    CRect rcWnd, rc;
    int nOffXTemp, nOffYTemp;
    vector<CRect> vRcToClosedTo;

    nOffx = 0; nOffy = 0;

    if (!m_pSkinWndDragHost) {
        return;
    }

    m_pSkinWndDragHost->getWndDragAutoCloseTo(vWnds);

    // close to display edge
    m_pWnd->getWindowRect(&rcWnd);
    if (getMonitorRestrictRect(rcWnd, rc)) {
        vRcToClosedTo.push_back(rc);
    }

#ifdef _WIN32
    // close to taskbar
    getTaskBarRect(rc);
    vRcToClosedTo.push_back(rc);
#endif

    for (i = 0; i < vWnds.size(); i++) {
        pWnd = vWnds[i];

        if (pWnd->isSameWnd(m_pWnd) || !pWnd->isVisible() || pWnd->isIconic()) {
            continue;
        }

        pWnd->getWindowRect(&rc);
        vRcToClosedTo.push_back(rc);
    }

    if (m_ResizeDirection & (RD_BOTTOM | RD_TOP)) {
        // top or bottom line close to vWnds
        // macOS: CRect.top 是视觉下边，CRect.bottom 是视觉上边，与 onSizing 一致
        int y;

        if (m_ResizeDirection & RD_TOP) {
#ifdef _WIN32
            y = m_rcResizing.top;
#else
            y = m_rcResizing.bottom;
#endif
        } else {
#ifdef _WIN32
            y = m_rcResizing.bottom;
#else
            y = m_rcResizing.top;
#endif
        }

        for (i = 0; i < vRcToClosedTo.size(); i++) {
            horzLineCloseToRect(m_rcResizing.left, m_rcResizing.right, y, vRcToClosedTo[i], nOffYTemp, 10);
            if (nOffYTemp != 0) {
                nOffy = nOffYTemp;
            }
            if (nOffy != 0) {
                if (m_ResizeDirection & RD_TOP) {
#ifdef _WIN32
                    m_rcResizing.top += nOffy;
#else
                    m_rcResizing.bottom += nOffy;
#endif
                } else {
#ifdef _WIN32
                    m_rcResizing.bottom += nOffy;
#else
                    m_rcResizing.top += nOffy;
#endif
                }
                break;
            }
        }
    }

    if (m_ResizeDirection & (RD_LEFT | RD_RIGHT)) {
        // top or bottom line close to vWnds
        int x;

        if (m_ResizeDirection & RD_LEFT) {
            x = m_rcResizing.left;
        } else {
            x = m_rcResizing.right;
        }

        for (i = 0; i < vRcToClosedTo.size(); i++) {
            vertLineCloseToRect(m_rcResizing.top, m_rcResizing.bottom, x, vRcToClosedTo[i], nOffXTemp, 10);
            if (nOffXTemp != 0) {
                nOffx = nOffXTemp;
            }
            if (nOffx != 0) {
                if (m_ResizeDirection & RD_LEFT) {
                    m_rcResizing.left += nOffx;
                } else {
                    m_rcResizing.right += nOffx;
                }
                break;
            }
        }
    }
}
