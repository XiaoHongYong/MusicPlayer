#pragma once

#ifndef MPlayerUI_MediaAlbumArtCtrl_h
#define MPlayerUI_MediaAlbumArtCtrl_h


#include "CurMediaAlbumArt.h"

class CMediaAlbumArtCtrl : public CUIObject, public IEventHandler {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    CMediaAlbumArtCtrl();
    virtual ~CMediaAlbumArtCtrl();

    void onCreate() override;

    virtual void onEvent(const IEvent *pEvent) override;

    virtual bool onLButtonUp( uint32_t nFlags, CPoint point ) override;

    virtual bool onLButtonDown(uint32_t nFlags, CPoint point) override;

    void onTimer(int nId) override;

    void onSize() override;

    void draw(CRawGraph *canvas) override;

    bool setProperty(cstr_t szProperty, cstr_t szValue) override;
#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

protected:
    void updateAlbumArt();

    void createAlbumArtImage(RawImageDataPtr pImgSrc);

    void resizeAlbumArt();

    bool isMaskImageUsed() { return m_frmMask.isValid(); }

    void beginArtCrossfade();
    void finishArtCrossfade();
    void startArtCrossfadeTimer();
    void stopArtCrossfadeTimer();
    bool isArtCrossfading() const { return m_artCrossfadeProgress < 1.f && m_imgPrev.isValid(); }
    void drawAlbumArtImage(CRawGraph *canvas, CSFImage &img);

    CCurMediaAlbumArt           m_mediaAlbumArt;

    string                      m_strImgFile;
    CSFImage                    m_img;
    CSFImage                    m_imgPrev;          // 多封面切换交叉淡入时的上一张
    float                       m_artCrossfadeProgress = 1.f;
    int                         m_nTimerIdCycle = 0;
    int                         m_nTimerIdFade = 0;

    // no album art mask image
    CSFImage                    m_imgOrg;           // save original album art for resize.

    // album art frame mask image
    string                      m_strFrmMask;
    CSFImage                    m_frmMask;

    CColor                      m_clrBg;

};

#endif // !defined(MPlayerUI_MediaAlbumArtCtrl_h)
