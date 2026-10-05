#include "SkinTypes.h"
#include "Skin.h"
#include "SkinImage.h"


CSFImage::CSFImage(void) {
    m_skinResMgr = nullptr;
    m_scaleFactor = 1;
}

CSFImage::CSFImage(const CSFImage &src) {
    m_skinResMgr = nullptr;
    copyFrom(src);
}

CSFImage::~CSFImage(void) {
    detach();
}

void CSFImage::attach(const RawImageDataPtr &image) {
    CRawImage::attach(image);
    m_scaleFactor = 1.0;
}

void CSFImage::attach(const RawImageDataPtr &image, float scaleFactor) {
    assert(scaleFactor > 0);

    CRawImage::attach(image);
    m_scaleFactor = scaleFactor;
    m_cx = (int)(m_cx / scaleFactor);
    m_cy = (int)(m_cy / scaleFactor);
}

void CSFImage::detach() {
    if (m_skinResMgr) {
        m_skinResMgr = nullptr;
        m_resName.clear();
    }

    CRawImage::detach();
}

CSFImage &CSFImage::operator=(const CSFImage &src) {
    copyFrom(src);

    return *this;
}

void CSFImage::copyFrom(const CSFImage &src) {
    if (this == &src) {
        return;
    }

    detach();

    m_image = src.m_image;
    m_x = src.m_x;
    m_y = src.m_y;
    m_cx = src.m_cx;
    m_cy = src.m_cy;
    m_skinResMgr = src.m_skinResMgr;
    m_resName = src.m_resName;
    m_scaleFactor = src.m_scaleFactor;
}

bool CSFImage::loadFromSRM(CSkinWnd *skinWnd, cstr_t resName) {
    assert(skinWnd);

    if (skinWnd->getSkinFactory()->getIcons().getImage(resName, *this)) {
        return true;
    }

    return loadFromSRM(skinWnd->getSkinFactory()->getResourceMgr(), resName, skinWnd->getScaleFactor());
}

bool CSFImage::loadFromSRM(CSkinResMgr *resMgr, cstr_t resName, int scaleFactor) {
    detach();

    m_skinResMgr = resMgr;
    m_resName = resName;
    m_scaleFactor = scaleFactor;

    auto image = m_skinResMgr->loadBitmap(resName, m_scaleFactor);
    if (image) {
        CRawImage::attach(image);
        m_cx /= m_scaleFactor;
        m_cy /= m_scaleFactor;
    } else {
        ERR_LOG1("load Bitmap: %s, FAILED.", resName);
    }

    return image != nullptr;
}

const RawImageDataPtr &CSFImage::getRawImageData(float scaleFactor) {
    if (m_scaleFactor != scaleFactor) {
        if (m_skinResMgr && !m_resName.empty()) {
            m_scaleFactor = scaleFactor;
            auto image = m_skinResMgr->loadBitmap(m_resName.c_str(), scaleFactor);
            assert(image);
            if (image) {
                m_image = image;
            }
        } else {
            // 动态创建的图片
            // 需要按照 scaleFactor 来缩放 image;
            m_image = createScaledRawImageData(m_image.get(), scaleFactor / m_scaleFactor);
            m_scaleFactor = scaleFactor;
        }
    }

    return m_image;
}

bool CSFImage::isPixelTransparent(CPoint pt) const {
    RGBQUAD pixel = m_image->getPixel((pt.x + m_x) * m_scaleFactor, (pt.y + m_y) * m_scaleFactor);
    return pixel.rgbReserved == 0;
}

/*
void CSFImage::tileBlt(CRawGraph *canvas, int xDest, int yDest, int nWidthDest, int nHeightDest, BlendPixMode bpm)
{
    assert(m_cy);
    assert(m_cx);

    if (m_cy == 0 || m_cx == 0)
        return;

    int            x, y;

    //
    // 先将Y方向的绘画了
    // 1111 1111 1111 222
    // 1111 1111 1111 222
    // 3333 3333 3333 444
    for (y = yDest; y + m_cy < yDest + nHeightDest; y += m_cy)
    {
        // 1111
        for (x = xDest; x + m_cx <= xDest + nWidthDest; x += m_cx)
        {
            blt(canvas,
                x, y,
                m_cx, m_cy,
                m_x, m_y, bpm);
        }

        // 2222
        if (x < xDest + nWidthDest)
        {
            blt(canvas,
                x, y,
                xDest + nWidthDest - x, m_cy,
                m_x, m_y, bpm);
        }
    }

    if (y < yDest + nHeightDest)
    {
        int        nLeftCy;

        nLeftCy = yDest + nHeightDest - y;

        // 3333
        for (x = xDest; x + m_cx <= xDest + nWidthDest; x += m_cx)
        {
            blt(canvas,
                x, y,
                m_cx, nLeftCy,
                m_x, m_y, bpm);
        }

        // 444
        if (x < xDest + nWidthDest)
        {
            blt(canvas,
                x, y,
                xDest + nWidthDest - x, nLeftCy,
                m_x, m_y, bpm);
        }
    }
}
*/

