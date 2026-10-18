#ifndef GfxRaw_RawGraph_h
#define GfxRaw_RawGraph_h

#pragma once

#include "../ImageLib/RawImageData.h"
#include "../Window/WindowTypes.h"
#include "RawPen.h"


#ifdef _WIN32
enum PixPosition {
    PIX_B                       = 0,
    PIX_G                       = 1,
    PIX_R                       = 2,
    PIX_A                       = 3, // Alpha channel
    PIX_SIZE                    = 4
};
#else // #ifdef _WIN32
enum PixPosition {
    PIX_R                       = 0,
    PIX_G                       = 1,
    PIX_B                       = 2,
    PIX_A                       = 3, // Alpha channel
    PIX_SIZE                    = 4
};
#endif // #ifdef _WIN32


#ifdef _WIN32
#include "win32/RawGraphData.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/RawGraphData.h"
#endif

#ifdef _MAC_OS
#include "mac/RawGraphData.h"
#endif


class CRawBmpFont;

//
// CRawGraph 是直接使用 CPU 操作内存修改图像数据的画布
//
// * 支持 ScaleFactor 和 ClipBox，Origin（设置画布的原点）
//    * 只有当涉及到修改内存时，才会将操作的坐标使用 scaleFactor, origin 转换到真实坐标
//
class CRawGraph : public CRawGraphData {
public:
    CRawGraph(float scaleFactor);
    virtual ~CRawGraph();

    class CClipBoxAutoRecovery {
    public:
        CClipBoxAutoRecovery(CRawGraph *canvas) {
            m_canvas = canvas;
            canvas->getClipBoundBox(m_rcClipOrg);
        }
        ~CClipBoxAutoRecovery() {
            if (m_canvas) {
                m_canvas->resetClipBoundBox(m_rcClipOrg);
            }
        }

        void recover() {
            if (m_canvas) {
                m_canvas->resetClipBoundBox(m_rcClipOrg);
                m_canvas = nullptr;
            }
        }

    protected:
        CRawGraph                   *m_canvas;
        CRect                       m_rcClipOrg;

    };

    class COpacityBlendAutoRecovery {
    public:
        COpacityBlendAutoRecovery(CRawGraph *canvas, int nOpacityPainting) {
            if (nOpacityPainting != 255) {
                m_canvas = canvas;
                m_nOpacityPainting = canvas->getOpacityPainting();

                nOpacityPainting = m_nOpacityPainting * nOpacityPainting / 255;
                m_canvas->setOpacityPainting(nOpacityPainting);
            } else {
                m_canvas = nullptr;
                m_nOpacityPainting = 255;
            }
        }
        ~COpacityBlendAutoRecovery() {
            if (m_canvas) {
                m_canvas->setOpacityPainting(m_nOpacityPainting);
            }
        }

        void recover() {
            if (m_canvas) {
                m_canvas->setOpacityPainting(m_nOpacityPainting);
                m_canvas = nullptr;
            }
        }

    protected:
        CRawGraph                   *m_canvas;
        int                         m_nOpacityPainting;

    };

    bool create(int cx, int cy, WindowHandle windowHandle, int nBitCount = 32) override;

    int width() const { return m_width; }
    int height() const { return m_height; }

    void drawToWindow(int xdest, int ydest, int width, int height, int xsrc, int ysrc);

    void line(float x1, float y1, float x2, float y2);

    void rectangle(float x, float y, float width, float height);
    void roundedRect(float x, float y, float width, float height, float radius);
    void fillRoundedRect(float x, float y, float width, float height, float radius, const CColor &clrFill);
    void fillPath(const VecPoints &points, const CColor &clrFill);

    void setPen(const CRawPen &pen);

    void getClipBoundBox(CRect &rc);
    void setClipBoundBox(const CRect &rc);
    void resetClipBoundBox(const CRect &rc);
    void clearClipBoundBox();

    bool textOut(float x, float y, cstr_t szText, size_t nLen);
    bool textOut(float x, float y, const StringView &text)
        { return textOut(x, y, text.data, text.len); }
    bool drawTextClip(cstr_t szText, size_t nLen, const CRect &rcPos, float xLeftClipOffset = 0);
    bool drawTextClip(const StringView &text, const CRect &rcPos, float xLeftClipOffset = 0)
        { return drawTextClip(text.data, text.len, rcPos, xLeftClipOffset); }

    bool textOutOutlined(float x, float y, cstr_t szText, size_t nLen, const CColor &clrText, const CColor &clrBorder);
    bool textOutOutlined(float x, float y, const StringView &text, const CColor &clrText, const CColor &clrBorder)
        { return textOutOutlined(x, y, text.data, text.len, clrText, clrBorder); }
    bool drawTextClipOutlined(cstr_t szText, size_t nLen, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, float xLeftClipOffset = 0);
    bool drawTextClipOutlined(const StringView &text, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, float xLeftClipOffset = 0)
        { return drawTextClipOutlined(text.data, text.len, rcPos, clrText, clrBorder, xLeftClipOffset); }

