/********************************************************************
Created  :    2001年7月21日 20:26:35
FileName :    UIWndSizing.cpp
Author   :    xhy

Purpose  :    调整窗口的大小
*********************************************************************/

#include "WindowTypes.h"
#include "WindowLib.h"
#include "WndResizer.h"


#define SIZING_REGION        5        // 调整大小的范围



WndResizer::WndResizer() {
    m_nMincx = 0;
    m_nMincy = 0;

    m_fixedWidth = false;
    m_fixedHeight = false;

    m_bResizing = false;
    m_ResizeDirection = 0;

    m_bResizingAutoCloseto = true;

    m_pWnd = nullptr;
    m_ptOld.x = m_ptOld.y = 0;
}

WndResizer::~WndResizer() {

}

void WndResizer::onMouseMessage(uint32_t fwKeys, CPoint pt) {
    if (m_fixedWidth && m_fixedHeight) {
        return;
    }
    if (m_pWnd->isZoomed()) {
        return;
    }

    if (m_bResizing) {
        setCursor();
    } else {
        // Need to do Hit test.
        CRect rc;
        m_pWnd->getWindowRect(&rc);

        CPoint point = getCursorPos();

        if (rc.ptInRect(point)) {
            m_pWnd->screenToClient(rc);
            m_ResizeDirection = hitTestResizeArea(pt, rc);
            setCursor();
        }
    }

    if (fwKeys & MK_LBUTTON) {
        if (m_bResizing) {
            onSizing(fwKeys, pt);
        } else {
            beginSize(fwKeys, pt);
        }
    } else if (m_bResizing) {
        endSizing(fwKeys, pt);

        m_pWnd->setCursor(&m_cursorArrow);
    }
}

void WndResizer::init(Window *pWnd) {
    m_pWnd = pWnd;

    m_cursorArrow.loadStdCursor(Cursor::C_ARROW);
    m_cursorNWSE.loadStdCursor(Cursor::C_SIZENWSE);
    m_cursorNESW.loadStdCursor(Cursor::C_SIZENESW);
    m_cursorWE.loadStdCursor(Cursor::C_SIZEWE);
    m_cursorNS.loadStdCursor(Cursor::C_SIZENS);
}

bool WndResizer::isSizing() {
    return m_bResizing;
}

void WndResizer::beginSize(uint32_t fwKeys, CPoint &pt) {
    // 记住鼠标位置
    m_ptOld = getCursorPos();

    // 记住窗口位置
    m_pWnd->getWindowRect(&m_rcResizing);

    if (m_ResizeDirection != 0) {
        m_bResizing = true;
    }

    m_pWnd->setCapture();
}

void WndResizer::onSizing(uint32_t fwKeys, CPoint &pt) {
    // 正在调整大小
    int nOffx;
    int nOffy;

    pt = getCursorPos();

    // 只有鼠标移动了才调整大小
    if ((pt.x == m_ptOld.x) && (pt.y == m_ptOld.y)) {
        return;
    }

    nOffx = pt.x - m_ptOld.x; // x 偏移
    nOffy = pt.y - m_ptOld.y; // y 偏移

    // 限制最小的宽度
    if (m_ResizeDirection & RD_LEFT) {
        if (m_rcResizing.right - (m_rcResizing.left + nOffx) <= m_nMincx) {
            nOffx = m_rcResizing.width() - m_nMincx;
        }

        m_rcResizing.left += nOffx;
    }

    // 限制最小的高度
    if (m_ResizeDirection & RD_TOP) {
#ifdef _WIN32
        if (m_rcResizing.bottom - (m_rcResizing.top + nOffy) <= m_nMincy) {
            nOffy = m_rcResizing.height() - m_nMincy;
        }

        m_rcResizing.top += nOffy;
#else
        // macOS 的 y 轴向上，CRect.top 实际是窗口的下边缘(origin.y)，
        // RD_TOP 拖动的是上边缘，对应 CRect.bottom。
        if (m_rcResizing.height() + nOffy <= m_nMincy) {
            nOffy = m_nMincy - m_rcResizing.height();
        }

        m_rcResizing.bottom += nOffy;
#endif
    }

    // 限制最小的宽度
    if (m_ResizeDirection & RD_RIGHT) {
        if ((m_rcResizing.right + nOffx) - m_rcResizing.left <= m_nMincx) {
            nOffx = m_nMincx - m_rcResizing.width();
        }

        m_rcResizing.right += nOffx;
    }

    // 限制最小的高度
    if (m_ResizeDirection & RD_BOTTOM) {
#ifdef _WIN32
        if ((m_rcResizing.bottom + nOffy) - m_rcResizing.top <= m_nMincy) {
            nOffy = m_nMincy - m_rcResizing.height();
        }

        m_rcResizing.bottom += nOffy;
#else
        // macOS: RD_BOTTOM 拖动的是下边缘，对应 CRect.top(origin.y)，
        // 拖动后新高度为 height - nOffy。
        if (m_rcResizing.height() - nOffy <= m_nMincy) {
            nOffy = m_rcResizing.height() - m_nMincy;
        }

        m_rcResizing.top += nOffy;
#endif
    }

    // 保存上一次鼠标的位置
    if (nOffx != 0) {
        m_ptOld.x = pt.x;
    }
    if (nOffy != 0) {
        m_ptOld.y = pt.y;
    }

    // 如果调整窗口时自动靠近则通过改变 nOffx, nOffy来调整
    if (m_bResizingAutoCloseto) {
        autoCloseToWindows(nOffx, nOffy);
        m_ptOld.x += nOffx;
        m_ptOld.y += nOffy;
    }

    CRect rc;
    m_pWnd->getWindowRect(&rc);
    if (!rc.equal(m_rcResizing)) {
#ifdef _WIN32
        m_pWnd->moveWindowSafely(m_rcResizing.left, m_rcResizing.top, m_rcResizing.width(), m_rcResizing.height(), true);
#else
        // Window::moveWindow 在 origin.y 不变时会保持窗口上边缘不动(模拟 Windows 语义)，
        // 与拖动上边缘的意图冲突，这里直接设置最终的 frame。
        m_pWnd->setFrameExact(m_rcResizing.left, m_rcResizing.top, m_rcResizing.width(), m_rcResizing.height(), true);
#endif
    }
}