/*
template<class _DrawImageFun>
void CSkinImage::tileBltExT(CRawGraph *canvas, int xOrg, int yOrg, int xDest, int yDest, int nWidthDest, int nHeightDest, _DrawImageFun &drawFunc)
{
    assert(m_cy);
    assert(m_cx);

    if (m_cy == 0 || m_cx == 0)
        return;

    int            xSrc, ySrc, x, y;
    int            nTileX, nTileY;
    int            nW;

    //
    // 先将Y方向的绘画了
    // BBB 0000 0000 0000 AAA
    // CCC 1111 1111 1111 222
    // CCC 1111 1111 1111 222
    // DDD 3333 3333 3333 444

    // BBB
    {
        int        nLeftCy;
        xSrc = (xDest - xOrg) % m_cx;
        ySrc = (yDest - yOrg) % m_cy;
        nLeftCy = min(m_cy - ySrc, nHeightDest);
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            drawFunc(this,
                xDest, yDest,
                nW, nLeftCy,
                m_x + xSrc, m_y + ySrc);
            nTileX = xDest + (m_cx - xSrc);
        }
        else
            nTileX = xDest;

        // 0000
        if (ySrc != 0)
        {
            for (x = nTileX; x + m_cx <= xDest + nWidthDest; x += m_cx)
            {
                drawFunc(this,
                    x, yDest,
                    m_cx, nLeftCy,
                    m_x, m_y + ySrc);
            }

            // AAA
            if (x < xDest + nWidthDest)
            {
                drawFunc(this,
                    x, yDest,
                    xDest + nWidthDest - x, nLeftCy,
                    m_x, m_y + ySrc);
            }
            nTileY = yDest + (m_cy - ySrc);
        }
        else
            nTileY = yDest;
    }

    for (y = nTileY; y + m_cy < yDest + nHeightDest; y += m_cy)
    {
        // CCCC
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            drawFunc(this,
                xDest, y,
                nW, m_cy,
                m_x + xSrc, m_y);
        }

        // 1111
        for (x = nTileX; x + m_cx < xDest + nWidthDest; x += m_cx)
        {
            drawFunc(this,
                x, y,
                m_cx, m_cy,
                m_x, m_y);
        }

        // 222
        if (x < xDest + nWidthDest)
        {
            drawFunc(this,
                x, y,
                xDest + nWidthDest - x, m_cy,
                m_x, m_y);
        }
    }

    if (y < yDest + nHeightDest)
    {
        int        nLeftCy;

        nLeftCy = yDest + nHeightDest - y;

        // DDD
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            drawFunc(this,
                xDest, y,
                nW, nLeftCy,
                m_x + xSrc, m_y);
        }

        // 3333
        for (x = nTileX; x + m_cx <= xDest + nWidthDest; x += m_cx)
        {
            drawFunc(this,
                x, y,
                m_cx, nLeftCy,
                m_x, m_y);
        }

        // 444
        if (x < xDest + nWidthDest)
        {
            drawFunc(this,
                x, y,
                xDest + nWidthDest - x, nLeftCy,
                m_x, m_y);
        }
    }
}*/

/*
void CSFImage::tileBltEx(CRawGraph *canvas, int xOrg, int yOrg, int xDest, int yDest, int nWidthDest, int nHeightDest)
{
    assert(m_cy);
    assert(m_cx);

    if (m_cy == 0 || m_cx == 0)
        return;

    int            xSrc, ySrc, x, y;
    int            nTileX, nTileY;
    int            nW;

    //
    // 先将Y方向的绘画了
    // BBB 0000 0000 0000 AAA
    // CCC 1111 1111 1111 222
    // CCC 1111 1111 1111 222
    // DDD 3333 3333 3333 444

    // BBB
    {
        int        nLeftCy;
        xSrc = (xDest - xOrg) % m_cx;
        ySrc = (yDest - yOrg) % m_cy;
        nLeftCy = min(m_cy - ySrc, nHeightDest);
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            canvas->DrawImage(this,
                xDest, yDest,
                nW, nLeftCy,
                m_x + xSrc, m_y + ySrc);
            nTileX = xDest + (m_cx - xSrc);
        }
        else
            nTileX = xDest;

        // 0000
        if (ySrc != 0)
        {
            for (x = nTileX; x + m_cx <= xDest + nWidthDest; x += m_cx)
            {
                canvas->DrawImage(this,
                    x, yDest,
                    m_cx, nLeftCy,
                    m_x, m_y + ySrc);
            }

            // AAA
            if (x < xDest + nWidthDest)
            {
                canvas->DrawImage(this,
                    x, yDest,
                    xDest + nWidthDest - x, nLeftCy,
                    m_x, m_y + ySrc);
            }
            nTileY = yDest + (m_cy - ySrc);
        }
        else
            nTileY = yDest;
    }

    for (y = nTileY; y + m_cy < yDest + nHeightDest; y += m_cy)
    {
        // CCCC
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            canvas->DrawImage(this,
                xDest, y,
                nW, m_cy,
                m_x + xSrc, m_y);
        }

        // 1111
        for (x = nTileX; x + m_cx < xDest + nWidthDest; x += m_cx)
        {
            canvas->DrawImage(this,
                x, y,
                m_cx, m_cy,
                m_x, m_y);
        }

        // 222
        if (x < xDest + nWidthDest)
        {
            canvas->DrawImage(this,
                x, y,
                xDest + nWidthDest - x, m_cy,
                m_x, m_y);
        }
    }

    if (y < yDest + nHeightDest)
    {
        int        nLeftCy;

        nLeftCy = yDest + nHeightDest - y;

        // DDD
        if (xSrc != 0)
        {
            nW = min(m_cx - xSrc, nWidthDest);
            canvas->DrawImage(this,
                xDest, y,
                nW, nLeftCy,
                m_x + xSrc, m_y);
        }

        // 3333
        for (x = nTileX; x + m_cx <= xDest + nWidthDest; x += m_cx)
        {
            canvas->DrawImage(this,
                x, y,
                m_cx, nLeftCy,
                m_x, m_y);
        }

        // 444
        if (x < xDest + nWidthDest)
        {
            canvas->DrawImage(this,
                x, y,
                xDest + nWidthDest - x, nLeftCy,
                m_x, m_y);
        }
    }
}*/
