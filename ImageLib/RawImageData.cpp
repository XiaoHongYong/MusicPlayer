// RawImageData.cpp: implementation of the RawImageData class.
//
//////////////////////////////////////////////////////////////////////

#include "ILIO.h"
#include "../GfxRaw/GfxRaw.h"

#include "../third-parties/Agg/include/agg_scanline_p.h"
#include "../third-parties/Agg/include/agg_renderer_scanline.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgba.h"
#include "../third-parties/Agg/include/agg_pixfmt_rgb.h"
#include "../third-parties/Agg/include/agg_rasterizer_scanline_aa.h"

#define BMPWIDTHBYTES(dwWidth, dwBitCount)    (( ( dwWidth * dwBitCount + 7 ) / 8 + 3 ) & ~3)

RawImageData *convert32BppTo24BppRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;
    uint8_t *rowSrc, *rowDst;

    dst->create(src->width, src->height, 24);

    rowSrc = src->rowPtr(0);
    rowDst = dst->rowPtr(0);
    for (int y = 0; y < src->height; y++)
    {
        uint8_t    *pixDst, *pixSrc;
        pixDst = rowDst;
        pixSrc = rowSrc;
        for (int x = 0; x < src->width; x++)
        {
            pixDst[0] = pixSrc[0];
            pixDst[1] = pixSrc[1];
            pixDst[2] = pixSrc[2];
            pixDst += 3;
            pixSrc += 4;
        }
        rowSrc += src->stride;
        rowDst += dst->stride;
    }

    return dst;
}

RawImageData *convert16BppTo24BppRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;
    uint8_t *rowSrc, *rowDst;

#define CLRMAX    256                //最大颜色数
#define    RMASK    0x1F            //0000 00  00 000   1 1111
#define RMOVE    0                //右移 0 位
#define RMAX    32                //最大颜色数
#define GMASK    0x3E0            //0000 00  11 111   0 0000
#define GMOVE    5                //
#define GMAX    32                //最大颜色数
#define BMASK    0x7C00            //1111 11  00 000   0 0000
#define BMOVE    10                //
#define BMAX    32                //最大颜色数

    uint8_t    clrTable[(int)RMAX];//颜色转换表
    for (int i = 0 ; i < (int)RMAX; i++)
        clrTable[i] = (uint8_t)((float)i * CLRMAX / RMAX);
    uint16_t    clrSrc;

    dst->create(src->width, src->height, 24);

    rowSrc = src->rowPtr(0);
    rowDst = dst->rowPtr(0);
    for (int y = 0; y < src->height; y++)
    {
        uint8_t    *pixDst;
        uint16_t    *pixSrc;
        pixDst = rowDst;
        pixSrc = (uint16_t*)rowSrc;
        for (int x = 0; x < src->width; x++)
        {
            clrSrc = *pixSrc;
            pixDst[PixPosition::PIX_R] = clrTable[clrSrc & RMASK];
            pixDst[PixPosition::PIX_G] = clrTable[(clrSrc & GMASK) >> GMOVE];
            pixDst[PixPosition::PIX_B] = clrTable[(clrSrc & BMASK) >> BMOVE];
            pixDst += 3;
            pixSrc++;
        }
        rowSrc += src->stride;
        rowDst += dst->stride;
    }

    return dst;
}

RawImageData *convert8BppTo24BppRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;
    uint8_t *rowSrc, *rowDst;

    dst->create(src->width, src->height, 24);

    rowSrc = src->rowPtr(0);
    rowDst = dst->rowPtr(0);
    for (int y = 0; y < src->height; y++)
    {
        uint8_t    *pixDst, *pixSrc;
        pixDst = rowDst;
        pixSrc = rowSrc;
        for (int x = 0; x < src->width; x++)
        {
            *pixDst = src->pallete[*pixSrc].rgbBlue;
            pixDst++;
            *pixDst = src->pallete[*pixSrc].rgbGreen;
            pixDst++;
            *pixDst = src->pallete[*pixSrc].rgbRed;
            pixDst++;
            pixSrc++;
        }
        rowSrc += src->stride;
        rowDst += dst->stride;
    }

    return dst;
}

