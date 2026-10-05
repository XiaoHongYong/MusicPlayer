#pragma once

#ifndef _SKIN_IMAGE_INC_
#define _SKIN_IMAGE_INC_

#include "../GfxRaw/RawImage.h"


class CSkinWnd;
class CSkinResMgr;

// CSFImage = CSkinFactoryImage
class CSFImage : public CRawImage {
public:
    CSFImage(void);
    CSFImage(const CSFImage &src);
    virtual ~CSFImage(void);

    CSFImage &operator=(const CSFImage &src);

    bool loadFromSRM(CSkinWnd *skinWnd, cstr_t resName);
    bool loadFromSRM(CSkinResMgr *resMgr, cstr_t resName, int scaleFactor);

    virtual void attach(const RawImageDataPtr &image) override;

    // 附加一张按 @scaleFactor 生成的图片（物理像素 = 逻辑尺寸 * scaleFactor），
    // 逻辑尺寸 m_cx/m_cy 会相应除以 scaleFactor。
    void attach(const RawImageDataPtr &image, float scaleFactor);

    virtual void detach() override;

    virtual const RawImageDataPtr &getRawImageData(float scaleFactor) override;

    virtual bool isPixelTransparent(CPoint pt) const override;

protected:
    void copyFrom(const CSFImage &src);

protected:
    CSkinResMgr                 *m_skinResMgr;
    string                      m_resName;
    float                       m_scaleFactor;

};

#endif // _SKIN_IMAGE_INC_
