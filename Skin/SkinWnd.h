/********************************************************************
    Created  :    2001/07/06
    FileName :    SkinWnd.h
    Author   :    xhy
    
    Purpose  :    
*********************************************************************/

#if !defined(AFX_SKIN_H__A1FDC39A_7098_11D5_9E04_02608CAD9330__INCLUDED_)
#define AFX_SKIN_H__A1FDC39A_7098_11D5_9E04_02608CAD9330__INCLUDED_

#pragma once

#include "UIObject.h"
#include "SkinFontProperty.h"
#include "SkinContainer.h"


class CSkinWnd;
class CSkinFactory;
class CSkinContainer;
class CSFImage;
class SjvmSkinWndEvent;


#define UIOBJ_ID_BEGIN_ALLOC        0xFFFFF

// COMMENT:
//        窗口的位置是否合法（显示位置是否正常）
bool isWndOutOfScreen(const CRect &rc);

// 0, 0, 10, 20
bool getRectValue(cstr_t szValue, CSFImage &image);
bool getRectValue(cstr_t szValue, int &x, int &y, int &cx, int &cy);
bool getRectValue(cstr_t szValue, CRect &rc);

inline bool isPropertyName(cstr_t szName, cstr_t szStr) { return strcasecmp(szName, szStr) == 0; }

void preMultiplyRGBColr(CColor &clr, uint8_t alpha);

string colorToStr(const CColor &clr);

// "#FFFFFF"
void getColorValue(CColor &clr, cstr_t szColor);

int getMenuKey(cstr_t szText);

//
// Base interface of UIObject Notification data
//
class IUIObjNotify
{
public:
    IUIObjNotify(CUIObject *pObject)
    {
        assert(pObject);
        pUIObject = pObject;
        nID = pObject->m_id;
    }
    virtual cstr_t getClassName() { if (pUIObject) return pUIObject->getClassName(); else return ""; }
    virtual bool isKindOf(cstr_t szClassName) { if (pUIObject) return pUIObject->isKindOf(szClassName); else return false; }

    int            nID;
    CUIObject    *pUIObject;

};


//
// Base interface of handle IUIObjNotify.
//
class IUIObjNotifyHandler
{
public:
    virtual void onUIObjNotify(IUIObjNotify *pNotify) = 0;

};


class IUICheckStatus
{
public:
    virtual bool getChecked(uint32_t nID, bool &bChecked) = 0;
    virtual bool getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) = 0;
};

class CSkinWnd :
    public Window,
    public ISkinWndDragHost,
    public IUICheckStatus
{
public:
    CSkinWnd();
    virtual ~CSkinWnd();

public:
    virtual int create(SkinWndStartupInfo &skinWndStartupInfo, CSkinFactory *pSkinFactory, bool bToolWindow = true, bool bTopmost = false, bool bVisible = true);

    virtual void destroy();

    virtual int openDefaultSkin();
    virtual int openSkin(cstr_t szSkinWndName);
    virtual void closeSkin();

    //
    // UIObject related functions
    //
    CUIObject *getUIObjectByClassName(cstr_t szClassName);
    CUIObject *getUIObjectById(int nId, cstr_t szClassName = nullptr);
    CUIObject *removeUIObjectById(int nId);
    bool removeUIObject(CUIObject *pObj, bool bFree = false);

    void setFocusUIObject(int nId);

    CUIObject *getFocusUIObj();

    CSkinContainer *getRootContainer() { return &m_rootConainter; }

    bool enableUIObject(int nId, bool bEnable = true, bool bRedraw = true);

    bool setUIObjectProperty(int nId, cstr_t szProperty, cstr_t szValue);
    bool setUIObjectProperty(cstr_t szId, cstr_t szProperty, cstr_t szValue)
        { return setUIObjectProperty(m_pSkinFactory->getIDByName(szId), szProperty, szValue); }

    void setUIObjectVisible(int nId, bool bVisible, bool bRedraw = true);
    void setUIObjectVisible(cstr_t szId, bool bVisible, bool bRedraw = true)
        { setUIObjectVisible(m_pSkinFactory->getIDByName(szId), bVisible, bRedraw); }

    void invalidateUIObject(int nId);
    void invalidateUIObject(cstr_t szId)
        { invalidateUIObject(m_pSkinFactory->getIDByName(szId)); }

    string getUIObjectText(int nId);
    string getUIObjectText(cstr_t szId)
        { return getUIObjectText(m_pSkinFactory->getIDByName(szId)); }

    void setUIObjectText(int nId, cstr_t szText, bool bRedraw = true);
    void setUIObjectText(cstr_t szId, cstr_t szText, bool bRedraw = true)
        { setUIObjectText(m_pSkinFactory->getIDByName(szId), szText, bRedraw); }

    void checkButton(int nId, bool bCheck);
    bool isButtonChecked(int nId);

    void checkToolbarButton(int nToolbarId, int nButtonId, bool bCheck);
    void checkToolbarButton(cstr_t szToolbarId, cstr_t szButtonId, bool bCheck);

    // register special messages
    void setCaptureMouse(CUIObject *pUIObj);
    CUIObject *getCaptureMouse();
    void releaseCaptureMouse(CUIObject *pUIObj);

    void unregisterTimerObject(CUIObject *pObj);
    void unregisterTimerObject(CUIObject *pObj, int nTimerId);
    int registerTimerObject(CUIObject *pObj, int nTimeDuration);

    void registerHandleContextMenuCmd(CUIObject *pObj) { m_pUIObjHandleContextMenuCmd = pObj; }

    int allocAutoUniID() { return ++m_nextAuoUniID; }

    int allocUIObjID() { return ++m_nextUIObjIDAlloc; }

    // update draw of UIObject
    virtual void invalidateUIObject(CUIObject *pObj);

    void enterInDrawUpdate();
    void leaveInDrawUpdate();
    void cancelDrawUpdate() { m_nInRedrawUpdate--; assert(m_nInRedrawUpdate >= 0); }

    CSkinFactory *getSkinFactory() const { return m_pSkinFactory; }

    void setMainAppWnd(bool bMainAppWnd);
    bool isMainAppWnd() const { return m_bMainAppWnd; }
    bool isDialogWnd() const { return m_bDialogWnd; }

    bool isInEditorMode() const { return m_bInEditorMode; }

public:
    // ISkinWndDragHost
    virtual void getWndDragAutoCloseTo(vector<Window *> &vWnd);
    virtual void getWndDragTrackMove(vector<Window *> &vWnd);

public:
    // IUICheckStatus interface
    virtual bool getChecked(uint32_t nID, bool &bChecked) { return false; }
    virtual bool getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) { return false; }

