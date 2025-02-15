#pragma once

#ifndef MPlayerUI_MPSkinFactory_h
#define MPlayerUI_MPSkinFactory_h


#include "../Skin/SkinFactory.h"


class CSkinMenu;

class CMPSkinFactory : public CSkinFactory {
public:
    CMPSkinFactory(CSkinApp *pApp, UIObjectIDDefinition uidDefinition[]);
    virtual ~CMPSkinFactory();

    virtual int init() override;

    virtual CSkinWnd *newSkinWnd(cstr_t szSkinWndName, bool bMainWnd) override;

    void topmostAll(bool bTopmost);
    void minizeAll(bool bSilently = false);
    void restoreAll();

    virtual int getIDByNameEx(cstr_t szId, string &strToolTip) override;
    virtual string getTooltip(int nId) override;

    virtual void adjustHue(float hue, float saturation = 0.5, float luminance = 0.5) override;

    void allUpdateTransparent();
    void setClickThrough(bool bClickThrough);
    bool getClickThrough() const { return m_bClickThrough; }

protected:
    bool                        m_bClickThrough;

protected:
    virtual SkinMenuPtr newSkinMenu(CSkinWnd *pWnd, const rapidjson::Value &items) override;

    SXNode *getMenuOfMenus(SXNode *pNodeMenus, cstr_t szMenuName);

};

#endif // !defined(MPlayerUI_MPSkinFactory_h)