    bool drawTextOutlined(cstr_t szText, size_t nLen, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, uint32_t uFormat = DT_SINGLELINE | DT_LEFT);
    bool drawTextOutlined(const StringView &text, const CRect &rcPos, const CColor &clrText, const CColor &clrBorder, uint32_t uFormat = DT_SINGLELINE | DT_LEFT)
        { return drawTextOutlined(text.data, text.len, rcPos, clrText, clrBorder, uFormat); }

    bool drawText(cstr_t szText, size_t nLen, const CRect &rcPos, uint32_t uFormat = DT_SINGLELINE | DT_LEFT);
    bool drawText(const StringView &text, const CRect &rcPos, uint32_t uFormat = DT_SINGLELINE | DT_LEFT)
        { return drawText(text.data, text.len, rcPos, uFormat); }

    bool getTextExtentPoint32(cstr_t szText, size_t nLen, CSize *pSize);
    bool getTextExtentPoint32(const StringView &text, CSize *size)
        { return getTextExtentPoint32(text.data, text.len, size); }

    void setTextColor(const CColor &color) { m_clrText = color; }

    void setFont(CRawBmpFont *font);

    void resetAlphaChannel(uint8_t byAlpha, const CRect &rcDst);
    void multiplyAlpha(uint8_t byAlpha, const CRect &rcDst);

    void vertAlphaFadeOut(const CRect &rcDst, bool bTop);
    void vertFadeOut(const CRect &rcDst, const CColor &clrBg, bool bTop);

    void horzAlphaFadeOut(const CRect &rcDst, bool bLeft);
    void horzFadeOut(const CRect &rcDst, const CColor &clrBg, bool bLeft);

    void fillRectXOR(const CRect &rcDst, const CColor &clrFill);

    void fillRect(const CRect &rcDst, CRawImage &imageMask, const CRect &rcMask, const CColor &clrFill, BlendPixMode bpm = BPM_COPY);
    void fillRect(const CRect &rcDst, const CColor &clrFill, BlendPixMode bpm = BPM_COPY);

    void fillRect(float x, float y, float nWidth, float nHeight, const CColor &clrFill, BlendPixMode bpm = BPM_COPY) {
        fillRect(CRect(x, y, x + nWidth, y + nHeight), clrFill, bpm);
    }

    // 物理像素坐标填充（已含 scale/origin），用于频谱等需像素级等宽的绘制。
    void fillRectPhysical(int x, int y, int xMax, int yMax, const CColor &clrFill, BlendPixMode bpm = BPM_COPY);

    // 目标与源均为逻辑坐标（与 fillRect / stretchBlt 一致）；image 为同 scale 的物理缓冲，内部统一换算。
    void bltImage(int xDest, int yDest, int widthDest, int heightDest, RawImageData *image, int xSrc, int ySrc, BlendPixMode bpm = BPM_BLEND, int nOpacitySrc = 255);

    int getOpacityPainting() const { return m_nOpacityPainting; }
    int setOpacityPainting(int nOpacityPainting) { int nOrg = m_nOpacityPainting; m_nOpacityPainting = nOpacityPainting; return nOrg; }

    bool setEnableAlphaChannel(bool bEnable) {
        bool bOrg = m_bAlphaChannelEnabled;
        m_bAlphaChannelEnabled = (bEnable && m_imageData.bitCount == 32);
        return bOrg;
    }
    bool getEnableAlphaChannel() const { return m_bAlphaChannelEnabled; }

    //
    // The following APIs provide logical coordinate and real coordinate convert.
    //
    CPoint resetOrigin(const CPoint &ptOrg);
    void getOrigin(CPoint &ptOrg) const { ptOrg = m_ptOrigin; }

    template<class _float> _float mapAndScaleX(_float x) const { return (_float)((x + m_ptOrigin.x) * m_scaleFactor); }
    template<class _float> _float mapAndScaleY(_float y) const { return (_float)((y + m_ptOrigin.y) * m_scaleFactor); }
    void mapAndScale(CRect &r) const;

    inline float scale(float n) const { return (int)(n * m_scaleFactor); }
    inline CRect scale(const CRect &rc) const {
        return CRect((int)(rc.left * m_scaleFactor), (int)(rc.top * m_scaleFactor),
                     (int)(rc.right * m_scaleFactor), (int)(rc.bottom * m_scaleFactor));
    }

    float getScaleFactor() const { return m_scaleFactor; }

protected:
    bool clipMapScaleRect(const CRect &rcDst, int &x, int &y, int &xMax, int &yMax);

protected:
    float                       m_scaleFactor;

    CColor                      m_clrText;

    CRawPen                     m_pen;

    int                         m_width, m_height;

    CRect                       m_rcClip;
    CRect                       m_rcClipScaleMaped;

    CRawBmpFont                 *m_rawBmpFont;

    // The Opacity of painting (lines, text, images) to graph
    int                         m_nOpacityPainting;

    // The origin of graph, default is 0, 0.
    CPoint                      m_ptOrigin;

    bool                        m_bAlphaChannelEnabled;

};

#include "RawImage.h"
// #include "RawBmpFont.h"


#endif // !defined(GfxRaw_RawGraph_h)
