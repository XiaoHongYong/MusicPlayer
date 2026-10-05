/********************************************************************
    Created  :    2001-12-15 2:36:55
    FileName :    SkinWnd.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include "SkinTypes.h"
#include "Skin.h"
#include "SkinWnd.h"
#include "SkinButton.h"
#include "SkinVScrollBar.h"
#include "SkinCaption.h"
#include "api-js/SkinJsAPI.hpp"


#define _SZ_SKINWND         "skinwnd"


#define TIMER_ID_BEG_ALLOC  120
#define TIMER_ID_TOOLTIP 115
#define TIMER_ID_TINY_JS_VM 116
#define TIMER_ID_ANIMATION  117
#define TIMER_ID_DYNAMIC_TRANS  118
#define TIMER_ID_MOUSE_INACTIVE 119

#define TIMER_SPAN_MOUSE_INACTIVE    (3 * 1000)

#define TIMER_SPAN_DYNAMIC_TRANS 30
#define TIME_OUT_TRANS_FADEIN   500
#define TIME_OUT_ANIMATION      30

#define _TRANSLUCENCY_ENABLED

bool isWndOutOfScreen(const CRect &rc) {
    CRect rcRestrict;

    if (getMonitorRestrictRect(rc, rcRestrict)) {
        return ((rc.right < rcRestrict.left + 20) || (rc.left > rcRestrict.right - 20) ||
            (rc.bottom < rcRestrict.top + 10) || (rc.top > rcRestrict.bottom - 40));
    } else {
        return false;
    }
}


template<class _TCHAR>
uint32_t getColorValue_t(_TCHAR *szColor, uint32_t nDefault) {
    uint32_t uRet = 0;

    if (*szColor == '#') {
        szColor++;
    }

    if (*szColor == '\0') {
        return nDefault;
    }

    int n;

    while (*szColor) {
        if (*szColor >= '0' && *szColor <= '9') {
            n = *szColor - '0';
        } else if (*szColor >= 'a' && *szColor <= 'f') {
            n = 10 + *szColor - 'a';
        } else if (*szColor >= 'A' && *szColor <= 'F') {
            n = 10 + *szColor - 'A';
        } else {
            break;
        }

        uRet = uRet * 16 + n;
        szColor++;
    }

    if (*szColor != '\0') {
        return nDefault;
    }

    uRet = ((uRet & 0xFF) << 16) + (uRet & 0xFF00) + ((uRet & 0xFF0000) >> 16);

    return uRet;
}

bool getRectValue(cstr_t szValue, CSFImage &image) {
    int x, y, cx, cy;
    if (scan4IntX(szValue, x, y, cx, cy)) {
        image.setXYWH(x, y, cx, cy);
        return true;
    }
    return false;
}

bool getRectValue(cstr_t szValue, int &x, int &y, int &cx, int &cy) {
    return scan4IntX(szValue, x, y, cx, cy);
}

bool getRectValue(cstr_t szValue, CRect &rc) {
    if (!scan4IntX(szValue, rc.left, rc.top, rc.right, rc.bottom)) {
        return false;
    }
    rc.right += rc.left;
    rc.bottom += rc.top;
    return true;
}


string colorToStr(const CColor &clr) {
    char szValue[128];

    stringFromColor(szValue, clr.get());

    return szValue;
}

// FgColor="#FFFFFF"
void getColorValue(CColor &clr, cstr_t szColor) {
    clr.set(stringToColor(szColor, clr.get()));
}

int getMenuKey(cstr_t szText) {
    while (*szText && *szText != '&') {
        szText++;
    }

    if (*szText == '&') {
        return szText[1];
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////

CSkinWnd::CSkinWnd() : m_skinToolTip(this, TIMER_ID_TOOLTIP) {
    m_bFreeOnDestory = false;
    m_bManageBySkinFactory = true;

    m_bSkinOpened = false;
    m_nInRedrawUpdate = 0;
    m_needRedraw = false;
    m_bInEditorMode = false;

    m_bActived = false;
    m_bMainAppWnd = false;
    m_bDialogWnd = false;

    m_isSkinMenuPopup = false;

    m_rootConainter.m_pSkin = this;
    m_pUIObjCapMouse = nullptr;

    m_pUIObjHandleContextMenuCmd = nullptr;

    m_rcBoundBox.setLTRB(0, 0, 600, 350);

    CRect rcRestrict;
    if (getMonitorRestrictRect(m_rcBoundBox, rcRestrict)) {
        m_rcBoundBox.offsetRect((rcRestrict.width() - m_rcBoundBox.width()) / 2,
            (rcRestrict.height() - m_rcBoundBox.height()) / 2);
    }

    m_timerIDMax = TIMER_ID_BEG_ALLOC;

    m_nWidth = 0;
    m_nHeight = 0;
    m_bRememberSizePos = true;

    m_nextUIObjIDAlloc = UIOBJ_ID_BEGIN_ALLOC;

    m_nextAuoUniID = 1;

    m_pSkinFactory = nullptr;

    m_bIgnoreNextOnCharMsg = false;

    m_timeLatestMouseMsg = 0;
    m_bMouseActive = false;

    m_animateType = AT_UNKNOWN;
    m_animateDuration = 0;

    m_bTranslucencyLayered = false;
    m_nAlpha = 255;
    m_nTranslucencyAlphaOnActive = -1;
    m_nTranslucencyAlphaOnHover = -1;
    m_nTranslucencyAlphaDefault = -1;
    m_nCurTranslucencyAlpha = 255;
    m_dwTransFadeinBegTime = 0;
    m_nTransAlphaBeg = 0;
    m_bClickThrough = false;
    m_bEnableClickThrough = true;

    m_bOnMouseHover = false;
    m_translucencyStatus = TS_NORMAL;

    m_vm = nullptr;

#ifndef _WIN32
    m_rcMemUpdate.top = m_rcMemUpdate.bottom = 0;
#endif
}

CSkinWnd::~CSkinWnd() {
    closeSkin();
}

int CSkinWnd::create(SkinWndStartupInfo &skinWndStartupInfo, CSkinFactory *pSkinFactory, bool bToolWindow, bool bTopmost, bool bVisible) {
    m_pSkinFactory = pSkinFactory;
    m_strSkinWndName = skinWndStartupInfo.strSkinWnd;
    m_mapExchangePool = skinWndStartupInfo.mapExchangePool;
    m_bMainAppWnd = skinWndStartupInfo.bMainWnd;

    onPreCreate(bTopmost, bVisible);

    CAutoRedrawLock drawLock(this);

    createForSkin(skinWndStartupInfo.strClassName.c_str(), skinWndStartupInfo.strCaptionText.c_str(),
        m_rcBoundBox.left, m_rcBoundBox.top, m_rcBoundBox.width(), m_rcBoundBox.height(), skinWndStartupInfo.pWndParent,
        bToolWindow, bTopmost, bVisible);

    return ERR_OK;
}

void CSkinWnd::destroy() {
    m_bOnDestroy = true;

    Window::destroy();
}

int CSkinWnd::openDefaultSkin() {
    if (m_strSkinWndName.empty()) {
        return ERR_NOT_FOUND;
    }

    string strSkinWndName = m_strSkinWndName;

    return openSkin(strSkinWndName.c_str());
}

int CSkinWnd::openSkin(cstr_t szSkinWndName) {
    SXNode *pNode;
    CSimpleXML xml;

    // before load it we close old first.
    closeSkin();

    pNode = m_pSkinFactory->getSkinWndNode(szSkinWndName);
    if (!pNode) {
        int nRet = m_pSkinFactory->loadAppResSkinWndXml(szSkinWndName, xml);
        if (nRet != ERR_OK) {
            return nRet;
        }
        pNode = xml.m_pRoot;
    }

    m_strSkinWndName = szSkinWndName;

    CAutoRedrawLock redrawLock(this);

    return fromXML(pNode);
}

void CSkinWnd::closeSkin() {
    if (!m_bSkinOpened) {
        return;
    }

    // the skin was opened before, so save latest settings.
    if (m_bRememberSizePos) {
        saveWndPos();
    }

    m_scriptFile.clear();

    killTimer(TIMER_ID_ANIMATION);
    m_listAnimations.clear();

    m_onMouseMoveListener = jsValueEmpty;
    m_onSizeListener = jsValueEmpty;
    m_onActivateListener = jsValueEmpty;
    m_onDestoryListener = jsValueEmpty;
    m_onCommandListener = jsValueEmpty;
    m_onMouseActivateListener = jsValueEmpty;

    if (m_onDestoryListener.isFunction()) {
        callVMFunction(m_onDestoryListener);
    }

    killTimer(TIMER_ID_TINY_JS_VM);

    if (m_vm) {
        delete m_vm;
        m_vm = nullptr;
    }

    m_nAlpha = 255;
    m_nTranslucencyAlphaOnActive = -1;
    m_nTranslucencyAlphaOnHover = -1;
    m_nTranslucencyAlphaDefault = -1;
    m_nCurTranslucencyAlpha = 255;
    m_dwTransFadeinBegTime = 0;
    m_nTransAlphaBeg = 0;
    m_bClickThrough = false;
    m_bEnableClickThrough = true;

    m_wndResizer.fixedHeight(false);
    m_wndResizer.fixedWidth(false);
    m_wndResizer.clearResizeArea();

    //
    // kill timers
    MAP_TIMER_OBJS::iterator it;

    for (it = m_mapTimerObjs.begin(); it != m_mapTimerObjs.end(); it++) {
        killTimer((*it).first);
    }
    m_mapTimerObjs.clear();

    m_pUIObjHandleContextMenuCmd = nullptr;

    m_pUIObjCapMouse = nullptr;
    m_uiObjectFocus = nullptr;

    m_rootConainter.onDestroy();
    m_rootConainter.destroy();

    killTimer(TIMER_ID_DYNAMIC_TRANS);
    m_translucencyStatus = TS_NORMAL;

#ifdef _WIN32
    ::unSetLayeredWindow(m_hWnd);
#endif

    assert(m_nInRedrawUpdate == 0);
    m_nInRedrawUpdate = 0;
    m_needRedraw = false;
    m_bSkinOpened = false;

    m_listUnprocessedProperties.clear();

    // Remember skin window name, it can be used to reload it.
    // m_strSkinWndName.resize(0);

    m_nWidth = 0;
    m_nHeight = 0;
    m_bRememberSizePos = true;

    m_fontProperty.setProperty("Font", "Tohama,14,normal,0,0,");

    m_timeLatestMouseMsg = 0;
    m_bMouseActive = false;
    killTimer(TIMER_ID_MOUSE_INACTIVE);

    m_vUIObjNotifyHandlers.clear();

    m_animateType = AT_UNKNOWN;
    m_animateDuration = 0;
}

CUIObject *CSkinWnd::createUIObject(cstr_t className, CSkinContainer *container) {
    return m_pSkinFactory->createUIObject(this, className, container);
}

CUIObject *CSkinWnd::getUIObjectByClassName(cstr_t className) {
    return m_rootConainter.getUIObjectByClassName(className);
}

CUIObject * CSkinWnd::getUIObjectById(int nId, cstr_t className) {
    return m_rootConainter.getUIObjectById(nId, className);
}

CUIObject *CSkinWnd::getUIObjectById(cstr_t idName, cstr_t className) {
    auto id = m_pSkinFactory->getIDByName(idName);
    if (id != ID_INVALID) {
        return getUIObjectById(id, className);
    }
    return nullptr;
}

void CSkinWnd::processMouseMove(CPoint point) {
    onMouseActiveMsg();

#ifdef _TRANSLUCENCY_ENABLED
    //
    // Change translucency on hover?
    //
    if (m_bTranslucencyLayered
        && m_translucencyStatus == TS_NORMAL
        && !m_bActived) {
        m_translucencyStatus = TS_ON_HOVER;

        m_bOnMouseHover = m_rcBoundBox.ptInRect(point);

        startTranslucencyFade();
    }
#endif    // _TRANSLUCENCY_ENABLED

    m_rootConainter.processMouseEnterLeave(point);
}

void CSkinWnd::onMouseDrag(uint32_t nFlags, CPoint point) {
    processMouseMove(point);
    m_skinToolTip.onMouseDrag(nFlags, point);

    if (m_wndResizer.isSizing()) {
        m_wndResizer.onMouseMessage(nFlags, point);
        return;
    }

    if (m_WndDrag.isDragging()) {
        point = getCursorPos();
        m_WndDrag.onDrag(nFlags, point);
        return;
    }

    // first send message to ui objects that captured the mouse
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured()) {
            if (m_pUIObjCapMouse->onMouseDrag(point)) {
                return;
            }
        } else {
            // capture lost!
        }
    }

    // then let root container to process it
    if (!m_rootConainter.onMouseDrag(point)) {
        // else, should set mouse resizing cursor
        m_wndResizer.onMouseMessage(nFlags, point);
    }
}

void CSkinWnd::onMouseMove(CPoint point) {
    processMouseMove(point);
    m_skinToolTip.onMouseMove(point);

    // first send message to ui objects that captured the mouse
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured()) {
            if (m_pUIObjCapMouse->onMouseMove(point)) {
                return;
            }
        } else {
            // capture lost!
        }
    }

    // then let root container to process it
    if (!m_rootConainter.onMouseMove(point)) {
        // else, should set mouse resizing cursor
        m_wndResizer.onMouseMessage(0, point);
    }

    if (m_onMouseMoveListener.isFunction()) {
        callVMFunction(m_onMouseMoveListener,
            ArgumentsX(makeJsValueInt32(point.x), makeJsValueInt32(point.y)));
    }
}

void CSkinWnd::onLButtonDown(uint32_t nFlags, CPoint point) {
    onMouseActiveMsg();
    m_skinToolTip.onLButtonDown(nFlags, point);

    setFocus();

    // first send message to ui objects that captured the mouse!
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured()) {
            if (m_pUIObjCapMouse->onLButtonDown(nFlags, point)) {
                return;
            }
        } else {
            // capture lost!
        }
    }

    // then let root container to process it
    if (!m_rootConainter.onLButtonDown(nFlags, point)) {
        m_wndResizer.onMouseMessage(nFlags, point);

        if (!m_wndResizer.isSizing()) {
            point = getCursorPos();
            m_WndDrag.onDrag(nFlags, point);
        }
    }
}

void CSkinWnd::onLButtonDblClk(uint32_t nFlags, CPoint point) {
    onMouseActiveMsg();
    m_skinToolTip.onLButtonDblClk(nFlags, point);

    setFocus();

    // first send message to ui objects that captured the mouse!
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured()) {
            if (m_pUIObjCapMouse->onLButtonDblClk(nFlags, point)) {
                return;
            }
        } else {
            // capture lost!
        }
    }

    // then let root container to process it
    m_rootConainter.onLButtonDblClk(nFlags, point);
}

void CSkinWnd::onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) {
    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus && pObjFocus->needMsgMouseWheel()) {
        pObjFocus->onMouseWheel(nWheelDistance, nMkeys, pt);
        return;
    }

    // 由当前鼠标所在的 UIObject 来处理.
    auto obj = m_rootConainter.getUIObjectAtPosition(pt);
    assert(obj);
    obj->onMouseWheel(nWheelDistance, nMkeys, pt);
}

void CSkinWnd::onMagnify(float magnification) {
    CUIObject *obj = getFocusUIObj();
    if (obj && obj->needMsgMagnify()) {
        obj->onMagnify(magnification);
        return;
    }

    // 由当前鼠标所在的 UIObject 来处理.
    CPoint pt = getCursorPos();
    screenToClient(pt);
    obj = m_rootConainter.getUIObjectAtPosition(pt);
    assert(obj);
    obj->onMagnify(magnification);
}

void CSkinWnd::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (m_wndResizer.isSizing()) {
        m_wndResizer.onMouseMessage(nFlags, point);
        //        bMsgProceed = true;
        return;
    }

    if (m_WndDrag.isDragging()) {
        CPoint ptTemp = getCursorPos();
        m_WndDrag.onDrag(nFlags, ptTemp);
        //        return;
    }

    // first send message to ui objects that captured the mouse!
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured()) {
            if (m_pUIObjCapMouse->onLButtonUp(nFlags, point)) {
                return;
            }
        } else {
            m_pUIObjCapMouse = nullptr;
            // capture lost!
        }
    }

    // then let root container to process it
    m_rootConainter.onLButtonUp(nFlags, point);
}

void CSkinWnd::onRButtonDown(uint32_t nFlags, CPoint point) {
    onMouseActiveMsg();
    m_skinToolTip.onRButtonDown(nFlags, point);

    // first send message to ui objects that captured the mouse!
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured() && m_pUIObjCapMouse->needMsgRButton()) {
            if (m_pUIObjCapMouse->onRButtonDown(nFlags, point)) {
                return;
            }
        } else {
            // capture lost!
            m_pUIObjCapMouse = nullptr;
        }
    }

    // then let root container to process it
    m_rootConainter.onRButtonDown(nFlags, point);
}

void CSkinWnd::onRButtonUp(uint32_t nFlags, CPoint point) {
    // first send message to ui objects that captured the mouse!
    if (m_pUIObjCapMouse) {
        if (isMouseCaptured() && m_pUIObjCapMouse->needMsgRButton()) {
            if (m_pUIObjCapMouse->onRButtonUp(nFlags, point)) {
                return;
            }
        } else {
            // capture lost!
            m_pUIObjCapMouse = nullptr;
        }
    }

    // then let root container to process it
    m_rootConainter.onRButtonUp(nFlags, point);
}

bool CSkinWnd::onKeyDown(uint32_t nChar, uint32_t nFlags) {
#ifdef DEBUG
    {
        // dump UIObject
        if (isModifierKeyPressed(MK_ALT, nFlags)) {
            if (nChar == VK_F1) {
                int nDeep = 0;
                m_rootConainter.dumpUIObject(nDeep, nullptr);
            } else if (nChar == VK_F2) {
                CPoint pt = getCursorPos();
                screenToClient(pt);
                DBG_LOG2("Current Pos: %d, %d", pt.x, pt.y);

                int nDeep = 0;
                m_rootConainter.dumpUIObject(nDeep, &pt);
            } else if (nChar == VK_F3) {
                m_pSkinFactory->dumpUID();
            }
        }
    }
#endif
    m_skinToolTip.onKeyDown(nChar, nFlags);

    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus) {
        if (pObjFocus->needMsgKey()) {
            if (pObjFocus->onKeyDown(nChar, nFlags)) {
                return true;
            }
        }
    }

    if (isDialogWnd()) {
        if (nChar == VK_ESCAPE) {
            onCancel();
            return true;
        } else if (nChar == VK_RETURN) {
            onOK();
            return true;
        }
    }

    if (nFlags == MK_COMMAND) {
        if (nChar == VK_W) {
            onCommand(ID_CLOSE);
            return true;
        }
    }

    // tab key to switch between uiobject
    if (nChar == VK_TAB) {
        switchFocusUIObj(isModifierKeyPressed(MK_SHIFT, nFlags));

        // Ignore '\t' WM_CHAR message.
        m_bIgnoreNextOnCharMsg = true;

        return true;
    }

    return false;
}

bool CSkinWnd::onKeyUp(uint32_t nChar, uint32_t nFlags) {
    // Let focus UIObject to process key message
    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus && pObjFocus->needMsgKey()) {
        return pObjFocus->onKeyUp(nChar, nFlags);
    }

    return false;
}

void CSkinWnd::onContexMenu(int xPos, int yPos) {
    if (m_rootConainter.getMenu()) {
        m_rootConainter.getMenu()->trackPopupMenu(xPos, yPos, this);
    }
}

void CSkinWnd::onCommand(uint32_t id) {
    if (m_pUIObjHandleContextMenuCmd) {
        // Only handle 1 command.
        if (m_pUIObjHandleContextMenuCmd->onCommand(id)) {
            m_pUIObjHandleContextMenuCmd = nullptr;
            return;
        }
        m_pUIObjHandleContextMenuCmd = nullptr;
    }

    if (m_rootConainter.onCommand(id)) {
        return;
    }

    if (m_onCommandListener.isFunction()) {
        callVMFunction(m_onCommandListener, ArgumentsX(makeJsValueInt32(id)));
    }

    int nRet = m_pSkinFactory->onDynamicCmd(id, this);
    if (nRet != ERR_NOT_FOUND && nRet != ERR_OK) {
        ERR_LOG2("execute Dynamic Command failed: %s, %s", m_pSkinFactory->getStringOfID(id).c_str(), (cstr_t)Error2Str(nRet));
    }

    switch (id) {
    case ID_OK:
        onOK();
        break;
    case ID_CANCEL:
        onCancel();
        break;
    case ID_CLOSE:
        if (m_rootConainter.onClose()) {
            postDestroy();
        }
        break;
    case ID_QUIT:
        CSkinApp::getInstance()->postQuitMessage();
        break;
    case ID_MINIMIZE:
        if (isToolWindow()) {
            hide();
        } else {
            minimize();// (SW_SHOWMINIMIZED);
        }
        break;
    case ID_MAXIMIZE:
        if (isZoomed()) {
            restore();
        } else {
#ifdef WIN32
            bool bToolWindow = isToolWindow();
            if (bToolWindow) {
                showAsAppWindowNoRefresh(m_hWnd);
            }

            ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);

            if (bToolWindow) {
                showAsToolWindowNoRefresh(m_hWnd);
            }
#else
            maximize();
#endif
        }
        break;
    case ID_MENU:
        {
            CPoint pt = getCursorPos();
            onContexMenu(pt.x, pt.y);
        }
        break;
    case ID_EXEC_FUNCTION:
        onExecOnMainThread();
        break;
    default:
        break;
    }

    Window::onCommand(id);
}

void CSkinWnd::onOK() {
    if (m_bDialogWnd && m_rootConainter.onOK()) {
        postDestroy();
    }
}

void CSkinWnd::onCancel() {
    if (m_bDialogWnd && m_rootConainter.onCancel()) {
        postDestroy();
    }
}

void CSkinWnd::onChar(uint32_t nChar) {
    if (m_bIgnoreNextOnCharMsg) {
        m_bIgnoreNextOnCharMsg = false;
        return;
    }

    // let focus uiobject to process key message
    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus && pObjFocus->needMsgKey()) {
        pObjFocus->onChar(nChar);
    }
}

bool CSkinWnd::updateSkinProperty() {
    if (m_bTranslucencyLayered) {
        if (m_bActived) {
            m_nCurTranslucencyAlpha = m_nTranslucencyAlphaOnActive;
        } else {
            m_nCurTranslucencyAlpha = m_nTranslucencyAlphaDefault;
        }
    }

    // init
    onSize(m_rcBoundBox.width(), m_rcBoundBox.height());

    // macOS 下需要把最小尺寸设置到 NSWindow 上，系统级的窗口缩放才会受 MinWidth/MinHeight 限制
    setMinSize(m_wndResizer.getMinCx(), m_wndResizer.getMinCy());

    return true;
}

void CSkinWnd::onPaint(CRawGraph *canvas, CRect *rcClip) {
    if (isIconic() || m_bOnDestroy) {
        return;
    }

    bool bRedraw = true;
#ifndef _WIN32
    if (m_rcMemUpdate.top != m_rcMemUpdate.bottom) {
        m_rcMemUpdate.top = m_rcMemUpdate.bottom = 0;
        if (m_rcMemUpdate == *rcClip) {
            bRedraw = false;
        }
    }
#endif // #ifdef _WIN32

    if (bRedraw) {
        canvas->resetClipBoundBox(*rcClip);

        if (m_imageBg.isValid()) {
            // WindowImage 固定 copy：不透明区域画底座，圆角外透明像素覆盖画布，
            // 半透明窗口下角外即透出桌面，不必再叠 copy 模式的 Frame 打孔。
            CDrawImageFunMask funcDraw(canvas, &m_imageBg,
                m_imageBgMask.isValid() ? &m_imageBgMask : nullptr, BPM_COPY);
            m_bgImagePainter.blt(0, 0, m_rcBoundBox.width(), m_rcBoundBox.height(), funcDraw);
        }

        // draw every UI objects on back buffer one by one
        m_rootConainter.draw(canvas);
        if (!m_isSkinMenuPopup && m_bActived) {
            // Show tooltip only when menu is not popup and window is activated.
            m_skinToolTip.onPaint(canvas);
        }
    }

    canvas->drawToWindow(rcClip->left, rcClip->top, rcClip->width(), rcClip->height(), rcClip->left, rcClip->top);
}


CColor CSkinWnd::getTranslucencyColor(const CColor &clr) const {
    if (m_bTranslucencyLayered) {
        CColor clrNew = clr;
        clrNew.setAlpha((uint8_t)m_nCurTranslucencyAlpha);
        preMultiplyColor(clrNew, (uint8_t)m_nCurTranslucencyAlpha);
        return clrNew;
    } else {
        return clr;
    }
}

void CSkinWnd::addToolTip(const StringView &text, CRect &rc, uint32_t idTooltip) {
    m_skinToolTip.addToolTip(text, rc, idTooltip);
}

void CSkinWnd::delToolTip(uint32_t nIDTool) {
    m_skinToolTip.delToolTip(nIDTool);
}

void CSkinWnd::invalidateUIObject(CUIObject *pObj) {
    assert(pObj);

    if (!isVisible() || isIconic()) {
        return;
    }

    if (m_nInRedrawUpdate > 0) {
        m_needRedraw = true;
        return;
    }

    if (!pObj->isVisible() || !pObj->isParentVisible()) {
        return;
    }

    if (!m_pmemGraph) {
        return; // SkinWnd isn't created yet.
    }

    invalidateRect(&pObj->m_rcObj);
}

void CSkinWnd::enterInDrawUpdate() {
    m_nInRedrawUpdate++;
}

void CSkinWnd::leaveInDrawUpdate() {
    m_nInRedrawUpdate--;
    assert(m_nInRedrawUpdate >= 0);
    if (m_nInRedrawUpdate == 0) {
        if (m_needRedraw) {
            invalidateRect();
        }
    }
}

void CSkinWnd::setMainAppWnd(bool bMainAppWnd) {
    m_bMainAppWnd = bMainAppWnd;
}

void CSkinWnd::getWndDragAutoCloseTo(vector<Window *> &vWnd) {
    m_pSkinFactory->getWndDragAutoCloseTo(vWnd);
}

void CSkinWnd::getWndDragTrackMove(vector<Window *> &vWnd) {
    if (m_bMainAppWnd) {
        m_pSkinFactory->getWndDragTrackMove(vWnd);
    }
}

void CSkinWnd::setCaptureMouse(CUIObject *pUIObj) {
    assert(pUIObj);

    setCapture();

    m_pUIObjCapMouse = pUIObj;
}

CUIObject * CSkinWnd::getCaptureMouse() {
    return m_pUIObjCapMouse;
}

void CSkinWnd::releaseCaptureMouse(CUIObject *pUIObj) {
    if (pUIObj == m_pUIObjCapMouse) {
        releaseCapture();

        m_pUIObjCapMouse = nullptr;
    } else {
        DBG_LOG2("NOT captured mouse, m_pUIObjCapMouse: %lx, pUIObj: %lx", m_pUIObjCapMouse, pUIObj);
    }
}

void CSkinWnd::onMove(int x, int y) {
    if (!isWindow()) {
        return;
    }

    if (isIconic()) {
        return;
    }

    CRect rc;

    getWindowRect(&rc);
    if (!isWndOutOfScreen(rc)) {
        rc.right = rc.left + m_rcBoundBox.width();
        rc.bottom = rc.top + m_rcBoundBox.height();
        m_rcBoundBox = rc;
    }
}

void CSkinWnd::onSize(int cx, int cy) {
    if (!isIconic()) {
        CAutoRedrawLock redrawLock(this);

        CRect rc;
        getWindowRect(&rc);
        rc.right = rc.left + cx;
        rc.bottom = rc.top + cy;
        if (isWndOutOfScreen(rc)) {
            moveWindow(m_rcBoundBox);
        } else {
            m_rcBoundBox = rc;

            // recalculate all ui objects' position
            m_rootConainter.m_rcObj.setLTRB(0, 0, m_rcBoundBox.width(), m_rcBoundBox.height());
            m_rootConainter.onSize();

            invalidateRect();

            if (m_onSizeListener.isFunction()) {
                callVMFunction(m_onSizeListener,
                    ArgumentsX(makeJsValueInt32(cx), makeJsValueInt32(cy)));
            }
        }
    }
}

void CSkinWnd::onSetFocus() {
    CUIObject *pObj = getFocusUIObj();
    if (pObj) {
        pObj->onSetFocus();
    }
}

void CSkinWnd::onKillFocus() {
    if (m_pUIObjCapMouse) {
        m_pUIObjCapMouse->onMouseMove(CPoint(-1, -1));
        releaseCaptureMouse(m_pUIObjCapMouse);
    }

    m_rootConainter.onKillFocus();
}

static CUIObject *getVisibleByHierachy(CUIObject *obj) {
    CUIObject *visibleObj = nullptr;
    for (auto p = obj; p != nullptr; p = p->getParent()) {
        if (p->isVisible()) {
            if (visibleObj == nullptr) {
                visibleObj = p;
            }
        } else {
            // 之前的 child 都 invisible
            visibleObj = nullptr;
        }
    }

    return visibleObj;
}

CUIObject *nextCanFocusUIObjInChildren(CSkinContainer *parent) {
    int pos = 0, count = (int)parent->getChildrenCount();
    for (; pos < count; pos++) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj->isVisible() && obj->isEnable()) {
            if (obj->needMsgKey()) {
                return obj;
            } else if (obj->isContainer()) {
                obj = nextCanFocusUIObjInChildren(obj->getContainerIf());
                if (obj) {
                    return obj;
                }
            }
        }
    }

    return nullptr;
}

CUIObject *nextCanFocusUIObjInSibParent(CUIObject *cur) {
    CSkinContainer *parent = cur->getParent();
    if (parent == nullptr) {
        // 找到了根节点
        return nullptr;
    }

    // 先找到 @cur 在 parent 中的位置
    int pos = 0, count = (int)parent->getChildrenCount();
    for (; pos < count; pos++) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj == cur) {
            break;
        }
    }

    // 在接下来的 sibling 中查找
    for (pos++; pos < count; pos++) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj->isVisible() && obj->isEnable()) {
            if (obj->needMsgKey()) {
                return obj;
            } else if (obj->isContainer()) {
                obj = nextCanFocusUIObjInChildren(obj->getContainerIf());
                if (obj) {
                    return obj;
                }
            }
        }
    }

    // Sibling 中也没有，继续在 parent 中查找
    return nextCanFocusUIObjInSibParent(parent);
}

CUIObject *prevCanFocusUIObjInChildren(CSkinContainer *parent) {
    int pos = (int)parent->getChildrenCount() - 1;
    for (; pos >= 0; pos--) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj->isVisible() && obj->isEnable()) {
            if (obj->needMsgKey()) {
                return obj;
            } else if (obj->isContainer()) {
                obj = prevCanFocusUIObjInChildren(obj->getContainerIf());
                if (obj) {
                    return obj;
                }
            }
        }
    }

    return nullptr;
}

CUIObject *prevCanFocusUIObjInSibParent(CUIObject *cur) {
    CSkinContainer *parent = cur->getParent();
    if (parent == nullptr) {
        // 找到了根节点
        return nullptr;
    }

    // 先找到 @cur 在 parent 中的位置
    int pos = (int)parent->getChildrenCount() - 1;
    for (; pos >= 0; pos--) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj == cur) {
            break;
        }
    }

    // 在接下来的 sibling 中查找
    for (pos--; pos >= 0; pos--) {
        CUIObject *obj = parent->getChildByIndex(pos);
        if (obj->isVisible() && obj->isEnable()) {
            if (obj->needMsgKey()) {
                return obj;
            } else if (obj->isContainer()) {
                obj = prevCanFocusUIObjInChildren(obj->getContainerIf());
                if (obj) {
                    return obj;
                }
            }
        }
    }

    // Sibling 中也没有，继续在 parent 中查找
    return prevCanFocusUIObjInSibParent(parent);
}

void CSkinWnd::switchFocusUIObj(bool toPrev) {
    CUIObject *obj = nullptr;
    CUIObject *prevFocus = getVisibleByHierachy(m_uiObjectFocus);
    if (toPrev) {
        if (prevFocus) {
            if (prevFocus->isContainer()) {
                // 在 prevFocus 的 children 中查找
                obj = prevCanFocusUIObjInChildren(prevFocus->getContainerIf());
            }

            if (!obj) {
                // 在兄弟节点/父节点查找
                obj = prevCanFocusUIObjInSibParent(prevFocus);
            }
        }

        if (!obj) {
            // 从头再来查找
            obj = prevCanFocusUIObjInChildren(&m_rootConainter);
        }
    } else {
        if (prevFocus) {
            if (prevFocus->isContainer()) {
                // 在 prevFocus 的 children 中查找
                obj = nextCanFocusUIObjInChildren(prevFocus->getContainerIf());
            }

            if (!obj) {
                // 在兄弟节点/父节点查找
                obj = nextCanFocusUIObjInSibParent(prevFocus);
            }
        }

        if (!obj) {
            // 从头再来查找
            obj = nextCanFocusUIObjInChildren(&m_rootConainter);
        }
    }

    if (obj != m_uiObjectFocus) {
        // Focus 改变了.
        setFocusUIObj(obj);
    }
}

bool CSkinWnd::moveWindow(int X, int Y, int nWidth, int nHeight, bool bRepaint) {
    if (nHeight < m_wndResizer.getMinCy()) {
        nHeight = m_wndResizer.getMinCy();
    }
    if (nWidth < m_wndResizer.getMinCx()) {
        nWidth = m_wndResizer.getMinCx();
    }

#ifdef _WIN32
    if (::GetParent(m_hWnd)) {
        return true;
    }
#endif
    return moveWindowSafely(X, Y, nWidth, nHeight);
}

bool CSkinWnd::moveWindow(CRect &rc, bool bRepaint) {
    return moveWindow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, bRepaint);
}

void CSkinWnd::setProperies(SXNode::ListProperties &listProperties) {
    SXNode::iterProperties it;
    for (it = listProperties.begin(); it != listProperties.end(); ++it) {
        SXNode::Property &prop = *it;
        if (!setProperty(prop.name.c_str(), prop.strValue.c_str())) {
            if (!isPropertyName(prop.name.c_str(), SZ_PN_EXTENDS) &&
                !isPropertyName(prop.name.c_str(), SZ_PN_OS)) { // Do NOT log extends/os
                m_listUnprocessedProperties.push_back(prop);
                DBG_LOG2("Unknow Property: %s, %s", prop.name.c_str(), prop.strValue.c_str());
            }
        }
    }
}

void CSkinWnd::applyXmlPropertyNodes(SXNode *pXmlNode) {
    for (SXNode *pNode : pXmlNode->listChildren) {
        if (!isPropertyName(pNode->name.c_str(), SZ_PN_PROPERTY)) {
            continue;
        }
        if (!isSkinXmlNodeForCurrentOs(pNode)) {
            continue;
        }

        cstr_t szPropName = pNode->getPropertySafe(SZ_PN_NAME);
        cstr_t szExtends = pNode->getProperty(SZ_PN_EXTENDS);
        SXNode *pExtends = nullptr;
        if (szExtends != nullptr) {
            pExtends = m_pSkinFactory->getExtendsStyle(szExtends);
        }
        CSXNodeProperty properties(pNode, pExtends);
        if (!isEmptyString(szPropName) && !setProperty(szPropName, &properties)) {
            DBG_LOG2("Unknow Property: %s, name: %s", pNode->name.c_str(), szPropName);
        }
    }
}

bool CSkinWnd::setProperty(cstr_t szProperty, CSXNodeProperty *pProperties) {
    if (isPropertyName(szProperty, "WindowImage") || isPropertyName(szProperty, "BgImage")) {
        // WindowImage 固定 copy，铺满整窗；旧名 BgImage 仍接受。
        loadBgImageProperty(this, pProperties, m_imageBg, m_imageBgMask, m_bgImagePainter, nullptr);
        return true;
    }

    return false;
}

bool CSkinWnd::setProperty(cstr_t szProperty, cstr_t szValue) {
    assert(isWindow());

    if (strcasecmp(szProperty, "MinWidth") == 0) {
        m_wndResizer.setMinCx(atoi(szValue));
    } else if (strcasecmp(szProperty, "MinHeight") == 0) {
        m_wndResizer.setMinCy(atoi(szValue));
    } else if (strcasecmp(szProperty, SZ_PN_WIDTH) == 0) {
        m_nWidth = atoi(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_HEIGHT) == 0) {
        m_nHeight = atoi(szValue);
    } else if (isPropertyName(szProperty, "RememberSizePos")) {
        m_bRememberSizePos = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "fixedWidth") == 0) {
        m_wndResizer.fixedWidth(isTRUE(szValue));
    } else if (strcasecmp(szProperty, "fixedHeight") == 0) {
        m_wndResizer.fixedHeight(isTRUE(szValue));
    } else if (strcasecmp(szProperty, SZ_PN_NAME) == 0) {
        m_strSkinWndName = szValue;
    } else if (isPropertyName(szProperty, "Caption")) {
        m_strCaption = _TL(szValue);
        setTitle(m_strCaption.c_str());
    } else if (isPropertyName(szProperty, SZ_PN_OS)) {
        // 由皮肤加载按当前 OS 过滤子节点，窗口本身忽略。
    } else if (strcasecmp(szProperty, "ContextMenu") == 0) {
        m_rootConainter.setProperty(szProperty, szValue);
    } else if (strcasecmp(szProperty, "Menu") == 0) {
        m_strMenuName = szValue;
    } else if (m_fontProperty.setProperty(szProperty, szValue)) {
        if (m_bSkinOpened) {
            // Notify every child to change its font if needed.
            m_rootConainter.onSkinFontChanged();

            // Redraw
            invalidateRect();
        }
        return true;
    } else if (isPropertyName(szProperty, "EnableClickThrough")) {
        m_bEnableClickThrough = isTRUE(szValue);
    }
    else if (isPropertyName(szProperty, "EnableTranslucency")) {
        m_bTranslucencyLayered = isTRUE(szValue);
        if (m_pmemGraph) {
            delete m_pmemGraph;
            m_pmemGraph = nullptr;
        }
    } else if (isPropertyName(szProperty, "TranslucencyAlpha")) {
        m_nTranslucencyAlphaDefault = atoi(szValue);
        if (m_nTranslucencyAlphaOnActive == -1) {
            m_nTranslucencyAlphaOnActive = m_nTranslucencyAlphaDefault;
        }
        if (m_nTranslucencyAlphaOnHover == -1) {
            m_nTranslucencyAlphaOnHover = m_nTranslucencyAlphaDefault;
        }
        if (m_bSkinOpened) {
            startTranslucencyFade();
        }
    } else if (isPropertyName(szProperty, "TranslucencyAlphaOnActive")) {
        m_nTranslucencyAlphaOnActive = atoi(szValue);
        if (m_nTranslucencyAlphaDefault == -1) {
            m_nTranslucencyAlphaDefault = m_nTranslucencyAlphaOnActive;
        }
        if (m_bSkinOpened) {
            startTranslucencyFade();
        }
    } else if (isPropertyName(szProperty, "TranslucencyAlphaOnHover")) {
        m_nTranslucencyAlphaOnHover = atoi(szValue);
    } else if (isPropertyName(szProperty, "IsDialog")) {
        m_bDialogWnd = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "animate")) {
        m_animateType = animateTypeFromString(szValue);
    } else if (isPropertyName(szProperty, "AnimateDuration")) {
        m_animateDuration = atoi(szValue);
    } else if (isPropertyName(szProperty, "Script")) {
        m_scriptFile = szValue;
    } else if (iStartsWith(szProperty, SZ_PN_TOOLTIP) &&
            szProperty[ConstStrLen(SZ_PN_TOOLTIP)] != '\0') {
        return m_skinToolTip.setProperty(szProperty + ConstStrLen(SZ_PN_TOOLTIP), szValue);
    } else if (m_rootConainter.setProperty(szProperty, szValue)) {
        DBG_LOG2("Property is set to Root Container: %s, %s", szProperty, szValue);
        return true;
    } else {
        return false;
    }

    return true;
}

#ifdef _SKIN_EDITOR_
void CSkinWnd::enumProperties(CUIObjProperties &listProperties) {
    listProperties.addPropStr(SZ_PN_NAME, m_strSkinWndName.c_str());
    listProperties.addPropStr("Caption", m_strCaption.c_str(), !m_strCaption.empty());

    listProperties.addPropInt("MinWidth", m_wndResizer.getMinCx(), m_wndResizer.getMinCx() != 0);
    listProperties.addPropInt("MinHeight", m_wndResizer.getMinCy(), m_wndResizer.getMinCy() != 0);
    listProperties.addPropInt(SZ_PN_WIDTH, m_nWidth, m_nWidth != 0);
    listProperties.addPropInt(SZ_PN_HEIGHT, m_nHeight, m_nHeight != 0);
    listProperties.addPropFile("Cursor", m_strCusor.c_str(), "*.cur", !m_strCusor.empty());
    listProperties.addPropBoolStr("fixedWidth", m_wndResizer.isFixedWidth(), m_wndResizer.isFixedWidth());
    listProperties.addPropBoolStr("fixedHeight", m_wndResizer.isFixedHeight(), m_wndResizer.isFixedHeight());

    m_fontProperty.enumProperties(listProperties);

    listProperties.addPropStr("ContextMenu", m_rootConainter.m_strContextMenu.c_str(), !m_rootConainter.m_strContextMenu.empty());

    listProperties.addPropBoolStr("EnableTransparentFeature", m_bEnableClickThrough, !m_bEnableClickThrough);
    listProperties.addPropBoolStr("EnableTranslucency", m_bTranslucencyLayered, m_bTranslucencyLayered);
    listProperties.addPropInt("TranslucencyAlpha", m_nTranslucencyAlphaDefault, m_nTranslucencyAlphaDefault != 255);
    listProperties.addPropInt("TranslucencyAlphaOnActive", m_nTranslucencyAlphaOnActive, m_nTranslucencyAlphaOnActive != 255);
    listProperties.addPropInt("TranslucencyAlphaOnHover", m_nTranslucencyAlphaOnHover, m_nTranslucencyAlphaOnHover != 255);
}
#endif // _SKIN_EDITOR_

bool CSkinWnd::getUnprocessedProperty(cstr_t szProperty, string &strValue) {
    for (SXNode::iterProperties it = m_listUnprocessedProperties.begin(); it != m_listUnprocessedProperties.end(); ++it) {
        SXNode::Property &prop = *it;
        if (strcasecmp(szProperty, prop.name.c_str()) == 0) {
            strValue = prop.strValue;
            return true;
        }
    }

    return false;
}

void CSkinWnd::setFocusUIObj(CUIObject *obj) {
    if (m_uiObjectFocus != obj) {
        if (m_uiObjectFocus) {
            m_uiObjectFocus->onKillFocus();
        }

        m_uiObjectFocus = obj;

        if (obj) {
            obj->onSetFocus();
        }
    }
}

CUIObject *CSkinWnd::getFocusUIObj() {
    auto obj = m_uiObjectFocus;
    while (obj) {
        if (!obj->isVisible()) {
            return nullptr;
        }
        obj = obj->getParent();
    }

    return m_uiObjectFocus;
}

bool CSkinWnd::enableUIObject(int nId, bool bEnable, bool bRedraw) {
    return m_rootConainter.enableUIObject(nId, bEnable, bRedraw);
}

bool CSkinWnd::setUIObjectProperty(int nId, cstr_t szProperty, cstr_t szValue) {
    return m_rootConainter.setUIObjectProperty(nId, szProperty, szValue);
}

void CSkinWnd::setUIObjectVisible(int nId, bool bVisible, bool bRedraw) {
    m_rootConainter.setUIObjectVisible(nId, bVisible, bRedraw);
}

void CSkinWnd::invalidateUIObject(int nId) {
    m_rootConainter.invalidateUIObject(nId);
}

string CSkinWnd::getUIObjectText(int nId) {
    return m_rootConainter.getUIObjectText(nId);
}

void CSkinWnd::setUIObjectText(int nId, cstr_t szText, bool bRedraw) {
    m_rootConainter.setUIObjectText(nId, szText, bRedraw);
}

void CSkinWnd::checkButton(int nId, bool bCheck) {
    m_rootConainter.checkButton(nId, bCheck);
}

bool CSkinWnd::isButtonChecked(int nId) {
    return m_rootConainter.isButtonChecked(nId);
}

void CSkinWnd::checkToolbarButton(cstr_t szToolbarId, cstr_t szButtonId, bool bCheck) {
    m_rootConainter.checkToolbarButton(szToolbarId, szButtonId, bCheck);
}

void CSkinWnd::checkToolbarButton(int nToolbarId, int nButtonId, bool bCheck) {
    m_rootConainter.checkToolbarButton(nToolbarId, nButtonId, bCheck);
}

CUIObject *CSkinWnd::removeUIObjectById(int nId) {
    return m_rootConainter.removeUIObjectById(nId);
}

bool CSkinWnd::removeUIObject(CUIObject *pObj, bool bFree) {
    return m_rootConainter.removeUIObject(pObj, bFree);
}

void CSkinWnd::onDestroy() {
    closeSkin();

    m_skinToolTip.onDestroy();
    m_fontProperty.clear();

    Window::onDestroy();

    if (m_bMainAppWnd) {
        m_pSkinFactory->onMainSkinWndDestory(this);
    }

    if (m_bManageBySkinFactory) {
        m_pSkinFactory->onSkinWndDestory(this);
    }
}


// COMMENT:
//        onCreate must return true to continue the creation of the CWnd object. If the application returns false, the window will be destroyed.
void CSkinWnd::onCreate() {
    m_bOnDestroy = false;

    Window::onCreate();
    m_skinToolTip.onCreate();

    if (m_bManageBySkinFactory) {
        m_pSkinFactory->onSkinWndCreate(this);
    }

    m_WndDrag.init(this, this);//, arrszClassCloseTo);
    m_wndResizer.init(this, this);//, arrszClassCloseTo, this);

    // load skin
    openDefaultSkin();

    // If translucency is used, call this to update screen explicitly
    if (m_bTranslucencyLayered) {
        invalidateRect(nullptr);
    }
}

bool CSkinWnd::isWndActive() {
    return m_bActived;
}

void CSkinWnd::onActivate(bool bActived) {
    CAutoRedrawLock redrawLock(this);

    m_bActived = bActived;

    if (!m_bOnDestroy) {
#ifdef _TRANSLUCENCY_ENABLED
        startTranslucencyFade();

        if (bActived) {
            m_translucencyStatus = TS_ON_ACTIVE;
        } else {
            m_translucencyStatus = TS_NORMAL;
        }

        if (m_bEnableClickThrough && m_bClickThrough) {
            setTransparent(m_nAlpha, m_bClickThrough);
        }

#ifdef _MAC_OS
        setHasShadow(bActived);
#endif
#endif
        if (bActived != m_bMouseActive) {
            m_bMouseActive = bActived;
            onMouseActive(m_bMouseActive);
        }

        if (m_onActivateListener.isFunction()) {
            callVMFunction(m_onActivateListener,
                ArgumentsX(makeJsValueBool(bActived)));
        }
    }

    if (bActived && !isIconic()) {
        m_pSkinFactory->onSkinWndActivate(this);
    }
}

void CSkinWnd::addWndCloseto(Window *pWnd, cstr_t szWndClass, cstr_t szWndName) {
    m_WndDrag.addWndCloseto(pWnd, szWndClass, szWndName);
}

void CSkinWnd::updateMemGraphicsToScreen(const CRect* lpRect) {
    if (!isVisible() || isIconic()) {
        return;
    }

    if (m_nInRedrawUpdate > 0) {
        m_needRedraw = true;
        return;
    }

#ifdef _WIN32
    if (getEnableTranslucencyLayered()) {
        updateLayeredWindowUsingMemGraph(m_pmemGraph);
    } else {
        m_pmemGraph->drawToWindow(
            lpRect->left, lpRect->top,
            lpRect->right - lpRect->left, lpRect->bottom - lpRect->top,
            lpRect->left, lpRect->top);
    }
#else // #ifdef _WIN32
    if (m_rcMemUpdate.top == m_rcMemUpdate.bottom) {
        m_rcMemUpdate = *lpRect;
    }

    invalidateRect(lpRect);
#endif // #ifdef _WIN32
}

void CSkinWnd::enableTranslucencyLayered(bool bTranslucencyLayered) {
    if (m_pmemGraph) {
        delete m_pmemGraph;
        m_pmemGraph = nullptr;
    }

    m_bTranslucencyLayered = bTranslucencyLayered;
    if (!bTranslucencyLayered) {
        m_nCurTranslucencyAlpha = 255;
    }
}

void CSkinWnd::startTranslucencyFade() {
    if (m_bTranslucencyLayered) {
        m_dwTransFadeinBegTime = getTickCount();
        m_nTransAlphaBeg = m_nCurTranslucencyAlpha;

        killTimer(TIMER_ID_DYNAMIC_TRANS);
        setTimer(TIMER_ID_DYNAMIC_TRANS, TIMER_SPAN_DYNAMIC_TRANS);
    }
}

void CSkinWnd::onTimerDynamicAlphaChange() {
    int nTargetAlpha;

    if (m_translucencyStatus == TS_ON_HOVER) {
        //
        // Adjust translucency alpha on mouse hover in/out.
        //

        CPoint point = getCursorPos();
        bool bOnMouseHover = m_rcBoundBox.ptInRect(point);
        if (m_bOnMouseHover != bOnMouseHover) {
            m_bOnMouseHover = bOnMouseHover;
            m_dwTransFadeinBegTime = getTickCount();
            m_nTransAlphaBeg = m_nCurTranslucencyAlpha;
        }

        if (m_bOnMouseHover) {
            nTargetAlpha = m_nTranslucencyAlphaOnHover;
        } else {
            nTargetAlpha = m_nTranslucencyAlphaDefault;
        }
    } else if (isWndActive()) {
        nTargetAlpha = m_nTranslucencyAlphaOnActive;
    } else {
        nTargetAlpha = m_nTranslucencyAlphaDefault;
    }

    auto now = getTickCount();
    int nAlphaNew;
    if (now - m_dwTransFadeinBegTime > TIME_OUT_TRANS_FADEIN) {
        killTimer(TIMER_ID_DYNAMIC_TRANS);
        if (m_translucencyStatus == TS_ON_HOVER) {
            m_translucencyStatus = TS_NORMAL;
        }
        nAlphaNew = nTargetAlpha;
    } else {
        float fRadio = ((float)(now - m_dwTransFadeinBegTime)) / TIME_OUT_TRANS_FADEIN;
        nAlphaNew = m_nTransAlphaBeg + int((nTargetAlpha - m_nTransAlphaBeg) * fRadio);
    }
    if (m_nCurTranslucencyAlpha != nAlphaNew && m_bTranslucencyLayered) {
        m_nCurTranslucencyAlpha = nAlphaNew;
        assert(m_nCurTranslucencyAlpha >= 0 && m_nCurTranslucencyAlpha <= 255);

        invalidateRect();
    }
}

void CSkinWnd::onSkinLoaded() {
    switchFocusUIObj();

    if (!m_scriptFile.empty()) {
        assert(m_vm == nullptr);
        string code;
        string fn = m_pSkinFactory->getResourceMgr()->getResourcePathName(m_scriptFile.c_str());
        if (!readFile(fn.c_str(), code)) {
            ERR_LOG1("Failed to open script file: %s", m_scriptFile.c_str());
        } else {
            m_vm = new JsVirtualMachine();
            auto runtime = m_vm->defaultRuntime();
            auto ctx = runtime->mainCtx();

            runtime->globalScope()->set(makeCommonString("document"),
                runtime->pushObject(new JsSkinDocument(this)));

            m_vm->run(code.c_str(), code.size());
            if (ctx->error != JE_OK) {
                auto err = m_vm->defaultRuntime()->toStringView(ctx, ctx->errorMessage);
                ERR_LOG3("Failed to eval script: %s, error: %.*s", m_scriptFile.c_str(), err.len, err.data);
            }

            // 需要周期性的执行 TinyJs VM 的任务.
            // 用 4ms(250Hz) 替代原来的 1ms：皮肤 JS 里最小 setTimeout/setInterval 就是 4/5ms，
            // 1ms 的泵粒度属于严重过采样，会以 ~1000 次/秒把 runloop 的 timer 机制保持烫，
            // 白白吃掉主线程几个点。4ms 仍远超显示刷新率，保留全部真实时序，唤醒量降到 1/4。
            setTimer(TIMER_ID_TINY_JS_VM, 4);

            // 初始化调用一些 JS 事件
            if (m_onSizeListener.isFunction()) {
                int cx = m_rcBoundBox.width(), cy = m_rcBoundBox.height();
                callVMFunction(m_onSizeListener,
                    ArgumentsX(makeJsValueInt32(cx), makeJsValueInt32(cy)));
            }
        }
    }
}

void CSkinWnd::postCustomCommandMsg(int nId) {
#ifdef _WIN32
    ::PostMessage(m_hWnd, WM_COMMAND, nId, 0);
#else // #ifdef _WIN32
#ifdef _MAC_OS
    ::postCommandMsgMac(this, nId);
#else
    onCommand(nId);
#endif // #ifdef _MAC_OS
#endif // #ifdef _WIN32
}

void CSkinWnd::postExecOnMainThread(const std::function<void()> &f) {
    RMutexAutolock lock(m_mutex);
    m_functionsToExecOnMainThread.push_back(f);
    postCustomCommandMsg(ID_EXEC_FUNCTION);
}

void CSkinWnd::onExecOnMainThread() {
    while (!m_functionsToExecOnMainThread.empty()) {
        RMutexAutolock lock(m_mutex);
        auto f = m_functionsToExecOnMainThread.front();
        m_functionsToExecOnMainThread.pop_front();
        f();
    }
}

void CSkinWnd::postShortcutKeyCmd(int nId) {
    postCustomCommandMsg(nId);
}

void CSkinWnd::onUserMessage(int nMessageID, LPARAM param) {
    m_rootConainter.onUserMessage(nMessageID, param);
}

void CSkinWnd::onRemoveUIObj(CUIObject *obj) {
    onUIObjectHidden(obj);
}

void CSkinWnd::onAddUIObj(CUIObject *obj) {
}

void CSkinWnd::onUIObjectHidden(CUIObject *obj) {
    if (obj == m_pUIObjCapMouse) {
        releaseCaptureMouse(obj);
    }

    if (obj == m_pUIObjHandleContextMenuCmd) {
        m_pUIObjHandleContextMenuCmd = nullptr;
    }

    if (obj == m_uiObjectFocus) {
        // switchFocusUIObj();
        m_uiObjectFocus = nullptr;
    }
}

void CSkinWnd::onLanguageChanged() {
    m_rootConainter.onLanguageChanged();
    invalidateRect(nullptr, true);
}

void CSkinWnd::onAdjustHue(float hue, float saturation, float luminance) {
    if (!m_bSkinOpened) {
        return;
    }

    m_fontProperty.onAdjustHue(this, hue, saturation, luminance);

    m_rootConainter.onAdjustHue(hue, saturation, luminance);
}

int CSkinWnd::registerTimerObject(CUIObject *pObj, int nTimeDuration) {
    m_timerIDMax++;
    int nTimerId = m_timerIDMax;
    if (pObj != nullptr) {
        TIMER_OBJECT TimerObj;

        TimerObj.nTimeDuration = nTimeDuration;
        TimerObj.pObj = pObj;

        m_mapTimerObjs[nTimerId] = TimerObj;
    }

    setTimer(nTimerId, nTimeDuration);

    return nTimerId;
}

void CSkinWnd::unregisterTimerObject(CUIObject *pObj, int nTimerId) {
    MAP_TIMER_OBJS::iterator it = m_mapTimerObjs.find(nTimerId);
    if (it != m_mapTimerObjs.end()) {
        m_mapTimerObjs.erase(it);
    }

    killTimer(nTimerId);
}

void CSkinWnd::unregisterTimerObject(CUIObject *pObj) {
    MAP_TIMER_OBJS::iterator it;

    for (it = m_mapTimerObjs.begin(); it != m_mapTimerObjs.end(); ) {
        TIMER_OBJECT &timerObj = (*it).second;

        if (timerObj.pObj == pObj) {
            killTimer((*it).first);
            m_mapTimerObjs.erase(it);
            it = m_mapTimerObjs.begin();
        } else {
            it++;
        }
    }
}

void CSkinWnd::onTimer(uint32_t nIDEvent) {
    MAP_TIMER_OBJS::iterator it;

#ifdef _TRANSLUCENCY_ENABLED
    if (nIDEvent == TIMER_ID_DYNAMIC_TRANS) {
        onTimerDynamicAlphaChange();
        return;
    }
#endif // #ifdef _TRANSLUCENCY_ENABLED

    if (nIDEvent == TIMER_ID_TINY_JS_VM) {
        auto runtime = m_vm->defaultRuntime();
        runtime->onRunTasks();

        if (runtime->shouldGarbageCollect()) {
#if DEBUG
            auto countAllocated = runtime->countAllocated();
            auto countFreed = runtime->garbageCollect();
            printf("** CountFreed: %d, CountAllocated: %d\n", countFreed, countAllocated);
            if (countAllocated != countFreed) {
                printf("** NOT FREED **\n");
            }
#else
            runtime->garbageCollect();
#endif
        }
        return;
    } else if (nIDEvent == TIMER_ID_MOUSE_INACTIVE) {
        onTimerMouseInactive();
        return;
    } else if (nIDEvent == TIMER_ID_ANIMATION) {
        onTimerAnimation();
        return;
    } else if (nIDEvent == TIMER_ID_TOOLTIP) {
        m_skinToolTip.onTimer(nIDEvent);
        return;
    }

    assert(nIDEvent >= TIMER_ID_BEG_ALLOC);

    it = m_mapTimerObjs.find(nIDEvent);
    assert(it != m_mapTimerObjs.end());

    if (it != m_mapTimerObjs.end()) {
        (*it).second.pObj->onTimer(nIDEvent);
        return;
    }
}

void CSkinWnd::onInputText(cstr_t text) {
    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus && pObjFocus->needMsgKey()) {
        pObjFocus->onInputText(text);
    }
}

void CSkinWnd::onInputMarkedText(cstr_t text) {
    CUIObject *pObjFocus = getFocusUIObj();
    if (pObjFocus && pObjFocus->needMsgKey()) {
        pObjFocus->onInputMarkedText(text);
    }
}

void CSkinWnd::getCursorClientPos(CPoint &pt) {
    pt = getCursorPos();

    screenToClient(pt);
}


bool CSkinWnd::registerUIObjNotifyHandler(int nIDUIObj, IUIObjNotifyHandler *pHandler) {
    VecUIObjNotifyHandlers::iterator it;

    for (it = m_vUIObjNotifyHandlers.begin(); it != m_vUIObjNotifyHandlers.end(); ++it) {
        UIObjNotifyHandlerInfo &h = *it;
        if (h.nIDUIObj == nIDUIObj && h.pHandler == pHandler) {
            return false;
        }
    }

    UIObjNotifyHandlerInfo h;
    h.nIDUIObj = nIDUIObj;
    h.pHandler = pHandler;
    m_vUIObjNotifyHandlers.push_back(h);

    return true;
}


bool CSkinWnd::unregisterUIObjNotifyHandler(IUIObjNotifyHandler *pHandler) {
    bool bRet = false;

    for (int i = (int)m_vUIObjNotifyHandlers.size() - 1; i >= 0; i--) {
        UIObjNotifyHandlerInfo &h = m_vUIObjNotifyHandlers[i];
        if (h.pHandler == pHandler) {
            m_vUIObjNotifyHandlers.erase(m_vUIObjNotifyHandlers.begin() + i);
            bRet = true;
        }
    }

    return bRet;
}


void CSkinWnd::dispatchUIObjNotify(IUIObjNotify *pNotify) {
    for (int i = 0; i < (int)m_vUIObjNotifyHandlers.size(); i++) {
        UIObjNotifyHandlerInfo &h = m_vUIObjNotifyHandlers[i];
        if (h.nIDUIObj == pNotify->nID) {
            h.pHandler->onUIObjNotify(pNotify);
        }
    }

    onUIObjNotify(pNotify);
}

void CSkinWnd::callVMFunction(const JsValue &memberFunc, const Arguments &args, const JsValue &thiz) {
    auto ctx = m_vm->defaultRuntime()->mainCtx();
    ctx->error = JE_OK;
    m_vm->callMember(ctx, thiz, memberFunc, args);
}

int CSkinWnd::fromXML(SXNode *pXmlNode) {
    m_bTranslucencyLayered = false;

    getWindowRect(&m_rcBoundBox);

    m_rootConainter.m_rcObj = m_rcBoundBox;

    m_fontProperty.setProperty("Font", "Tohama,14,normal,0,0,");

    SXNode *pNodeExtends = nullptr;
    cstr_t szExtends = pXmlNode->getProperty(SZ_PN_EXTENDS);
    if (szExtends && !isEmptyString(szExtends)) {
        pNodeExtends = m_pSkinFactory->getExtendsStyle(szExtends);
    }

    if (pNodeExtends) {
        setProperies(pNodeExtends->listProperties);
    }

    setProperies(pXmlNode->listProperties);

    if (pNodeExtends) {
        applyXmlPropertyNodes(pNodeExtends);
    }
    applyXmlPropertyNodes(pXmlNode);

    m_fontProperty.create();

    // load window size of skin
    onLoadWndSizePos();

    // create UIObjects defined in Extends window
    CSkinContainer *pContainterClientArea = nullptr;
    if (pNodeExtends) {
        m_rootConainter.createChild(pNodeExtends);
        pContainterClientArea = (CSkinContainer*)m_rootConainter.getUIObjectByClassName(CSkinClientArea::className());

        // set properties to client area.
        cstr_t szDefaultPage = pXmlNode->getProperty("DefaultPage");
        if (szDefaultPage) {
            pContainterClientArea->setProperty("DefaultPage", szDefaultPage);
        }
    }

    if (pContainterClientArea == nullptr) {
        pContainterClientArea = &m_rootConainter;
    }

    pContainterClientArea->createChild(pXmlNode);

    m_bSkinOpened = true;

    //
    // Call onCreate of Every Child.
    //
    m_rootConainter.dispatchOnCreateMsg();

    //
    updateSkinProperty();

    onSkinLoaded();

    string strOnCreateCmd;
    strOnCreateCmd = pXmlNode->getPropertySafe("OnCreateCommand");

    if (!strOnCreateCmd.empty()) {
        VecStrings vStrParam;

        strSplit(strOnCreateCmd.c_str(), ',', vStrParam);
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);
            onCommand(m_pSkinFactory->getIDByName(vStrParam[i].c_str()));
        }
    }

    float hue, saturation, luminance;
    m_pSkinFactory->getResourceMgr()->getAdjustHueParam(hue, saturation, luminance);
    if (hue != 0.0f) {
        onAdjustHue(hue, saturation, luminance);
    }

    return ERR_OK;
}

#ifdef _SKIN_EDITOR_
void CSkinWnd::toXML(CXMLWriter &xmlStream) {
    xmlStream.writeStartElement(_SZ_SKINWND);

    // m_rootConainter.toXML(xmlStream);

    CUIObjProperties properties;

    enumProperties(properties);
    properties.toXMLCategoryAttrib(xmlStream);

    m_rootConainter.onToXMLChild(xmlStream);

    xmlStream.writeEndElement();
}
#endif // _SKIN_EDITOR_

void CSkinWnd::onMouseActiveMsg() {
    m_timeLatestMouseMsg = getTickCount();

    killTimer(TIMER_ID_MOUSE_INACTIVE);
    setTimer(TIMER_ID_MOUSE_INACTIVE, TIMER_SPAN_MOUSE_INACTIVE);

    if (!m_bMouseActive) {
        m_bMouseActive = true;
        onMouseActive(m_bMouseActive);
        invalidateRect();
    }
}

void CSkinWnd::onTimerMouseInactive() {
    m_bMouseActive = false;
    onMouseActive(m_bMouseActive);

    killTimer(TIMER_ID_MOUSE_INACTIVE);

    invalidateRect();
}

void CSkinWnd::onMouseActive(bool bMouseActive) {
    if (m_onMouseActivateListener.isFunction()) {
        callVMFunction(m_onMouseActivateListener, ArgumentsX(makeJsValueBool(bMouseActive)));
    }
}

void CSkinWnd::startAnimation(int nUIDAnimation) {
    for (auto &am : m_listAnimations) {
        if (am->getUIObjectID() == nUIDAnimation) {
            return;
        }
    }

    CSkinAnimationUIObj *pObj = (CSkinAnimationUIObj*)getUIObjectById(nUIDAnimation, CSkinAnimationUIObj::className());
    if (pObj) {
        if (m_listAnimations.empty()) {
            setTimer(TIMER_ID_ANIMATION, TIME_OUT_ANIMATION);
        }

        m_listAnimations.push_back(std::make_unique<CSkinAnimation>(pObj));
    } else {
        ERR_LOG1("Invalid animation id: %s", m_pSkinFactory->getStringOfID(nUIDAnimation).c_str());
    }
}

void CSkinWnd::startAnimation(CUIObject *pObjTarget, CAnimationMotion *pAnimation, int nDurationTime) {
    if (m_listAnimations.empty()) {
        setTimer(TIMER_ID_ANIMATION, TIME_OUT_ANIMATION);
    }

    m_listAnimations.push_back(std::make_unique<CSkinAnimation>(pObjTarget, pAnimation, nDurationTime));
}

void CSkinWnd::stopAnimation(int nIDAnimation) {
    for (ListAnimation::iterator it = m_listAnimations.begin(); it != m_listAnimations.end(); ++it) {
        CSkinAnimation *am = (*it).get();
        if (am->getUIObjectID() == nIDAnimation) {
            m_listAnimations.erase(it);
            if (m_listAnimations.empty()) {
                killTimer(TIMER_ID_ANIMATION);
            }
            return;
        }
    }
}

void CSkinWnd::stopAnimation(CUIObject *pObjTarget) {
    for (ListAnimation::iterator it = m_listAnimations.begin(); it != m_listAnimations.end(); ++it) {
        CSkinAnimation *am = (*it).get();
        if (am->getUIObject() == pObjTarget) {
            m_listAnimations.erase(it);
            if (m_listAnimations.empty()) {
                killTimer(TIMER_ID_ANIMATION);
            }
            return;
        }
    }
}

AnimateType CSkinWnd::getAnimationType() const {
    if (m_animateType == AT_UNKNOWN) {
        return AT_MOVE;
    }
    return m_animateType;
}

int CSkinWnd::getAnimationDuration() const {
    if (m_animateDuration > 0) {
        return m_animateDuration;
    }

    return DEFAULT_ANIMATION_DURATION;
}

void CSkinWnd::onTimerAnimation() {
    CAutoRedrawLock redrawLock(this);

    for (ListAnimation::iterator it = m_listAnimations.begin(); it != m_listAnimations.end();) {
        CSkinAnimation *am = (*it).get();
        if (!am->onAnimate()) {
            it = m_listAnimations.erase(it);
        } else {
            ++it;
        }
    }

    invalidateRect();

    if (m_listAnimations.empty()) {
        killTimer(TIMER_ID_ANIMATION);
    }
}

void CSkinWnd::onLoadWndSizePos() {
#ifdef _WIN32
    if (::GetParent(m_hWnd)) {
        return;
    }
#endif

    // WndSizeMode        sizeMode;
    CRect rcNew;
    bool bCenterWnd = true;

    if (isZoomed()) {
        restore();
    }

    getWindowRect(&m_rcBoundBox);

    rcNew = m_rcBoundBox;

    if (m_bRememberSizePos) {
        rcNew.left = g_profile.getInt(getSkinWndName(), "wnd_left", rcNew.left);
        rcNew.top = g_profile.getInt(getSkinWndName(), "wnd_top", rcNew.top);
        rcNew.right = rcNew.left + g_profile.getInt(getSkinWndName(), "wnd_width", m_nWidth);
        rcNew.bottom = rcNew.top + g_profile.getInt(getSkinWndName(), "wnd_height", m_nHeight);
        bCenterWnd = false;

        if (m_wndResizer.isFixedWidth()) {
            rcNew.right = rcNew.left + m_nWidth;
        }
        if (m_wndResizer.isFixedHeight()) {
            rcNew.bottom = rcNew.top + m_nHeight;
        }
    } else {
        rcNew.right = rcNew.left + m_nWidth;
        rcNew.bottom = rcNew.top + m_nHeight;
    }

    if (rcNew.width() < m_wndResizer.getMinCx()) {
        rcNew.right = rcNew.left + m_wndResizer.getMinCx();
    }
    if (rcNew.height() < m_wndResizer.getMinCy()) {
        rcNew.bottom = rcNew.top + m_wndResizer.getMinCy();
    }

    if (bCenterWnd) {
        CRect rcScreen;
        int w = rcNew.width(), h = rcNew.height();

        if (getMonitorRestrictRect(rcNew, rcScreen)) {
            rcNew.left = (rcScreen.left + rcScreen.right - w) / 2;
            rcNew.top = (rcScreen.top + rcScreen.bottom - h) / 2;
            rcNew.right = rcNew.left + w;
            rcNew.bottom = rcNew.top + h;
        }
    }

    if (!rcNew.equal(m_rcBoundBox)) {
        moveWindowSafely(rcNew.left, rcNew.top, rcNew.width(), rcNew.height(), true);
    }

    /*    if (!m_wndResizer.isFixedWidth() && !m_wndResizer.isFixedHeight())
    {
//         sizeMode = (WndSizeMode)g_profile.getInt(getSkinWndName(), "SizeMode", WndSizeMode_Normal);
//         if (sizeMode == WndSizeMode_Maximized)
//             dwShowFlag = SW_MAXIMIZE;
//             // showWindow(SW_MAXIMIZE);
//         else if (sizeMode == WndSizeMode_Minimized)
//             dwShowFlag = SW_MINIMIZE;
            // showWindow(SW_MINIMIZE);
    }*/

    /*    WINDOWPLACEMENT        wp;
    memset(&wp, 0, sizeof(wp));
    wp.length = sizeof(wp);
    wp.flags = 0;
    wp.ptMaxPosition.y = wp.ptMaxPosition.x = -1;
    wp.ptMinPosition.y = wp.ptMinPosition.x = -1;
    wp.rcNormalPosition = rcNew;
    wp.showCmd = dwShowFlag;
    SetWindowPlacement(getHandle(), &wp);*/
}