public:
    //
    // Messages of the window
    //
    virtual void onActivate(bool bActived);
    virtual void onPreCreate(bool &bTopmost, bool &bVisible) { }
    virtual void onCreate();
    virtual void onDestroy();
    virtual void onPaint(CRawGraph *surface, CRect *rcClip);

    virtual void onContexMenu(int xPos, int yPos);

    virtual void onCommand(uint32_t uID, uint32_t nNotifyCode);

    virtual void onOK();
    virtual void onCancel();

    // normal input message
    virtual void onKeyDown(uint32_t nChar, uint32_t nFlags);
    virtual void onKeyUp(uint32_t nChar, uint32_t nFlags);
    virtual void onChar(uint32_t nChar);
    virtual void onLButtonUp(uint32_t nFlags, CPoint point );
    virtual void onLButtonDown(uint32_t nFlags, CPoint point );
    virtual void onLButtonDblClk(uint32_t nFlags, CPoint point);
    virtual void onRButtonUp(uint32_t nFlags, CPoint point );
    virtual void onRButtonDown(uint32_t nFlags, CPoint point );
    virtual void onMouseDrag(uint32_t nFlags, CPoint point );
    virtual void onMouseMove(CPoint point );
    virtual void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt);

    virtual void onMove(int x, int y);

    virtual void onSize(int cx, int cy);
    virtual void onSetFocus();
    virtual void onKillFocus();

    virtual void onTimer(uint32_t nIDEvent);

    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) { }
    virtual void onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) { }

    // messages to process
    virtual void onSkinLoaded();

    virtual bool moveWindow(int X, int Y, int nWidth, int nHeight, bool bRepaint = true);
    virtual bool moveWindow(CRect &rc, bool bRepaint = true);

    virtual bool onCustomCommand(int nId);
    virtual void postCustomCommandMsg(int nId);
    virtual void postShortcutKeyCmd(int nId);

    virtual void onUserMessage(int nMessageID, LPARAM param);

    virtual void onUIObjNotify(IUIObjNotify *pNotify) { }

    virtual void onRemoveUIObj(CUIObject *pObj);
    virtual void onAddUIObj(CUIObject *pObj);

    virtual void onLanguageChanged();

    virtual void onAdjustHue(float hue, float saturation, float luminance);

    virtual void updateMemGraphicsToScreen(const CRect* lpRect);

#ifdef _WIN32
    static int ms_msgIDCustomCommand;

    virtual LRESULT wndProc(uint32_t message, WPARAM wParam, LPARAM lParam);
#endif

