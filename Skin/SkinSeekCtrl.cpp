#include "SkinTypes.h"
#include "Skin.h"
#include "SkinSeekCtrl.h"


// set classname
UIOBJECT_CLASS_NAME_IMP(CSkinSeekCtrl, "SeekCtrl");

CSkinSeekCtrl::CSkinSeekCtrl() {
    // 基本设置
    m_nWidthEnd = 17; // 上、下按钮的高度
    m_nWidthThumb = 41; // 滚动条的高度
    m_nPosThumb = 0; // 滚动条的位置(上面按钮的下方 －> thumb的上方的距离)

    // 虚拟滚动位置
    m_nVirtualMin = 0; // 最小的值
    m_nVirtualMax = 10; // 最大的值
    m_nVirtualLine = 1;
    m_nVirtualPage = 10;
    m_nVirtualCurPos = m_nVirtualMin;

    m_msgNeed = UO_MSG_WANT_LBUTTON | UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_MOUSEWHEEL;

    m_nXCursorToThumbBeg = 0;
    m_pushDownPos = PUSH_DOWN_NONE;
    m_cursorPosLatest = PUSH_DOWN_NONE;

    m_pScrollNotify = nullptr;
}

CSkinSeekCtrl::~CSkinSeekCtrl() {
}

int CSkinSeekCtrl::takePendingSeekPos() {
    int pos = m_pendingSeekPos;
    m_pendingSeekPos = -1;
    return pos;
}

bool CSkinSeekCtrl::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (nChar == VK_UP) {
        setScrollPos(getScrollPos() - m_nVirtualLine);
    } else if (nChar == VK_DOWN) {
        setScrollPos(getScrollPos() + m_nVirtualLine);
    } else if (nChar == VK_LEFT || nChar == VK_PRIOR) {
        setScrollPos(getScrollPos() - m_nVirtualPage);
    } else if (nChar == VK_RIGHT || nChar == VK_NEXT) {
        setScrollPos(getScrollPos() + m_nVirtualPage);
    } else {
        return false;
    }

    onVScroll(SB_THUMBPOSITION);
    return true;
}

bool CSkinSeekCtrl::onLButtonDown(uint32_t nFlags, CPoint point) {
    assert(m_pSkin);

    m_pushDownPos = getPushDownPos(point);
    m_cursorPosLatest = m_pushDownPos;

    switch (m_pushDownPos) {
    case PUSH_DOWN_THUMB:
        // 滚动条的拖动box(thumb)
        thumbOnLButtonDown(nFlags, point);
        break;
    case PUSH_DOWN_TOPTRACK:
    case PUSH_DOWN_BOTTOMTRACK:
        // 滚动条的空白位置，进行seek操作
        trackOnLButtonDown(nFlags, point);
        break;
    default:
        return false;
    }

    return true;
}

void CSkinSeekCtrl::thumbOnLButtonDown(uint32_t nFlags, CPoint point) {
    // 捕捉鼠标输入
    m_pSkin->setCaptureMouse(this);

    m_nXCursorToThumbBeg = point.x - (m_rcObj.left + m_nPosThumb);

    invalidate();

    notifyEvent(CSkinSeekCtrlEventNotify::C_BEG_DRAG);
}

void CSkinSeekCtrl::trackOnLButtonDown(uint32_t nFlags, CPoint point) {
    // 捕捉鼠标输入
    m_pSkin->setCaptureMouse(this);

    m_nPosThumb = point.x - m_rcObj.left;
    if (m_pushDownPos == PUSH_DOWN_BOTTOMTRACK) {
        m_nPosThumb -= m_nWidthThumb / 2;
    } else {
        m_nPosThumb -= m_nWidthThumb / 2;
    }

    if (m_nPosThumb < 0) {
        m_nPosThumb = 0;
    } else if (m_nPosThumb > getTrackRealWidth()) {
        m_nPosThumb = getTrackRealWidth();
    }

    m_nVirtualCurPos = objectPosToVirtualPos(m_nPosThumb);

    m_cursorPosLatest = m_pushDownPos = PUSH_DOWN_THUMB;

    m_nXCursorToThumbBeg = point.x - (m_rcObj.left + m_nPosThumb);

    invalidate();

    notifyEvent(CSkinSeekCtrlEventNotify::C_BEG_DRAG);

    onVScroll(SB_THUMBPOSITION);
}

