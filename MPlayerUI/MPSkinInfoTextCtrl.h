﻿#pragma once

#ifndef MPlayerUI_MPSkinInfoTextCtrl_h
#define MPlayerUI_MPSkinInfoTextCtrl_h


#include "../Skin/SkinScrollText.h"


class CMPSkinInfoTextCtrl : public CSkinScrollText, public IEventHandler {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinScrollText)
public:
    CMPSkinInfoTextCtrl();
    virtual ~CMPSkinInfoTextCtrl();

    void onCreate() override;

    virtual void onEvent(const IEvent *pEvent) override;

    bool onMouseMove(CPoint point) override;

    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;

    void onTimer(int nId) override;

    void draw(CRawGraph *canvas) override;

protected:
    void updateShowDefaultInfo();

protected:
    int                         m_nTimerIDHideInfo;
    string                      m_strCmd;
    Cursor                      m_cursor;

};

#endif // !defined(MPlayerUI_MPSkinInfoTextCtrl_h)