public:
    //
    // window alpha and transparent features
    //
    inline bool getEnableTranslucencyLayered() const { return m_bTranslucencyLayered; }
    void enableTranslucencyLayered(bool bTranslucencyLayered);
    inline int getCurrentTranslucency() const { return m_nCurTranslucencyAlpha; }

    CColor getTranslucencyColor(const CColor &clr) const;

#ifdef _WIN32_DESKTOP
    void invalidateRectOfLayeredWindow(const CRect* lpRect);
    virtual bool invalidateRect(const CRect* lpRect = nullptr, bool bErase = false);
#endif

    void startTranslucencyFade();

    void onTimerDynamicAlphaChange();

    virtual bool isClickThrough() { return m_bEnableClickThrough && m_bClickThrough && !m_bActived; }

    enum TranslucencyStatus
    {
        TS_NORMAL,
        TS_ON_ACTIVE,
        TS_ON_HOVER,
    };

    int                    m_nTranslucencyAlphaDefault;        // Alpha on inactivate
    uint32_t                m_dwTransFadeinBegTime;                // Begin time of fade in/out translucency
    int                    m_nTransAlphaBeg;                    // The alpha value of begin fade in/out.
    int                    m_nTranslucencyAlphaOnActive;        // Alpha on activate
    int                    m_nTranslucencyAlphaOnHover;        // Alpha on mouse hover
    int                    m_nCurTranslucencyAlpha;            // Current translucency alpha value

    TranslucencyStatus    m_translucencyStatus;
    bool                m_bOnMouseHover;

    bool                m_bEnableClickThrough;


public:
    void setProperies(SXNode::ListProperties &listProperties);
    virtual bool setProperty(cstr_t szProperty, cstr_t szValue);
#ifdef _SKIN_EDITOR_
    virtual void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    bool getUnprocessedProperty(cstr_t szProperty, string &strValue);

#ifdef _SKIN_EDITOR_
    virtual void toXML(CXMLWriter &xmlStream);
#endif // _SKIN_EDITOR_

    bool updateSkinProperty();

    bool isWndActive();

    CSkinFontProperty *getFontProperty() { return &m_fontProperty; }
    CRawBmpFont *getRawFont() { return m_fontProperty.getFont(); }

    cstr_t getSkinWndName() { return m_strSkinWndName.c_str(); }
    cstr_t getCaptionText() { return m_strCaption.c_str(); }
    void setCaptionText(cstr_t szCaption, bool bRedraw = false) {
        assert(szCaption);
        m_strCaption = szCaption;
        setTitle(m_strCaption.c_str());
        if (bRedraw) {
            invalidateRect();
        }
    }

    cstr_t getMenuName() { return m_strMenuName.c_str(); }

    bool isWindowsAppearance() const { return m_bWindowsAppearance; }

    // after process WM_DESTORY, delete this.
    void setFreeOnDestory(bool bFreeOnDestory = true) { m_bFreeOnDestory = bFreeOnDestory; }

    CRect &getBoundBox() { return m_rcBoundBox; }

public:
    void getCursorClientPos(CPoint &pt);

    void addWndCloseto(Window *pWnd, cstr_t szWndClass, cstr_t szWndName);

    void trackMove(Window *pWnd, int x, int y);

    void addTool(cstr_t szText, CRect *lpRectTool = nullptr, uint32_t nIDTool = 0);
    void delTool(uint32_t nIDTool);


    bool registerUIObjNotifyHandler(int nIDUIObj, IUIObjNotifyHandler *pHandler);
    bool unregisterUIObjNotifyHandler(IUIObjNotifyHandler *pHandler);

    void dispatchUIObjNotify(IUIObjNotify *pNotify);

protected:
    void rootContainerFromXml(SXNode *pSkinWndNode, CSkinContainer *pRootContainer);

    virtual int fromXML(SXNode *pXmlNode);

    void onSwitchChildFocus(CUIObject *pUIObjFocusOld, CUIObject *pUIObjFocusNew);

    virtual void onLoadWndSizePos();
    void saveWndPos();

    void processMouseMove(CPoint point);

    friend class CSkinContainer;
    friend class CUIObject;
    friend class CSjvmSkinWnd;

protected:
    //
    // Hide UIObject, if mouse is inactive in 3 seconds.
    //
    uint32_t            m_nTimeLatestMouseMsg;
    bool            m_bMouseActive;

    void onMouseActiveMsg();
    void onTimerMouseInactive();
    virtual void onMouseActive(bool bMouseActive);

public:
    class CAutoRedrawLock {
    public:
        CAutoRedrawLock(CSkinWnd *pSkinWnd) { pSkinWnd->enterInDrawUpdate(); m_pSkinWnd = pSkinWnd; }
        ~CAutoRedrawLock() { if (m_pSkinWnd) m_pSkinWnd->leaveInDrawUpdate(); }
        void cancelDrawUpdate() {
            if (m_pSkinWnd) {
                m_pSkinWnd->cancelDrawUpdate(); m_pSkinWnd = nullptr;
            }
        }

    protected:
        CSkinWnd    *m_pSkinWnd;

    };

