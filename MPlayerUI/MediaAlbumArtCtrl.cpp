#include "MPlayerApp.h"
#include "MediaAlbumArtCtrl.h"


RawImageDataPtr convertTo32BppRawImage(const RawImageDataPtr &src);

/**
 * 按照 @wDst 和 @hDst 缩放和裁剪 @srcImageData 到刚好是 @wDst, @hDst 的大小
 */
RawImageDataPtr createScaleImage(const RawImageDataPtr &srcImageData, int wDst, int hDst) {
    assert(srcImageData);

    if (!srcImageData || srcImageData->height == 0 || srcImageData->width == 0) {
        return nullptr;
    }

    RawImageDataPtr dstImageData = createRawImageData(wDst, hDst, srcImageData->bitCount);

    CRawImage imgSrc(srcImageData);
    CRawImage imgDst(dstImageData);

    int xSrc = 0, ySrc = 0, wSrc = imgSrc.width(), hSrc = imgSrc.height();

    float ratio = (float)wDst / hDst;
    if ((float)imgSrc.width() / imgSrc.height() < ratio) {
        // 原来的图片太高了，截断一部分
        ySrc = (imgSrc.height() - imgSrc.width() / ratio) / 2;
        hSrc -= ySrc * 2;
    } else {
        // 原来的图片太宽了，截断一部分
        xSrc = (imgSrc.width() - imgSrc.height() * ratio) / 2;
        wSrc -= xSrc * 2;
    }

    imgSrc.stretchBlt(&imgDst, 0, 0, wDst, hDst, xSrc, ySrc, wSrc, hSrc, BPM_COPY | BPM_BILINEAR);

    return dstImageData;
}



UIOBJECT_CLASS_NAME_IMP(CMediaAlbumArtCtrl, "AlbumArt")

CMediaAlbumArtCtrl::CMediaAlbumArtCtrl() {
    m_msgNeed = UO_MSG_WANT_LBUTTON;

    m_clrBg.set(RGB(0, 0, 0));
}

CMediaAlbumArtCtrl::~CMediaAlbumArtCtrl() {
    m_pSkin->unregisterTimerObject(this);
}

void CMediaAlbumArtCtrl::onCreate() {
    CUIObject::onCreate();

    registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_MEDIA_CHANGED, ET_PLAYER_CUR_MEDIA_INFO_CHANGED);

    m_pSkin->registerTimerObject(this, 20 * 1000);

    updateAlbumArt();
}

void CMediaAlbumArtCtrl::onEvent(const IEvent *pEvent) {
    if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED || pEvent->eventType == ET_PLAYER_CUR_MEDIA_INFO_CHANGED) {
        m_pSkin->unregisterTimerObject(this);
        m_pSkin->registerTimerObject(this, 20 * 1000);
        updateAlbumArt();
        invalidate();
    }
}

bool CMediaAlbumArtCtrl::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (m_id != ID_INVALID) {
        m_pSkin->postCustomCommandMsg(m_id);

        return true;
    }
    return false;
}

bool CMediaAlbumArtCtrl::onLButtonDown(uint32_t nFlags, CPoint point) {
    return false;
}

void CMediaAlbumArtCtrl::onTimer(int nId) {
    RawImageDataPtr image = m_mediaAlbumArt.loadNext();
    if (image) {
        createAlbumArtImage(image);
        invalidate();
    }
    // 没有新图片时不回卷重解：避免暂停/静止时每 20s 无谓地重解同一张封面（JPEG 解码）。
    // 换歌已由 onEvent -> updateAlbumArt() 负责 reset() 并重新加载，无需在此重复回卷。
}

void CMediaAlbumArtCtrl::onSize() {
    CUIObject::onSize();

    if (!isMaskImageUsed()) {
        resizeAlbumArt();
    }
}

void CMediaAlbumArtCtrl::draw(CRawGraph *canvas) {
    CUIObject::draw(canvas);

    if (!m_img.isValid()) {
        return;
    }

    if (isMaskImageUsed()) {
        m_img.blt(canvas, m_rcObj.left, m_rcObj.top, m_rcObj.width(), m_rcObj.height(), 0, 0, BPM_BLEND);
        return;
    }

    //
    // No album art mask image is used.
    //
    CRect rc = m_rcObj;

    if (m_img.m_cx == 0 || m_img.m_cy == 0 || rc.empty()) {
        return;
    }

    int w, h;
    w = m_img.m_cx * rc.height() / m_img.m_cy;
    if (w < rc.width()) {
        // fill left and right
        h = rc.height();
    } else {
        // fill top and bottom
        h = m_img.m_cy * rc.width() / m_img.m_cx;
    }

    m_img.blt(canvas, rc.left + (rc.width() - w) / 2, rc.top + (rc.height() - h) / 2, BPM_BLEND);
}