bool CSkinSeekCtrl::onLButtonUp(uint32_t nFlags, CPoint point) {
    // DBG_LOG0("onLButtonUp ");
    switch (m_pushDownPos) {
    case PUSH_DOWN_THUMB:
        {
            // 滚动条的拖动box(thumb)
            m_pSkin->releaseCaptureMouse(this);

            onVScroll(SB_THUMBPOSITION);
            // 先缓存目标，再异步 post：避免 ID_SEEK 执行前被 POS 定时器写回旧位置
            setPendingSeekPos(m_nVirtualCurPos);
            m_pSkin->postCustomCommandMsg(m_id);

            notifyEvent(CSkinSeekCtrlEventNotify::C_END_DRAG);
        }
        break;
    default:
        break;
    }

    m_pushDownPos = PUSH_DOWN_NONE;

    invalidate();

    return true;
}

void CSkinSeekCtrl::onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) {
    if (nMkeys == 0) {
        int nVirtualPosOld;

        nVirtualPosOld = m_nVirtualCurPos;

        if (nWheelDistance > 0) {
            if (m_nVirtualCurPos > m_nVirtualMin) {
                m_nVirtualCurPos -= m_nVirtualLine;
            }
            if (m_nVirtualCurPos < m_nVirtualMin) {
                m_nVirtualCurPos = m_nVirtualMin;
            }
        } else {
            if (m_nVirtualCurPos < m_nVirtualMax) {
                m_nVirtualCurPos += m_nVirtualLine;
            }
            if (m_nVirtualCurPos > m_nVirtualMax) {
                m_nVirtualCurPos = m_nVirtualMax;
            }
        }

        if (m_nVirtualCurPos == nVirtualPosOld) {
            return;
        }

        m_nPosThumb = virtualPosToObjectPos(m_nVirtualCurPos);

        onVScroll(nWheelDistance > 0 ? SB_LINEDOWN : SB_LINEUP);
        invalidate();

        setPendingSeekPos(m_nVirtualCurPos);
        m_pSkin->postCustomCommandMsg(m_id);
    }
}

bool CSkinSeekCtrl::onMouseDrag(CPoint point) {
    switch (m_pushDownPos) {
    case PUSH_DOWN_THUMB:
        thumbOnMouseMove(point);
        break;
    default:
        break;
    }

    return true;
}

bool CSkinSeekCtrl::onMouseMove(CPoint point) {
    switch (m_pushDownPos) {
    case PUSH_DOWN_NONE:
        {
            PUSH_DOWN_POS CursorPosNew;

            CursorPosNew = getPushDownPos(point);

            if (CursorPosNew != m_cursorPosLatest) {
                m_cursorPosLatest = CursorPosNew;
                invalidate();
            }
        }
        break;
    default:
        break;
    }

    return true;
}

void CSkinSeekCtrl::onMouseEnter(CPoint point) {
    invalidate();
}

void CSkinSeekCtrl::onMouseLeave(CPoint point) {
    invalidate();
}

void CSkinSeekCtrl::thumbOnMouseMove(CPoint point) {
    int nVirtualPosOld;
    int nPosThumbOld;

    nVirtualPosOld = m_nVirtualCurPos;
    nPosThumbOld = m_nPosThumb;

    m_nPosThumb = point.x - m_rcObj.left - m_nXCursorToThumbBeg;

    if (m_nPosThumb < 0) {
        m_nPosThumb = 0;
    } else if (m_nPosThumb > getTrackRealWidth()) {
        m_nPosThumb = getTrackRealWidth();
    }

    // // DBG_LOG1("m_nPosThumb: %d", m_nPosThumb);

    m_nVirtualCurPos = objectPosToVirtualPos(m_nPosThumb);

    if (m_nVirtualCurPos == nVirtualPosOld) {
        if (m_nPosThumb != nPosThumbOld) {
            invalidate();
        }
        return;
    }

    onVScroll(SB_THUMBPOSITION);

    invalidate();
}