RawImageData *convert4BppTo24BppRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;
    uint8_t *rowSrc, *rowDst;

    dst->create(src->width, src->height, 24);

    rowSrc = src->rowPtr(0);
    rowDst = dst->rowPtr(0);
    for (int y = 0; y < src->height; y++)
    {
        uint8_t    *pixDst, *pixSrc;
        pixDst = rowDst;
        pixSrc = rowSrc;
        for (int x = 0; x < src->width; x++)
        {
            uint8_t    px;
            if (x % 2 == 0)
                px = (*pixSrc >> 4 )& 0xF;
            else
            {
                px = *pixSrc & 0xF;
                pixSrc++;
            }
            *pixDst = src->pallete[px].rgbBlue;
            pixDst++;
            *pixDst = src->pallete[px].rgbGreen;
            pixDst++;
            *pixDst = src->pallete[px].rgbRed;
            pixDst++;
        }
        rowSrc += src->stride;
        rowDst += dst->stride;
    }

    return dst;
}

RawImageData *convertTo24BppRawImage(RawImageData *src)
{
    RawImageData *dst = nullptr;
    switch (src->bitCount)
    {
    case 24:
        return src;
    case 32:
        dst = convert32BppTo24BppRawImage(src);
        break;
    case 16:
        dst = convert16BppTo24BppRawImage(src);
        break;
    case 8:
        dst = convert8BppTo24BppRawImage(src);
        break;
    case 4:
        dst = convert4BppTo24BppRawImage(src);
        break;
    }

    freeRawImage(src);

    return dst;
}

RawImageData *convert24BppTo32BppRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;
    uint8_t *rowSrc, *rowDst;

    dst->create(src->width, src->height, 32);

    rowSrc = src->rowPtr(0);
    rowDst = dst->rowPtr(0);
    for (int y = 0; y < src->height; y++)
    {
        uint8_t    *pixDst, *pixSrc;
        pixDst = rowDst;
        pixSrc = rowSrc;
        for (int x = 0; x < src->width; x++)
        {
            pixDst[0] = pixSrc[0];
            pixDst[1] = pixSrc[1];
            pixDst[2] = pixSrc[2];
            pixDst[3] = 255;
            pixDst += 4;
            pixSrc += 3;
        }
        rowSrc += src->stride;
        rowDst += dst->stride;
    }

    return dst;
}

RawImageData *convertTo32BppRawImage(RawImageData *src)
{
    RawImageData *dst = nullptr;
    switch (src->bitCount)
    {
    case 32:
        return src;
    case 24:
        dst = convert24BppTo32BppRawImage(src);
        freeRawImage(src);
        break;
    default:
        src = convertTo24BppRawImage(src);
        dst = convert24BppTo32BppRawImage(src);
        freeRawImage(src);
        break;
    }

    return dst;
}

RawImageData *duplicateRawImage(RawImageData *src)
{
    RawImageData *dst = new RawImageData;

    if (!dst->create(src->width, src->height, src->bitCount))
        return nullptr;

    dst->stride = src->stride;

    memcpy(dst->buff, src->buff, src->getBuffSize());
    return dst;
}

RawImageData *loadRawImageDataFromBmpFile(IILIO *io);

RawImageData *loadRawImageDataFromPngFile(IILIO *io);

RawImageData *loadRawImageDataFromJpgFile(IILIO *io);

RawImageData *loadRawImageDataFromGifFile(IILIO *io);

RawImageData *loadRawImageDataFromFile(cstr_t szFile)
{
#ifdef _IPHONE
    return nullptr;
#else
    cstr_t szExt;
    size_t n;
    CFileILIO io;
    RawImageData *image = nullptr;

    n = strlen(szFile);
    if (n < 4)
        return nullptr;

    if (!io.open(szFile))
        return nullptr;

    szExt = szFile + n - 4;
    if (strcasecmp(szExt, ".bmp") == 0)
        image = loadRawImageDataFromBmpFile(&io);
    else if (strcasecmp(szExt, ".png") == 0)
        image = loadRawImageDataFromPngFile(&io);
    else if (strcasecmp(szExt, ".jpg") == 0
        || strcasecmp(szExt, "jpeg") == 0)
        image = loadRawImageDataFromJpgFile(&io);
    else if (strcasecmp(szExt, ".gif") == 0)
        image = loadRawImageDataFromGifFile(&io);
    else
        image = nullptr;

    if (image)
    {
        if (image->bitCount != 24 && image->bitCount != 32)
            return convertTo24BppRawImage(image);
    }

    return image;
#endif // _IPHONE
}