bool CMediaAlbumArtCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (strcasecmp(szProperty, SZ_PN_IMAGE) == 0) {
        m_strImgFile = szValue;
        m_img.loadFromSRM(m_pSkin, szValue);
    } else if (strcasecmp(szProperty, "FrameMask") == 0) {
        m_strFrmMask = szValue;
        m_frmMask.loadFromSRM(m_pSkin, szValue);
    } else if (strcasecmp(szProperty, "BgColor") == 0) {
        getColorValue(m_clrBg, szValue);
    } else {
        return false;
    }

    return true;
}

#ifdef _SKIN_EDITOR_
void CMediaAlbumArtCtrl::enumProperties(CUIObjProperties &listProperties) {
    CUIObject::enumProperties(listProperties);

    listProperties.addPropImageFile(SZ_PN_IMAGE, m_strImgFile.c_str(), true);
}
#endif // _SKIN_EDITOR_

void CMediaAlbumArtCtrl::updateAlbumArt() {
    m_mediaAlbumArt.reset();
    m_img.detach();
    m_imgOrg.detach();

    auto image = m_mediaAlbumArt.loadNext();
    if (image) {
        createAlbumArtImage(image);
        return;
    }

    if (isMaskImageUsed()) {
        m_img.loadFromSRM(m_pSkin, m_strImgFile.c_str());
    } else {
        m_imgOrg.loadFromSRM(m_pSkin, m_strImgFile.c_str());
        resizeAlbumArt();
    }
}

void clearRawImage(RawImageData *pImg, const CColor &clr);

void CMediaAlbumArtCtrl::createAlbumArtImage(RawImageDataPtr srcImageData) {
    assert(srcImageData);

    if (!isMaskImageUsed()) {
        m_imgOrg.attach(srcImageData);

        resizeAlbumArt();
        return;
    }

    //
    // 最后显示的 album art 带的边框是叠加混合显示的
    // * m_img 是底图
    // * 使用 imageMask 去掉 srcImageData 的边框
    // * 再将去掉边框后的 srcImageData 复制到 m_img 就完成了
    //

    // 图片的大小
    // * imageMask 是从 m_img 中抠出来的部分，所以 imageMask 相当于是在 m_img 中剧中的.

    if (!m_img.loadFromSRM(m_pSkin, m_strImgFile.c_str())) {
        return;
    }

    srcImageData = convertTo32BppRawImage(srcImageData);
    assert(srcImageData);
    if (!srcImageData) {
        return;
    }

    auto scaleFactor = m_pSkin->getScaleFactor();

    // img, frameMask 是和 scaleFactor 无关的
    CRawImage img(m_img.getRawImageData(scaleFactor));
    CRawImage frameMask(m_frmMask.getRawImageData(scaleFactor));

    // 一般来说 mask 比 显示的要小一圈.
    int widthMask = frameMask.width(), heightMask = frameMask.height();
    int leftMask = (img.width() - widthMask) / 2, topMask = (img.height() - heightMask) / 2;

    // 缩放和截断 srcImageData 到 widthMask, heightMask 的大小.
    CRawImage imageAlbumArt(createScaleImage(srcImageData, widthMask, heightMask));

    frameMask.blt(&imageAlbumArt, 0, 0, widthMask, heightMask, 0, 0, BPM_MULTIPLY);

    imageAlbumArt.blt(&img, leftMask, topMask, widthMask, heightMask, 0, 0, BPM_BLEND);
}

void CMediaAlbumArtCtrl::resizeAlbumArt() {
    if (isMaskImageUsed()) {
        return;
    }
    if (!m_imgOrg.isValid()) {
        return;
    }

    auto scaleFactor = m_pSkin->getScaleFactor();
    auto imageAlbumArtData = createScaleImage(m_imgOrg.getHandle(), m_rcObj.width() * scaleFactor, m_rcObj.height() * scaleFactor);
    if (imageAlbumArtData) {
        m_img.attach(imageAlbumArtData);
    }
}