void CSkinSeekCtrl::draw(CRawGraph *canvas) {
    int nTrackHeight = m_imgTrackNormal.height();
    int nThumbHeight = m_imgThumb.height() / 3;

    int x, y;
    x = m_rcObj.left;
    y = (m_rcObj.top + m_rcObj.bottom - nTrackHeight) / 2;

    if (m_imgTrackPlayed.isValid()) {
        // draw left
        CRawGraph::CClipBoxAutoRecovery clipAR(canvas);

        canvas->setClipBoundBox(CRect(x, y, x + m_nPosThumb + m_nWidthThumb / 2, y + nTrackHeight));
        m_imgTrackPlayed.xScaleBlt(canvas, x, y,
            m_rcObj.width(), nTrackHeight,
            m_nWidthEnd, m_imgTrackPlayed.width() - m_nWidthEnd,
            true, BPM_BLEND);
    }

    if (m_imgTrackNormal.isValid()) {
        // draw right
        CRawGraph::CClipBoxAutoRecovery clipAR(canvas);

        canvas->setClipBoundBox(CRect(x + m_nPosThumb + m_nWidthThumb / 2, y, m_rcObj.right, y + nTrackHeight));
        m_imgTrackNormal.xScaleBlt(canvas, x, y,
            m_rcObj.width(), nTrackHeight,
            m_nWidthEnd, m_imgTrackNormal.width() - m_nWidthEnd,
            true, BPM_BLEND);
    }

    // draw thumb
    if (m_imgThumb.isValid()) {
        int yThumbSrc = 0;
        if (m_pushDownPos != PUSH_DOWN_THUMB) {
            if (m_isMouseIn) {
                yThumbSrc = nThumbHeight; // Hover on thumb.
            }
        } else {
            if (m_cursorPosLatest == PUSH_DOWN_THUMB) {
                yThumbSrc = nThumbHeight * 2; // push down on thumb.
            }
        }

        m_imgThumb.blt(canvas, m_rcObj.left + m_nPosThumb, (m_rcObj.top + m_rcObj.bottom - nThumbHeight) / 2,
            m_imgThumb.m_cx, nThumbHeight,
            0, yThumbSrc, BPM_BLEND);
    }
}

bool CSkinSeekCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (strcasecmp(szProperty, "EndWidth") == 0) {
        m_nWidthEnd = atoi(szValue);
    } else if (isPropertyName(szProperty, "ImageTrack")) {
        m_strImageTrack = szValue;

        m_imgTrackNormal.loadFromSRM(m_pSkin, szValue);
        m_imgTrackPlayed.loadFromSRM(m_pSkin, szValue);

        m_imgTrackNormal.m_cy = m_imgTrackPlayed.m_cy = m_imgTrackNormal.m_cy / 2;
        m_imgTrackPlayed.m_y = m_imgTrackNormal.m_cy;
    } else if (isPropertyName(szProperty, "ImageThumb")) {
        m_strImageThumb = szValue;

        m_imgThumb.loadFromSRM(m_pSkin, szValue);
        m_nWidthThumb = m_imgThumb.width();
    } else {
        return false;
    }

    return true;
}

#ifdef _SKIN_EDITOR_
void CSkinSeekCtrl::enumProperties(CUIObjProperties &listProperties) {
    CUIObject::enumProperties(listProperties);

    listProperties.addPropInt("EndWidth", m_nWidthEnd);
    listProperties.addPropImageFile(SZ_PN_IMAGE, m_strBmpFile.c_str());
}
#endif // _SKIN_EDITOR_

// COMMENT:
//        设置新的滚动位置
// RETURN:
//        返回设置以前的滚动位置的值
int CSkinSeekCtrl::setScrollPos(int nPos, bool bRedraw) {
    int nPosOld, nPosThumbOld;

    // 拖动中或已缓存待 seek 目标时，拒绝外部（POS 定时器）覆盖
    if (m_pushDownPos == PUSH_DOWN_THUMB || m_pendingSeekPos >= 0) {
        return m_nVirtualCurPos;
    }

    nPosThumbOld = m_nPosThumb;
    nPosOld = m_nVirtualCurPos;
    m_nVirtualCurPos = nPos;

    if (m_nVirtualCurPos > m_nVirtualMax) {
        m_nVirtualCurPos = m_nVirtualMax;
    } else if (m_nVirtualCurPos < m_nVirtualMin) {
        m_nVirtualCurPos = m_nVirtualMin;
    }

    m_nPosThumb = virtualPosToObjectPos(m_nVirtualCurPos);

    if (bRedraw && m_nPosThumb != nPosThumbOld) {
        invalidate();
    }

    return nPosOld;
}