RawImageData *loadRawImageDataFromMem(const void *buf, int nSize)
{
#ifdef _IPHONE
    return nullptr;
#else
    CBuffILIO io;
    uint8_t *tag = (uint8_t *)buf;
    uint8_t png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    RawImageData *image = nullptr;

    if (!io.open(buf, nSize))
        return nullptr;

    if (memcmp(buf, "BM", 2) == 0)
        image = loadRawImageDataFromBmpFile(&io);
    else if (memcmp(buf, png_signature, 8) == 0)
        image = loadRawImageDataFromPngFile(&io);
    else if (tag[0] == 0xFF && tag[1] == 0xD8)
        return loadRawImageDataFromJpgFile(&io);
    else if (memcmp(buf, "GIF8", 4) == 0)
        image = loadRawImageDataFromGifFile(&io);
    else
        image = nullptr;

    if (image)
    {
        if (image->bitCount != 24 && image->bitCount != 32)
            return convertTo24BppRawImage(image);
    }

    return image;
#endif // _IPHONE
}

void freeRawImage(RawImageData * image)
{
    image->free();
    delete image;
}


void rawImageBGR24Set(RawImageData *image, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t        *pRow, *p;

    pRow = image->rowPtr(0);

    for (int y = 0; y < image->height; y++)
    {
        p = pRow;
        for (int x = 0; x < image->width; x++)
        {
            p[PixPosition::PIX_B] = b;
            p[PixPosition::PIX_G] = g;
            p[PixPosition::PIX_R] = r;
            p += 3;
        }
        pRow += image->stride;
    }
}

void rawImageBGRA32Set(RawImageData *image, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    assert(image->pixFormat == PF_RGBA32);

    uint8_t        *pRow, *p;

    pRow = image->rowPtr(0);

    for (int y = 0; y < image->height; y++)
    {
        p = pRow;
        for (int x = 0; x < image->width; x++)
        {
            p[PixPosition::PIX_B] = b;
            p[PixPosition::PIX_G] = g;
            p[PixPosition::PIX_R] = r;
            p[PixPosition::PIX_A] = a;
            p += 4;
        }
        pRow += image->stride;
    }
}

void rawImageSet(RawImageData *image, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (image->pixFormat == PF_RGB24)
        rawImageBGR24Set(image, r, g, b);
    else
        rawImageBGRA32Set(image, r, g, b, a);
}

//////////////////////////////////////////////////////////////////////////

inline unsigned char ToRGBx(float rm1, float rm2, float rh);

