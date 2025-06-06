#pragma once

#include <stack>
#include "UIObject.h"


class CSkinMenu;
using SkinMenuPtr = std::shared_ptr<CSkinMenu>;

class CSkinContainer : public CUIObject {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    enum ResultCode {
        RESULT_OK                   = 0,
        RESULT_CANCELD              = 1,
    };

    CSkinContainer();
    virtual ~CSkinContainer();

    virtual void onInitialUpdate() override;
    virtual void onDestroy() override;

    virtual bool onMouseDrag(CPoint point) override;
    virtual bool onMouseMove(CPoint point) override;
    virtual void onMouseLeave(CPoint point) override;
    virtual bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    virtual bool onLButtonDblClk(uint32_t nFlags, CPoint point) override;
    virtual bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    virtual bool onRButtonUp(uint32_t nFlags, CPoint point) override;
    virtual bool onRButtonDown(uint32_t nFlags, CPoint point) override;

    virtual bool onMenuKey(uint32_t nChar, uint32_t nFlags) override;

    virtual void onSize() override;

    virtual bool onCommand(uint32_t nId) override;

    virtual void onSwitchTo();

    // For default result code, please see ResultCode definition.
    virtual void onPageViewResult(int nRequestCodeOfPage, int nResultCode) { }

    virtual bool onUserMessage(int nMessageID, LPARAM param);

    virtual bool onClose(); // Return false, if don't want to be closed.
    virtual bool onOK();
    virtual bool onCancel();

    virtual void draw(CRawGraph *canvas) override;

    virtual int fromXML(SXNode *pXmlNode) override;
    void createChild(SXNode *pXmlNode);

    virtual bool setProperty(cstr_t szProperty, cstr_t szValue) override;
#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    virtual void onLanguageChanged() override;

public:
    // functions for CSkinWnd
    void destroy();

    CUIObject *getUIObjectByClassName(cstr_t szClassName);
    CUIObject *getUIObjectById(int nId, cstr_t szClassName);
    CUIObject *getUIObjectById(cstr_t szId, cstr_t szClassName);
    CUIObject *getUIObjectAtPosition(const CPoint &pos);

    bool enableUIObject(int nId, bool bEnable = true, bool bRedraw = true);
    bool enableUIObject(cstr_t szId, bool bEnable = true, bool bRedraw = true);

    bool setUIObjectProperty(int nId, cstr_t szProperty, cstr_t szValue);
    bool setUIObjectProperty(cstr_t szId, cstr_t szProperty, cstr_t szValue);

    void setUIObjectVisible(int nId, bool bVisible, bool bRedraw = true);
    void setUIObjectVisible(cstr_t szId, bool bVisible, bool bRedraw = true);

    void invalidateUIObject(int nId);
    void invalidateUIObject(cstr_t szId);

    string getUIObjectText(int nId);
    string getUIObjectText(cstr_t szId);

    bool getUIObjectRect(int nId, CRect &rc);
    bool getUIObjectRect(cstr_t szId, CRect &rc);

    void setUIObjectText(int nId, cstr_t szText, bool bRedraw = true);
    void setUIObjectText(cstr_t szId, cstr_t szText, bool bRedraw = true);

    void checkButton(int nId, bool bCheck);
    void checkButton(cstr_t szId, bool bCheck);
    bool isButtonChecked(int nId);
    bool isButtonChecked(cstr_t szId);

    void checkToolbarButton(int nToolbarId, int nButtonId, bool bCheck);
    void checkToolbarButton(cstr_t szToolbarId, cstr_t szButtonId, bool bCheck);

    bool removeUIObject(CUIObject *pObj, bool bFree);

    CUIObject *removeUIObjectById(int nId);

    bool addUIObject(CUIObject *pObj);
    bool insertUIObjectAt(CUIObject *pObjWhere, CUIObject *pObj);