void CSkinSeekCtrl::setScrollInfo(int nMin, int nMax, int nPage, int nPos, int nLine, bool bRedraw) {
    m_nVirtualMin = nMin;
    m_nVirtualMax = nMax;
    m_nVirtualLine = nLine;
    m_nVirtualPage = nPage;

    m_nVirtualCurPos = nPos;

    if (m_nVirtualCurPos > m_nVirtualMax) {
        m_nVirtualCurPos = m_nVirtualMax;
    } else if (m_nVirtualCurPos < m_nVirtualMin) {
        m_nVirtualCurPos = m_nVirtualMin;
    }

    m_nPosThumb = virtualPosToObjectPos(m_nVirtualCurPos);

    if (bRedraw) {
        invalidate();
    }
}

int CSkinSeekCtrl::getMax() const {
    return m_nVirtualMax;
}

void CSkinSeekCtrl::setScrollNotify(IScrollNotify *pNofity) {
    m_pScrollNotify = pNofity;
}

CSkinSeekCtrl::PUSH_DOWN_POS CSkinSeekCtrl::getPushDownPos(CPoint pt) {
    //assert(isPtIn(pt));
    if (!isPtIn(pt)) {
        return PUSH_DOWN_NONE;
    }

    int x;

    x = m_rcObj.left + m_nPosThumb;
    if (pt.x < x) {
        return PUSH_DOWN_TOPTRACK;
    }

    x += m_nWidthThumb;
    if (pt.x < x) {
        return PUSH_DOWN_THUMB;
    }

    return PUSH_DOWN_BOTTOMTRACK;
}

int CSkinSeekCtrl::virtualPosToObjectPos(int nVirtualPos) {
    if (m_nVirtualMax - m_nVirtualMin == 0) {
        return 0;
    }

    return int((double)nVirtualPos * (getTrackRealWidth())) / (m_nVirtualMax - m_nVirtualMin);
}

int CSkinSeekCtrl::objectPosToVirtualPos(int nThumbPos) {
    assert(getTrackRealWidth() != 0);

    if (getTrackRealWidth() == 0) {
        return 0;
    }

    return int((double)nThumbPos * (m_nVirtualMax - m_nVirtualMin) / (getTrackRealWidth()));
}

// COMMENTS:
//        滚动后的大小，其大小通过 getScrollPos() 取得
// INPUT:
//        nSBCode:
//        SB_BOTTOM   Scroll to bottom.
//        SB_ENDSCROLL   End scroll.
//        SB_LINEDOWN   Scroll one line down.
//        SB_LINEUP   Scroll one line up.
//        SB_PAGEDOWN   Scroll one page down.
//        SB_PAGEUP   Scroll one page up.
//        SB_THUMBPOSITION   Scroll to the absolute position. The current position is provided in nPos.
//        SB_THUMBTRACK   Drag scroll box to specified position. The current position is provided in nPos.
//        SB_TOP   Scroll to top.
void CSkinSeekCtrl::onVScroll(uint32_t nSBCode) {
    if (m_pScrollNotify) {
        m_pScrollNotify->onVScroll(nSBCode, getScrollPos(), this);
    } else {
        m_pSkin->onVScroll(nSBCode, getScrollPos(), this);
    }
}

void CSkinSeekCtrl::onSize() {
    CUIObject::onSize();

    m_nPosThumb = virtualPosToObjectPos(m_nVirtualCurPos);
}

void CSkinSeekCtrl::notifyEvent(CSkinSeekCtrlEventNotify::Command cmd) {
    CSkinSeekCtrlEventNotify event(this);

    event.cmd = cmd;
    m_pSkin->dispatchUIObjNotify(&event);
}
