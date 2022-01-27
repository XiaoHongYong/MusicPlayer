
#pragma once

#include "SkinWnd.h"

class IPopupSkinWndNotify
{
public:
    virtual void popupSkinWndOnSelected() { }
    // virtual void popupSkinWndOnDestroy() { }

};

class CPopupSkinWnd : public CSkinWnd
{
public:
    CPopupSkinWnd();
    ~CPopupSkinWnd();

    virtual int create(CSkinFactory *pSkinFactory, cstr_t szSkinWndName,
        Window *pWndParent, IPopupSkinWndNotify *pNotify, const CRect &rc);

    virtual void onKillFocus();

    void onKeyDown(uint32_t nChar, uint32_t nFlags);

protected:
    IPopupSkinWndNotify            *m_pPopupSkinWndNotify;

};