void WndResizer::setResizeArea(int nIDArea, uint32_t resizeDirection, CRect &rcResizeArea) {
    ResizeArea ra;
    ListResizeArea::iterator it;

    if (nIDArea != -1) {
        for (it = m_listResizeArea.begin(); it != m_listResizeArea.end(); ++it) {
            ResizeArea &t = *it;
            if (t.nID == nIDArea) {
                t.resizeDirection = resizeDirection;
                t.rcResizeArea = rcResizeArea;
                return;
            }
        }
    }

    ra.nID = nIDArea;
    ra.resizeDirection = resizeDirection;
    ra.rcResizeArea = rcResizeArea;
    m_listResizeArea.push_back(ra);
}

void WndResizer::removeResizeArea(int nIDArea) {
    ListResizeArea::iterator it;

    if (nIDArea != -1) {
        for (it = m_listResizeArea.begin(); it != m_listResizeArea.end(); ++it) {
            ResizeArea &t = *it;
            if (t.nID == nIDArea) {
                m_listResizeArea.erase(it);
                return;
            }
        }
    }
}

// 结束调整窗口大小
void WndResizer::endSizing(uint32_t fwKeys, CPoint &pt) {
    m_bResizing = false;

    m_ResizeDirection = 0;

    m_pWnd->releaseCapture();
}

void WndResizer::autoCloseToWindows(int &nOffx, int &nOffy) {
    nOffx = 0;
    nOffy = 0;
}

uint32_t WndResizer::hitTestResizeArea(CPoint &pt, const CRect &rc) {
    ListResizeArea::iterator it;

    uint32_t resizeDirection = 0;

    for (it = m_listResizeArea.begin(); it != m_listResizeArea.end(); ++it) {
        ResizeArea &t = *it;
        if (t.rcResizeArea.ptInRect(pt)) {
            resizeDirection |= t.resizeDirection;
        }
    }

    if (pt.x <= rc.left + SIZING_REGION) {
        resizeDirection |= RD_LEFT;
    } else if (pt.x >= rc.right - SIZING_REGION) {
        resizeDirection |= RD_RIGHT;
    }

    if (pt.y <= rc.top + SIZING_REGION) {
        resizeDirection |= RD_TOP;
    } else if (pt.y >= rc.bottom - SIZING_REGION) {
        resizeDirection |= RD_BOTTOM;
    }

    if (pt.x > rc.right - SIZING_REGION * 2 && pt.y > rc.bottom - SIZING_REGION * 2) {
        resizeDirection |= RD_RIGHT | RD_BOTTOM;
    }

    // remove fixed direction
    if (m_fixedWidth) {
        resizeDirection &= ~ (RD_LEFT | RD_RIGHT);
    }
    if (m_fixedHeight) {
        resizeDirection &= ~ (RD_TOP | RD_BOTTOM);
    }

    // remove exclude area
    if (resizeDirection & RD_LEFT) {
        resizeDirection &= ~RD_RIGHT;
    }
    if (resizeDirection & RD_TOP) {
        resizeDirection &= ~RD_BOTTOM;
    }

    return resizeDirection;
}

void WndResizer::setCursor() {
    if (isFlagSet(m_ResizeDirection, RD_LEFT | RD_TOP)
        || isFlagSet(m_ResizeDirection, RD_RIGHT | RD_BOTTOM)) {
        m_pWnd->setCursor(&m_cursorNWSE);
    } else if (isFlagSet(m_ResizeDirection, RD_LEFT | RD_BOTTOM)
        || isFlagSet(m_ResizeDirection, RD_RIGHT | RD_TOP)) {
        m_pWnd->setCursor(&m_cursorNESW);
    } else if (m_ResizeDirection & (RD_LEFT | RD_RIGHT)) {
        m_pWnd->setCursor(&m_cursorWE);
    } else if (m_ResizeDirection & (RD_TOP | RD_BOTTOM)) {
        m_pWnd->setCursor(&m_cursorNS);
    }
}