    virtual void recalculateUIObjSizePos(CUIObject *pObj);
    bool recalculateVarValue(CFormula &form, int &nRetValue);

    CSkinMenu *getMenu() { return m_pMenu.get(); }

    //
    // CSkinContainer API
    //
    bool isChild(CUIObject *pObj);

    virtual void invalidateUIObject(CUIObject *pObj);

    virtual void updateMemGraphicsToScreen(const CRect* lpRect, CUIObject *pObjCallUpdate);

    size_t getChildrenCount() { return m_vUIObjs.size(); }
    CUIObject *getChildByIndex(int nIndex)
        { if (nIndex >= 0 && nIndex < (int)m_vUIObjs.size()) return m_vUIObjs[nIndex]; else return nullptr; }

    CUIObject *getChildByClass(cstr_t szClassName);

    virtual bool isContainer() const override { return true; }
    virtual CSkinContainer *getContainerIf() override { return this; }

    virtual void onAdjustHue(float hue, float saturation, float luminance) override;
    virtual void onSkinFontChanged() override;

    virtual void onSetChildVisible(CUIObject *pChild, bool bVisible, bool bRedraw) { }

    virtual CRawGraph *getMemGraph();

    void dispatchOnCreateMsg();

    // Page view APIs
    // void switchToPage(int nIdPage, bool bWaitResultOfNextPage, int nRequestCodeOfNextPage, bool bAnimation);
    void switchToPage(cstr_t szPageClass, bool bWaitResultOfNextPage, int nRequestCodeOfNextPage, bool bAnimation);
    void switchToPage(CSkinContainer *toActivate, bool bWaitResultOfNextPage, int nRequestCodeOfNextPage, bool bAnimation);
    void switchToLastPage(int nResultCodeOfPage, bool bAnimation);
    bool hasActivePage() const { return m_vStackPageView.size() > 0; }

    void setExPool(cstr_t szProperty, cstr_t szValue);
    void setExPool(cstr_t szProperty, int value);

    string getExPoolStr(cstr_t szProperty, cstr_t szDefault = "");
    int getExPoolInt(cstr_t szProperty, int nDefault = 0);
    bool getExPoolBool(cstr_t szProperty, bool bDefault = false);

#ifdef DEBUG
    void dumpUIObject(int &nDeep, CPoint *pt);
#endif

    void processMouseEnterLeave(CPoint point);

protected:
    bool isPageAtLeft(CSkinContainer *pPage, CSkinContainer *pPage2);

    void onAdd(CUIObject *pObj);

    void callChildMouseLeave(CPoint point);

protected:
#ifdef _SKIN_EDITOR_
    virtual bool hasXMLChild();
    virtual void onToXMLChild(CXMLWriter &xmlStream);
#endif // _SKIN_EDITOR_

    friend class CSkinWnd;

protected:
    // All the children UIObject

    VecUIObjects                m_vUIObjs;

    bool                        m_bClipChildren;
    string                      m_strContextMenu;
    SkinMenuPtr                 m_pMenu;

    // Status for Page view
    struct PageViewItem {
        bool                        bWaitResultOfNextPage;
        int                         nRequestCodeOfNextPage;
        CSkinContainer              *pContainerPage;
        PageViewItem(CSkinContainer *pContainerPage, bool bWaitResultOfNextPage, int nRequestCodeOfNextPage) {
            this->bWaitResultOfNextPage = bWaitResultOfNextPage;
            this->nRequestCodeOfNextPage = nRequestCodeOfNextPage;
            this->pContainerPage = pContainerPage;
        }
    };
    typedef std::stack<PageViewItem>    VecStackPageView;
    VecStackPageView            m_vStackPageView;
    string                      m_strDefaultPageClass;

};

class CSkinClientArea : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    virtual bool setProperty(cstr_t szProperty, cstr_t szValue) override
        { return CSkinContainer::setProperty(szProperty, szValue); }

};
