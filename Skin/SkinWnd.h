/********************************************************************
    Created  :    2001/07/06
    FileName :    SkinWnd.h
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#ifndef Skin_SkinWnd_h
#define Skin_SkinWnd_h

#pragma once

#include <functional>
#include "UIObject.h"
#include "SkinToolTip.h"
#include "SkinContainer.h"
#include "interpreter/VirtualMachine.hpp"


class CSkinWnd;
class CSkinFactory;
class CSkinContainer;
class CSFImage;


#define UIOBJ_ID_BEGIN_ALLOC 0xFFFFF

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
inline CColor getColorValue(cstr_t str) { CColor c; getColorValue(c, str); return c;}
inline CColor parseColorString(cstr_t str) { CColor c; getColorValue(c, str); return c;}

int getMenuKey(cstr_t szText);

//
// Base interface of UIObject Notification data
//
class IUIObjNotify {
public:
    IUIObjNotify(CUIObject *pObject) {
        assert(pObject);
        pUIObject = pObject;
        nID = pObject->m_id;
    }
    virtual cstr_t getClassName() { if (pUIObject) return pUIObject->getClassName(); else return ""; }
    virtual bool isKindOf(cstr_t szClassName) { if (pUIObject) return pUIObject->isKindOf(szClassName); else return false; }

    int                         nID;
    CUIObject                   *pUIObject;

};


//
// Base interface of handle IUIObjNotify.
//
class IUIObjNotifyHandler {
public:
    virtual void onUIObjNotify(IUIObjNotify *pNotify) = 0;

};


class IUICheckStatus {
public:
    virtual bool getChecked(uint32_t nID, bool &bChecked) = 0;
    virtual bool getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) = 0;
};

class CSkinWnd : public Window, public ISkinWndDragHost, public IUICheckStatus {
public:
    CSkinWnd();
    virtual ~CSkinWnd();

public:
    virtual int create(SkinWndStartupInfo &skinWndStartupInfo, CSkinFactory *pSkinFactory, bool bToolWindow = true, bool bTopmost = false, bool bVisible = true);

    virtual void destroy() override;

    virtual int openDefaultSkin();
    virtual int openSkin(cstr_t szSkinWndName);
    virtual void closeSkin();

    int getIDByName(cstr_t szId) { return m_pSkinFactory->getIDByName(szId); }

    virtual CUIObject *createUIObject(cstr_t className, CSkinContainer *container);

    //
    // UIObject related functions
    //
    CUIObject *getUIObjectByClassName(cstr_t className);
    CUIObject *getUIObjectById(int nId, cstr_t className = nullptr);
    CUIObject *getUIObjectById(cstr_t idName, cstr_t className = nullptr);
    CUIObject *getUIObjectById(const string &idName, cstr_t className = nullptr)
        { return getUIObjectById(idName.c_str(), className); }
    CUIObject *removeUIObjectById(int nId);
    bool removeUIObject(CUIObject *pObj, bool bFree = false);

    void setFocusUIObj(CUIObject *obj);

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
    void setUIObjectText(int id, const string &text, bool isRedraw = true)
        { setUIObjectText(id, text.c_str(), isRedraw); }
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
    virtual void getWndDragAutoCloseTo(vector<Window *> &vWnd) override;
    virtual void getWndDragTrackMove(vector<Window *> &vWnd) override;

public:
    // IUICheckStatus interface
    virtual bool getChecked(uint32_t nID, bool &bChecked) override { return false; }
    virtual bool getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) override { return false; }

public:
    //
    // Messages of the window
    //
    virtual void onActivate(bool bActived) override;
    virtual void onPreCreate(bool &bTopmost, bool &bVisible) { }
    virtual void onCreate() override;
    virtual void onDestroy() override;
    virtual void onPaint(CRawGraph *surface, CRect *rcClip) override;

    virtual void onContexMenu(int xPos, int yPos) override;

    virtual void onCommand(uint32_t id) override;

    virtual void onOK();
    virtual void onCancel();

    // normal input message
    virtual bool onKeyDown(uint32_t nChar, uint32_t nFlags) override;
    virtual bool onKeyUp(uint32_t nChar, uint32_t nFlags) override;
    virtual void onChar(uint32_t nChar) override;
    virtual void onLButtonUp(uint32_t nFlags, CPoint point ) override;
    virtual void onLButtonDown(uint32_t nFlags, CPoint point ) override;
    virtual void onLButtonDblClk(uint32_t nFlags, CPoint point) override;
    virtual void onRButtonUp(uint32_t nFlags, CPoint point ) override;
    virtual void onRButtonDown(uint32_t nFlags, CPoint point ) override;
    virtual void onMouseDrag(uint32_t nFlags, CPoint point ) override;
    virtual void onMouseMove(CPoint point ) override;
    virtual void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) override;
    virtual void onMagnify(float magnification) override;

    virtual void onMove(int x, int y) override;

    virtual void onSize(int cx, int cy) override;
    virtual void onSetFocus() override;
    virtual void onKillFocus() override;

    virtual void onTimer(uint32_t nIDEvent) override;

    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override { }
    virtual void onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override { }

    // 最终输入的文字
    virtual void onInputText(cstr_t text) override;
    // MarketText 是临时的文字，当输入其他字符时会被替代
    virtual void onInputMarkedText(cstr_t text) override;

    // messages to process
    virtual void onSkinLoaded();

    virtual bool moveWindow(int X, int Y, int nWidth, int nHeight, bool bRepaint = true);
    virtual bool moveWindow(CRect &rc, bool bRepaint = true);

    virtual void postCustomCommandMsg(int nId);
    virtual void postShortcutKeyCmd(int nId);

    virtual void postExecOnMainThread(const std::function<void()> &f);

    virtual void onUserMessage(int nMessageID, LPARAM param) override;

    virtual void onUIObjNotify(IUIObjNotify *pNotify) { }

    virtual void onRemoveUIObj(CUIObject *obj);
    virtual void onAddUIObj(CUIObject *obj);
    virtual void onUIObjectHidden(CUIObject *obj);

    virtual void onLanguageChanged() override;

    virtual void onAdjustHue(float hue, float saturation, float luminance);

    virtual void updateMemGraphicsToScreen(const CRect* lpRect);

#ifdef _WIN32
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

    // 窗口磨砂玻璃背景：<Window Glass="true"/> 后，透明区域透出系统 blur（mac NSVisualEffect，
    // win DWM blur-behind）。
    inline bool getGlassEffect() const { return m_bGlassEffect; }
    void setGlassEffect(bool bGlassEffect, cstr_t szMaterial = nullptr) override;

#ifdef _WIN32
    void invalidateRectOfLayeredWindow(const CRect* lpRect);
    virtual bool invalidateRect(const CRect* lpRect = nullptr, bool bErase = false);
#endif

    void startTranslucencyFade();

    void onTimerDynamicAlphaChange();

    virtual bool isClickThrough() override { return m_bEnableClickThrough && m_bClickThrough && !m_bActived; }

    enum TranslucencyStatus {
        TS_NORMAL,
        TS_ON_ACTIVE,
        TS_ON_HOVER,
    };

    int                         m_nTranslucencyAlphaDefault; // Alpha on inactivate
    int64_t                     m_dwTransFadeinBegTime; // Begin time of fade in/out translucency
    int                         m_nTransAlphaBeg;   // The alpha value of begin fade in/out.
    int                         m_nTranslucencyAlphaOnActive; // Alpha on activate
    int                         m_nTranslucencyAlphaOnHover; // Alpha on mouse hover
    int                         m_nCurTranslucencyAlpha; // Current translucency alpha value

    TranslucencyStatus          m_translucencyStatus;
    bool                        m_bOnMouseHover;

    bool                        m_bEnableClickThrough;

    // 窗口磨砂玻璃背景是否开启（<Window Glass="true"/>）
    bool                        m_bGlassEffect;
    string                      m_strGlassMaterial;
    // 磨砂背板圆角裁剪半径（<Window GlassRadius="10"/>，-1=未设置，mac 用 10）
    int                         m_nGlassRadius;


public:
    void setProperies(SXNode::ListProperties &listProperties);
    virtual bool setProperty(cstr_t szProperty, CSXNodeProperty *pProperties);
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

    void setSkinMenuPopup(bool isPopup) { m_isSkinMenuPopup = isPopup; }

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

    // after process WM_DESTORY, delete this.
    void setFreeOnDestory(bool bFreeOnDestory = true) { m_bFreeOnDestory = bFreeOnDestory; }

    CRect &getBoundBox() { return m_rcBoundBox; }

public:
    void getCursorClientPos(CPoint &pt);

    void addWndCloseto(Window *pWnd, cstr_t szWndClass, cstr_t szWndName);

    void addToolTip(const StringView &text, CRect &rc, uint32_t idTooltip);
    void delToolTip(uint32_t nIDTool);


    bool registerUIObjNotifyHandler(int nIDUIObj, IUIObjNotifyHandler *pHandler);
    bool unregisterUIObjNotifyHandler(IUIObjNotifyHandler *pHandler);

    void dispatchUIObjNotify(IUIObjNotify *pNotify);

public:
    JsVirtualMachine *getVM() { return m_vm; }

    void callVMFunction(const JsValue &memberFunc, const Arguments &args = Arguments(), const JsValue &thiz = jsValueGlobalThis);

protected:
    virtual int fromXML(SXNode *pXmlNode);
    void applyXmlPropertyNodes(SXNode *pXmlNode);

    void switchFocusUIObj(bool toPrev=false);

    virtual void onLoadWndSizePos();
    void saveWndPos();

    void processMouseMove(CPoint point);

    void onExecOnMainThread();

    friend class CSkinContainer;
    friend class CUIObject;

protected:
    //
    // Hide UIObject, if mouse is inactive in 3 seconds.
    //
    int64_t                     m_timeLatestMouseMsg;
    bool                        m_bMouseActive;

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
        CSkinWnd                    *m_pSkinWnd;

    };

public:
    //
    // Public method for Hide UIObject, if mouse is inactive.
    //
    bool isMouseActive() { return m_bMouseActive; }

public:
    CSkinWndDrag                m_WndDrag;
    CSkinWndResizer             m_wndResizer;

    // default width and height
    int                         m_nWidth, m_nHeight;
    bool                        m_bRememberSizePos;

protected:
    bool                        m_bFreeOnDestory;
    bool                        m_bManageBySkinFactory;

    bool                        m_bOnDestroy;

    // flag to indicate whether the skin was opened.
    bool                        m_bSkinOpened;
    bool                        m_bInEditorMode;    // Is in skin editor mode?

    bool                        m_needRedraw;
    int                         m_nInRedrawUpdate;

    CSkinFactory                *m_pSkinFactory;

    string                      m_strSkinWndName;   // skin 名
    string                      m_strCaption;

    string                      m_strMenuName;

    CSkinToolTip                m_skinToolTip;

    //
    // Status
    //
    bool                        m_bActived;         // 当前窗口是否为激活状态Focus?
    bool                        m_bMainAppWnd;      // true: when the Window is closed, exit process.
    bool                        m_bDialogWnd;       // For dialog Skin: Enter key = OK, Escape key = cancel.

    bool                        m_isSkinMenuPopup;

    //
    // properties
    //
    CRect                       m_rcBoundBox;       // skin的矩形

    CSkinFontProperty           m_fontProperty;     // Default font property.

    // WindowImage：copy 铺满整窗底座（圆角外透明一并写入，一般不必再叠 Frame）
    CScaleImagePainter          m_bgImagePainter;
    CSFImage                    m_imageBg, m_imageBgMask;

    // Unprocessed properties
    SXNode::ListProperties      m_listUnprocessedProperties;

#ifndef _WIN32
    CRect                       m_rcMemUpdate;
#endif

    //
    // Timer IDs
    //
    struct TIMER_OBJECT {
        CUIObject                   *pObj;
        int                         nTimeDuration;
    };
    int                         m_timerIDMax;

    typedef map<int, TIMER_OBJECT>    MAP_TIMER_OBJS;
    MAP_TIMER_OBJS              m_mapTimerObjs;


    // root skin uiobject container
    CSkinClientArea             m_rootConainter;

    // only one ui object can capture the mouse at any time
    CUIObject                   *m_pUIObjCapMouse;

    // The UIObject registered to handle context menu command.
    CUIObject                   *m_pUIObjHandleContextMenuCmd;

    CUIObject                   *m_uiObjectFocus = nullptr;

    // auto allocated uni-ID
    int                         m_nextAuoUniID;

    int                         m_nextUIObjIDAlloc;

    bool                        m_bIgnoreNextOnCharMsg;

    //
    // UIObject notification handler : register, Unregister and dispatch
    //
    struct UIObjNotifyHandlerInfo {
        int                         nIDUIObj;
        IUIObjNotifyHandler         *pHandler;
    };
    typedef vector<UIObjNotifyHandlerInfo>    VecUIObjNotifyHandlers;
    VecUIObjNotifyHandlers      m_vUIObjNotifyHandlers;

    // exchange Pool is used to exchange data between pages.
    MapStrings                  m_mapExchangePool;

    //
    // 将在主线程待执行的函数列表
    //
    std::recursive_mutex        m_mutex;
    std::list<std::function<void()>> m_functionsToExecOnMainThread;

    //
    // Skin script releated members.
    //
protected:
    friend class JsSkinDocument;

    string                      m_scriptFile;
    JsVirtualMachine            *m_vm;

    JsValue                     m_onMouseMoveListener;
    JsValue                     m_onSizeListener;
    JsValue                     m_onActivateListener;
    JsValue                     m_onDestoryListener;
    JsValue                     m_onCommandListener;
    JsValue                     m_onMouseActivateListener;

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
    ListAnimation               m_listAnimations;

    AnimateType                 m_animateType;
    int                         m_animateDuration;

    void onTimerAnimation();

};

#endif // !defined(Skin_SkinWnd_h)