inline unsigned char ToRGBx(float rm1, float rm2, float rh)
{
    if        (rh > 360.0f)
    {
        rh -= 360.0f;
        if (rh > 360.0f)
            rh -= 360.0f;
    }
    else if (rh <   0.0f) rh += 360.0f;
    
    if      (rh <  60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
    else if (rh < 180.0f) rm1 = rm2;
    else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;

    return static_cast<unsigned char>(rm1 * 255);
}

inline void adjustPixHue(uint8_t *pix, float hueOffset);

inline void adjustPixHue(uint8_t *pix, float hueOffset)
{
    float hue, saturation, luminance;

    unsigned char minval = min(pix[0], min(pix[1], pix[2]));
    unsigned char maxval = max(pix[0], max(pix[1], pix[2]));
    float mdiff  = float(maxval) - float(minval);
    float msum   = float(maxval) + float(minval);

    luminance = msum / 510.0f;

    if (maxval == minval)
    {
        saturation = 0.0f;
        hue = 0.0f;
    }
    else
    {
        float rnorm = (maxval - pix[PixPosition::PIX_R]  ) / mdiff;
        float gnorm = (maxval - pix[PixPosition::PIX_G]  ) / mdiff;
        float bnorm = (maxval - pix[PixPosition::PIX_B]  ) / mdiff;

        saturation = (luminance <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

        if (pix[2]   == maxval)         hue = 60.0f * (6.0f + bnorm - gnorm);
        else if (pix[1]   == maxval) hue = 60.0f * (2.0f + rnorm - bnorm);
        else                         hue = 60.0f * (4.0f + gnorm - rnorm);
        if (hue > 360.0f) hue = hue - 360.0f;
    }

    hue += hueOffset;

    if (saturation == 0.0)
    {
        pix[0] = pix[1] = pix[2] = (uint8_t)(luminance * 255.0);
    }
    else
    {
        float rm1, rm2;

        if (luminance <= 0.5f)
            rm2 = luminance + luminance * saturation;  
        else
            rm2 = luminance + saturation - luminance * saturation;

        rm1 = 2.0f * luminance - rm2;
        pix[2] = ToRGBx(rm1, rm2, hue + 120.0f);
        pix[1] = ToRGBx(rm1, rm2, hue);
        pix[0] = ToRGBx(rm1, rm2, hue - 120.0f);
    }
}

inline void adjustPixHue(uint8_t *pix, float hueOffset, float saturationRatio, float luminanceRatio);

inline void adjustPixHue(uint8_t *pix, float hueOffset, float saturationRatio, float luminanceRatio)
{
    float hue, saturation, luminance;

    unsigned char minval = min(pix[0], min(pix[1], pix[2]));
    unsigned char maxval = max(pix[0], max(pix[1], pix[2]));
    float mdiff  = float(maxval) - float(minval);
    float msum   = float(maxval) + float(minval);

    luminance = msum / 510.0f;

    if (maxval == minval)
    {
        saturation = 0.0f;
        hue = 0.0f;
    }
    else
    {
        float rnorm = (maxval - pix[PixPosition::PIX_R]  ) / mdiff;
        float gnorm = (maxval - pix[PixPosition::PIX_G]  ) / mdiff;
        float bnorm = (maxval - pix[PixPosition::PIX_B]  ) / mdiff;

        saturation = (luminance <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

        if (pix[2]   == maxval)         hue = 60.0f * (6.0f + bnorm - gnorm);
        else if (pix[1]   == maxval) hue = 60.0f * (2.0f + rnorm - bnorm);
        else                         hue = 60.0f * (4.0f + gnorm - rnorm);
        if (hue > 360.0f) hue = hue - 360.0f;
    }

    hue += hueOffset;
    if (saturationRatio > 0)
        saturation += (float)((1.0 - saturation) * saturationRatio);
    else
        saturation *= -saturationRatio;
    assert(saturation >= 0 && saturation <= 1.0);

    if (luminanceRatio > 0)
        luminance += (float)((1.0 - luminance) * luminanceRatio);
    else
        luminance *= -luminanceRatio;
    assert(luminance >= 0 && luminance <= 1.0);

    if (saturation == 0.0)
    {
        pix[0] = pix[1] = pix[2] = (uint8_t)(luminance * 255.0);
    }
    else
    {
        float rm1, rm2;

        if (luminance <= 0.5f)
            rm2 = luminance + luminance * saturation;  
        else
            rm2 = luminance + saturation - luminance * saturation;

        rm1 = 2.0f * luminance - rm2;
        pix[2] = ToRGBx(rm1, rm2, hue + 120.0f);
        pix[1] = ToRGBx(rm1, rm2, hue);
        pix[0] = ToRGBx(rm1, rm2, hue - 120.0f);
    }
}

void adjustColorHue(COLORREF &clr, float hueOffset)
{
    uint8_t    *p = (uint8_t *)&clr;
    uint8_t    pix[3];
    pix[0] = p[2];
    pix[1] = p[1];
    pix[2] = p[0];
    adjustPixHue(pix, hueOffset);
    p[0] = pix[2];
    p[1] = pix[1];
    p[2] = pix[0];
}

void adjustColorHue(COLORREF &clr, float hueOffset, float saturationRatio, float luminanceRatio)
{
    uint8_t    *p = (uint8_t *)&clr;
    uint8_t    pix[3];
    pix[0] = p[2];
    pix[1] = p[1];
    pix[2] = p[0];
    adjustPixHue(pix, hueOffset, saturationRatio, luminanceRatio);
    p[0] = pix[2];
    p[1] = pix[1];
    p[2] = pix[0];
}

// hueOffset: 0-360
void adjustImageHue32(RawImageData *pImage, float hueOffset)
{
    uint8_t        *p, *pRow;

    pRow = pImage->rowPtr(0);
    for (int y = 0; y < pImage->height; y++)
    {
        p = pRow;
        for (int x = 0; x < pImage->width; x++)
        {
            adjustPixHue(p, hueOffset);
            p += 4;
        }
        pRow += pImage->stride;
    }
}

// hueOffset: 0-360
void adjustImageHue24(RawImageData *pImage, float hueOffset)
{
    uint8_t        *p, *pRow;

    pRow = pImage->rowPtr(0);
    for (int y = 0; y < pImage->height; y++)
    {
        p = pRow;
        for (int x = 0; x < pImage->width; x++)
        {
            adjustPixHue(p, hueOffset);
            p += 3;
        }
        pRow += pImage->stride;
    }
}

void adjustImageHue(RawImageData *pImage, float hueOffset)
{
    if (pImage->bitCount == 32)
        adjustImageHue32(pImage, hueOffset);
    else if (pImage->bitCount == 24)
        adjustImageHue24(pImage, hueOffset);
}

// hueOffset: 0-360
void adjustImageHue32(RawImageData *pImage, float hueOffset, float saturationRatio, float luminanceRatio)
{
    uint8_t        *p, *pRow;

    pRow = pImage->rowPtr(0);
    for (int y = 0; y < pImage->height; y++)
    {
        p = pRow;
        for (int x = 0; x < pImage->width; x++)
        {
            adjustPixHue(p, hueOffset, saturationRatio, luminanceRatio);
            p += 4;
        }
        pRow += pImage->stride;
    }
}

// hueOffset: 0-360
void adjustImageHue24(RawImageData *pImage, float hueOffset, float saturationRatio, float luminanceRatio)
{
    uint8_t        *p, *pRow;

    pRow = pImage->rowPtr(0);
    for (int y = 0; y < pImage->height; y++)
    {
        p = pRow;
        for (int x = 0; x < pImage->width; x++)
        {
            adjustPixHue(p, hueOffset, saturationRatio, luminanceRatio);
            p += 3;
        }
        pRow += pImage->stride;
    }
}

// hueOffset: 0 ~ 360
// luminanceRatio, saturationRatio: 0 ~ 1.0, increase ratio
//                                    -1.0 ~ 0, ratio of scale
void adjustImageHue(RawImageData *pImage, float hueOffset, float saturationRatio, float luminanceRatio)
{
    if (pImage->bitCount == 32)
        adjustImageHue32(pImage, hueOffset, saturationRatio, luminanceRatio);
    else if (pImage->bitCount == 24)
        adjustImageHue24(pImage, hueOffset, saturationRatio, luminanceRatio);
}

COLORREF HLSToRGB(float hue, float saturation, float luminance)
{
    COLORREF        clr;

    if (saturation == 0.0)
    {
        unsigned char    c = (uint8_t)(luminance * 255.0);
        clr = RGB(c, c, c);
    }
    else
    {
        float rm1, rm2;

        if (luminance <= 0.5f)
            rm2 = luminance + luminance * saturation;  
        else
            rm2 = luminance + saturation - luminance * saturation;

        rm1 = 2.0f * luminance - rm2;
        clr = RGB(ToRGBx(rm1, rm2, hue + 120.0f),
                ToRGBx(rm1, rm2, hue),
                ToRGBx(rm1, rm2, hue - 120.0f));
    }

    return clr;
}

void RGBToHLS(COLORREF clr, float &hue, float &saturation, float &luminance)
{
    // Konvertierung
    unsigned char minval = min(GetRValue(clr), min(GetGValue(clr), GetBValue(clr)));
    unsigned char maxval = max(GetRValue(clr), max(GetGValue(clr), GetBValue(clr)));
    float mdiff  = float(maxval) - float(minval);
    float msum   = float(maxval) + float(minval);

    luminance = msum / 510.0f;

    if (maxval == minval) 
    {
        saturation = 0.0f;
        hue = 0.0f; 
    }
    else 
    { 
        float rnorm = (maxval - GetRValue(clr)  ) / mdiff;      
        float gnorm = (maxval - GetGValue(clr)  ) / mdiff;
        float bnorm = (maxval - GetBValue(clr)  ) / mdiff;   

        if ((luminance <= 0.5f))
            saturation = mdiff / msum;
        else
        {
            saturation = (float)(mdiff / (2.0 - msum));
            if (saturation < 0)
                saturation = -saturation;
        }

        if (GetRValue(clr)   == maxval) hue = 60.0f * (6.0f + bnorm - gnorm);
        else if (GetGValue(clr) == maxval) hue = 60.0f * (2.0f + rnorm - bnorm);
        else if (GetBValue(clr)  == maxval) hue = 60.0f * (4.0f + gnorm - rnorm);
        if (hue > 360.0f) hue = hue - 360.0f;
    }
}
/*
void  rgb_to_hsl(
    float    r,
    float    g,
    float    b,
    float    *h,
    float    *s,
    float    *l )
{
    float  v, m, vm, r2, g2, b2;

    v = max( r, max(g, b) );

    m = min( r, min(g, b ));

    *l = (m + v) / 2.0;

    if( *l > 0.0 )
    {
        vm = v - m;
        *s = vm;

        if( *s > 0.0 )
        {
            if( *l <= 0.5 )
                *s /= v + m;
            else
                *s /= 2.0 - v - m;

            r2 = (v - r) / vm;
            g2 = (v - g) / vm;
            b2 = (v - b) / vm;

            if( r == v )
            {
                if( g == m )
                    *h = 5.0 + b2;
                else
                    *h = 1.0 - g2;
            }
            else if( g == v )
            {
                if( b == m )
                    *h = 1.0 + r2;
                else
                    *h = 3.0 - b2;
            }
            else
            {
                if( r == m )
                    *h = 3.0 + g2;
                else
                    *h = 5.0 - r2;
            }

            *h /= 6.0;
        }
    }
}
void  hsl_to_rgb(
    float   h,
    float   sl,
    float   l,
    float   *r,
    float   *g,
    float   *b )
{
    float  v;
    float  m, sv;
    int   sextant;
    float  fract, vsf, mid1, mid2;

    if( l <= 0.5 )
    {
        v = l * (1.0 + sl);
    }
    else
    {
        v = l + sl - l * sl;
    }

    if( v <= 0.0 )
    {
        *r = 0.0;
        *g = 0.0;
        *b = 0.0;
    }
    else
    {
        m = l + l - v;
        sv = (v - m) / v;
        h *= 6.0;
        sextant = (int) h;
        fract = h - (float) sextant;
        vsf = v * sv * fract;
        mid1 = m + vsf;
        mid2 = v - vsf;

        switch ( sextant )
        {
        case 0:
        case 6:  *r = v;     *g = mid1;  *b = m;     break;
        case 1:  *r = mid2;  *g = v;     *b = m;     break;
        case 2:  *r = m;     *g = v;     *b = mid1;  break;
        case 3:  *r = m;     *g = mid2;  *b = v;     break;
        case 4:  *r = mid1;  *g = m;     *b = v;     break;
        case 5:  *r = v;     *g = m;     *b = mid2;  break;
        }
    }
}


void HLSTest()
{
    COLORREF        clr, clrNew;
    int        r, g, b;
    float    H, S, L;
    float    H2, S2, L2;
    uint16_t    hue, saturation, luminance;
    float    R, G, B;
// 
//     clrNew = RGB(255, 254, 253);
//     rgb_to_hsl(255, 254, 253, &H, &S, &L);
//     hsl_to_rgb(H, S, L, &R, &G, &B);
//     RGBToHLS(clrNew, H, S, L);
//     adjustColorHue(clrNew, 0, -1.0, -0.8);
//     RGBToHLS(clrNew, H2, S2, L2);
//     clr = HLSToRGB(H2, S2, L2);
// 
//     clr = RGB(255, 254, 253);
//     clrNew = clr;
// 
//     for (float l = -1.0; l < 0; l += 0.01)
//     {
//         clrNew = clr;
//         adjustColorHue(clrNew, 0, -1.0, l);
//     }


//    return;

    {
        for (r = 0; r <= 255; r++)
        {
            for (g = 0; g <= 255; g++)
            {
                for (b = 0; b <= 255; b++)
                {
//                     rgb_to_hsl(r, g, b, &H, &S, &L);
//                     hsl_to_rgb(H, S, L, &R, &G, &B);
//                     if (abs(r - R) > 2
//                         || abs(g - G) > 2
//                         || abs(b - B) > 2)
//                     {
//                         DBG_LOG0("Not equal");
//                     }


                    clr = RGB(r, g, b);
                    clrNew = clr;
                    adjustColorHue(clrNew, 0);//, -1.0, -1.0);

                    //RGBToHLS(clr, H, S, L);
                    //clrNew = HLSToRGB(H, S, L);
                    if (abs(GetRValue(clr) - GetRValue(clrNew)) > 1
                        || abs(GetGValue(clr) - GetGValue(clrNew)) > 1
                        || abs(GetBValue(clr) - GetBValue(clrNew)) > 1)
                    {
                        DBG_LOG0("Not equal");
                    }
                }
            }
        }
    }
}
*/

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RawImageData::RawImageData()
{
    buff = nullptr;
    width = height = 0;
    stride = 0;
    bitCount = 0;
    pixFormat = PF_UNKNOWN;

    nClrUsed = 0;
    pallete = nullptr;
}

RawImageData::~RawImageData() {
    free();
}

void RawImageData::attach(uint8_t *buf, int nWidth, int nHeight, int nBitCount)
{
    buff = buf;
    width = nWidth;
    height = nHeight;
    bitCount = nBitCount;
    if (nBitCount == 32)
        pixFormat = PF_RGBA32;
    else if (nBitCount == 24)
        pixFormat = PF_RGB24;
    else
        pixFormat = PF_UNKNOWN;

    stride = -BMPWIDTHBYTES(width, nBitCount);
}

void RawImageData::attach(RawImageData *pSrc)
{
    buff = pSrc->buff;
    width = pSrc->width;
    height = pSrc->height;
    stride = pSrc->stride;
    bitCount = pSrc->bitCount;
    pixFormat = pSrc->pixFormat;
    nClrUsed = pSrc->nClrUsed;
    pallete = pSrc->pallete;
}

void RawImageData::exchange(RawImageData *pSrc)
{
    RawImageData        temp;
    temp.attach(this);
    attach(pSrc);
    pSrc->attach(&temp);
    temp.detach();
}

void RawImageData::detach()
{
    buff = nullptr;
    pallete = nullptr;
    nClrUsed = 0;
}

bool RawImageData::create(int nWidth, int nHeight, int nBitCount)
{
    width = nWidth;
    height = nHeight;
    bitCount = nBitCount;
    if (nBitCount == 32)
        pixFormat = PF_RGBA32;
    else if (nBitCount == 24)
        pixFormat = PF_RGB24;
    else
        pixFormat = PF_UNKNOWN;

    stride = -BMPWIDTHBYTES(width, nBitCount);

    buff = new uint8_t[abs(stride) * height];
    assert(buff);

    if (nBitCount <= 8)
        pallete = new RGBQUAD[256];

    return true;
}

bool RawImageData::createReverse(int nWidth, int nHeight, int nBitCount)
{
    if (!create(nWidth, nHeight, nBitCount))
        return false;

    stride = -stride;

    return true;
}

void RawImageData::setClrUsed(int _nClrUsed)
{
    nClrUsed = (uint8_t)_nClrUsed;

    if (_nClrUsed == 0)
    {
        if (pallete)
        {
            delete[] pallete;
            pallete = nullptr;
        }
        return;
    }
    if (!pallete)
        pallete = new RGBQUAD[256];
}

RGBQUAD RawImageData::getPixel(int x, int y) const
{
    uint8_t        *p;
    RGBQUAD        quad;

    p = rowPtr(y);
    switch (pixFormat)
    {
    case PF_RGBA32:
        {
            p += x * 4;
            quad.rgbBlue = p[PixPosition::PIX_B];
            quad.rgbGreen = p[PixPosition::PIX_G];
            quad.rgbRed = p[PixPosition::PIX_R];
            quad.rgbReserved = p[3];
        }
        break;
    case PF_RGB24:
        {
            p += x * 3;
            quad.rgbBlue = p[PixPosition::PIX_B];
            quad.rgbGreen = p[PixPosition::PIX_G];
            quad.rgbRed = p[PixPosition::PIX_R];
            quad.rgbReserved = 255;
        }
        break;
    case PF_PALLETE256:
        {
            p += x;
            if (pallete)
                quad = pallete[*p];
            else
                memset(&quad, 0, sizeof(quad));
        }
        break;
    case PF_PALLETE16:
    case PF_PALLETE2:
        {
            memset(&quad, 0, sizeof(quad));
            assert(0 && "unsupported format.");
        }
        break;
    default:
        {
            memset(&quad, 0, sizeof(quad));
            assert(0 && "unsupported format.");
        }
        break;
    }
    return quad;
}

void RawImageData::free()
{
    if (buff)
    {
        delete[] buff;
        buff = nullptr;
    }
    if (pallete)
    {
        delete[] pallete;
        pallete = nullptr;
    }
}