public:
    //
    // Public method for Hide UIObject, if mouse is inactive.
    //
    bool isMouseActive() { return m_bMouseActive; }

public:
    CSkinWndDrag    m_WndDrag;
    CSkinWndResizer    m_wndResizer;

    // default width and height
    int                m_nWidth, m_nHeight;
    bool            m_bRememberSizePos;

protected:
    bool            m_bFreeOnDestory;
    bool            m_bManageBySkinFactory;

    bool            m_bOnDestroy;

    // flag to indicate whether the skin was opened.
    bool            m_bSkinOpened;
    bool            m_bInEditorMode;    // Is in skin editor mode?

    bool            m_needRedraw;
    int                m_nInRedrawUpdate;

    CSkinFactory    *m_pSkinFactory;

    string            m_strSkinWndName;        // skin 名
    string            m_strCaption;

    string            m_strMenuName;

    CSkinToolTip    m_wndToolTip;

    //
    // Status
    //
    bool            m_bActived;        // 当前窗口是否为激活状态Focus?
    bool            m_bMainAppWnd;    // true: when the Window is closed, exit process.
    bool            m_bDialogWnd;    // For dialog Skin: Enter key = OK, Escape key = cancel.

    double            m_dbScale;        // 窗口是否进行缩放
    CRect            m_rcReal;        // 窗口的实际大小

    //
    // properties
    //
    Cursor            m_cursor;
    string            m_strCusor;        // 光标文件

    bool            m_bWindowsAppearance;
    bool            m_bAeroGlass;

     // skin region
    CRect            m_rcBoundBox;    // 当使用region时，它是区域m_rgnWnd的边框
                                    // 当不使用region，它是skin的矩形。

    CSkinFontProperty    m_fontProperty;    // Default font property.

    // Unprocessed properties
    SXNode::ListProperties    m_listUnprocessedProperties;

#ifndef _WIN32
    CRect            m_rcMemUpdate;
#endif

    //
    // Timer IDs
    //
    typedef struct TIMER_OBJECT
    {
        CUIObject    *pObj;
        int            nTimeDuration;
    }TIMER_OBJECT;
    int                m_timerIDMax;

    typedef map<int, TIMER_OBJECT>    MAP_TIMER_OBJS;
    MAP_TIMER_OBJS    m_mapTimerObjs;


    // root skin uiobject container
    CSkinClientArea    m_rootConainter;

    // only one ui object can capture the mouse at any time
    CUIObject        *m_pUIObjCapMouse;

    // The UIObject registered to handle context menu command.
    CUIObject        *m_pUIObjHandleContextMenuCmd;

    // auto allocated uni-ID
    int                m_nextAuoUniID;

    int                m_nextUIObjIDAlloc;

    bool            m_bIgnoreNextOnCharMsg;

    //
    // UIObject notification handler : register, Unregister and dispatch
    //
    struct UIObjNotifyHandlerInfo
    {
        int                    nIDUIObj;
        IUIObjNotifyHandler    *pHandler;
    };
    typedef vector<UIObjNotifyHandlerInfo>    VecUIObjNotifyHandlers;
    VecUIObjNotifyHandlers        m_vUIObjNotifyHandlers;

    // exchange Pool is used to exchange data between pages.
    MapStrings                m_mapExchangePool;

    //
    // Skin script message delivery
    //
public:
    void sjvmSetNotifyEvent(SjvmSkinWndEvent *pNotify) { m_pSjvmNotifyEvent = pNotify; }

protected:
    SjvmSkinWndEvent    *m_pSjvmNotifyEvent;

    //
    // Skin animation object
    //
public:
    void startAnimation(int nUIDAnimation);
    void startAnimation(CUIObject *pObjTarget, int nAnimateId, int nDurationTime);
    void startAnimation(CUIObject *pObjTarget, CAnimationMotion *pAnimation, int nDurationTime);
    void stopAnimation(int nIDAnimation);
    void stopAnimation(CUIObject *pObjTarget);

    AnimateType getAnimationType() const;
    int getAnimationDuration() const;

protected:
    typedef list<std::unique_ptr<CSkinAnimation>>    ListAnimation;
    ListAnimation                    m_listAnimations;

    AnimateType                        m_animateType;
    int                                m_animateDuration;

    void onTimerAnimation();

};

#endif // !defined(AFX_SKIN_H__A1FDC39A_7098_11D5_9E04_02608CAD9330__INCLUDED_)