// COMMENT:
//        save current skin window size pos.
void CSkinWnd::saveWndPos() {
    CRect rcToSave;

#ifdef _WIN32
    if (::GetParent(m_hWnd)) {
        return;
    }

    WINDOWPLACEMENT wp;

    wp.length = sizeof(wp);
    if (GetWindowPlacement(m_hWnd, &wp)) {
        rcToSave = wp.rcNormalPosition;
    } else {
        rcToSave = m_rcBoundBox;
    }

    WndSizeMode sizeMode;
    if (isFlagSet(wp.showCmd, SW_SHOWMAXIMIZED)) {
        sizeMode = WndSizeMode_Maximized;
    } else if (isFlagSet(wp.showCmd, SW_SHOWMINIMIZED)) {
        sizeMode = WndSizeMode_Minimized;
    } else {
        sizeMode = WndSizeMode_Normal;
    }

    g_profile.writeInt(getSkinWndName(), "SizeMode", sizeMode);
#else
    if (!getWindowRect(&rcToSave)) {
        return;
    }
#endif

    g_profile.writeInt(getSkinWndName(), "wnd_left", rcToSave.left);
    g_profile.writeInt(getSkinWndName(), "wnd_top", rcToSave.top);
    g_profile.writeInt(getSkinWndName(), "wnd_width", rcToSave.width());
    g_profile.writeInt(getSkinWndName(), "wnd_height", rcToSave.height());
}
