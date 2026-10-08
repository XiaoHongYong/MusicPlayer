#include "Skin.h"
#include "SkinScrollText.h"


#define TIMER_SPAN_SCROLL   30
#define WIDTH_TXT_STILL            (2000 / TIMER_SPAN_SCROLL)

UIOBJECT_CLASS_NAME_IMP(CSkinScrollText, "ScrollText")

CSkinScrollText::CSkinScrollText() {
    m_nTimerIDScroll = 0;
    m_nPosScroll = WIDTH_TXT_STILL;
    m_nWidthText = 0;
    m_bToLeft = true;

    m_pTextCache = nullptr;
    m_nCacheWidth = -1;
    m_nCacheScale = 0;
    m_cacheFont = nullptr;
    m_cacheOutlined = false;
}

CSkinScrollText::~CSkinScrollText() {
    delete m_pTextCache;
    m_pTextCache = nullptr;
}
//
// bool CSkinScrollText::onLButtonUp(uint32_t nFlags, CPoint point)
// {
//     if (!m_enable)
//         return false;
//
//     if (m_id != ID_INVALID)
//         m_pSkin->postCustomCommandMsg(m_id);
//
//     return true;
// }

void CSkinScrollText::draw(CRawGraph *canvas) {
    CRawGraph::CClipBoxAutoRecovery clipRecover(canvas);
    canvas->setClipBoundBox(m_rcObj);

    CUIObject::draw(canvas);

    if (m_font.isGood()) {
        canvas->setFont(m_font.getFont());
    }

    CRect rc = m_rcObj;

    rc.left += m_nLeftMargin;
    rc.top = m_rcObj.top + (m_rcObj.height() - m_font.getHeight()) / 2;
    rc.bottom = rc.top + m_font.getHeight();

    CSize size;
    canvas->getTextExtentPoint32(m_strText.c_str(), m_strText.size(), &size);
    size.cx += 3;
    if (m_nWidthText != size.cx) {
        m_nWidthText = size.cx;
        m_nPosScroll = 0;
        m_bToLeft = true;
    }

    // set timer to scroll
    if (m_nWidthText > rc.width() && m_nTimerIDScroll == 0) {
        m_nTimerIDScroll = m_pSkin->registerTimerObject(this, TIMER_SPAN_SCROLL);
    }

    // kill timer
    if (m_nWidthText <= rc.width() && m_nTimerIDScroll != 0) {
        m_pSkin->unregisterTimerObject(this, m_nTimerIDScroll);
        m_nTimerIDScroll = 0;
        m_nPosScroll = -WIDTH_TXT_STILL;
    }

    int nVisibleWidth = rc.width();
    int nLeftClip = getDrawOffset(nVisibleWidth);

    // 文本未超出可视区时尊重 AlignText（滚动中仍从左裁切）。
    if (nLeftClip == 0 && m_nWidthText < nVisibleWidth) {
        if (isFlagSet(m_dwAlignText, AT_CENTER)) {
            rc.left += (nVisibleWidth - m_nWidthText) / 2;
            rc.right = rc.left + m_nWidthText;
        } else if (isFlagSet(m_dwAlignText, AT_RIGHT)) {
            rc.left += nVisibleWidth - m_nWidthText;
        }
    }

    rc.left -= nLeftClip;

    // 整串先栅格化进离屏缓存，每帧只做一次切片 blt：滚动只改切片原点，不再每帧逐字形重跑 drawGlyph*。
    // 仅当串/字体/颜色/描边开关/scale 变化（换歌、切显示项等）才重建缓存。
    CRawGraph *pCache = getOrBuildTextCache(canvas);
    if (pCache) {
        // 整串缓存里文本像素 i 对应屏幕 (rc.left + nLeftClip) + (i - nLeftClip)：最终文本左端
        // (m_rcObj.left + m_nLeftMargin) 处放第 nLeftClip 个文本像素，窗口正好显示文本的
        // [nLeftClip - m_nLeftMargin … +nVisibleWidth] 这段。因此 blt 目标锚定在减去前的
        // 原始左端 rc.left + nLeftClip，源从第 nLeftClip 个像素开始，宽度固定为可视窗 nVisibleWidth。
        // 注意 rc.left 上面已 -= nLeftClip：这一处偏移只能作用在目标或源之一，不能两处都算，
        // 否则右移滚动时 blt 覆盖不到窗口右端，文字滚着滚着会整串消失。
        RawImageData *s = pCache->getRawBuff();
        int nScale = (int)canvas->getScaleFactor();
        canvas->bltImage(canvas->mapAndScaleX(rc.left + nLeftClip), canvas->mapAndScaleY(rc.top),
            nVisibleWidth * nScale, s->height,
            s, nLeftClip * nScale, 0, BPM_BLEND);
        return;
    }
    // 缓存不可用（空文本/离屏构建失败）：回退原动态路径，逐帧重绘字形。
    // 与缓存路径同参：rc.left 已 -= nLeftClip，文本左端本就随滚动左移、窗口左缘自然裁掉
    // 滚出头的部分，因此 xLeftClipOffset 不再按 nLeftClip 额外裁剪（否则偏移算两遍，最右端整串消失）。
    if (m_font.isOutlined()) {
        canvas->drawTextClipOutlined(m_strText.c_str(), (int)m_strText.size(), rc, m_font.getTextColor(m_enable), m_font.getColorOutlined(), 0);
    } else {
        canvas->setTextColor(m_font.getTextColor(m_enable));
        canvas->drawTextClip(m_strText.c_str(), (int)m_strText.size(), rc, 0);
    }
}
//
// bool CSkinScrollText::setProperty(cstr_t szProperty, cstr_t szValue)
// {
//     if (CSkinStaticText::setProperty(szProperty, szValue))
//         return true;
//     else
//         return false;
//
//     return true;
// }
//
// void CSkinScrollText::enumProperties(CUIObjProperties &listProperties)
// {
//     CSkinStaticText::enumProperties(listProperties);
// }
//
// void CSkinScrollText::onCreate()
// {
// //     m_nTimerIDScroll = m_pSkin->registerTimerObject(this, TIMER_SPAN_SCROLL);
// }

CRawGraph *CSkinScrollText::getOrBuildTextCache(CRawGraph *canvas) {
    // 缓存签名：任一输入变化即重建，否则复用。
    void *pFont = m_font.getFont();
    const CColor &clrTxt = m_font.getTextColor(m_enable);
    const CColor &clrBorder = m_font.getColorOutlined();
    int nScale = (int)canvas->getScaleFactor();
    bool bOutlined = m_font.isOutlined();

    bool bStale = (m_pTextCache == nullptr)
        || m_nCacheScale != nScale
        || m_cacheFont != pFont
        || m_nCacheWidth != m_nWidthText
        || m_cacheOutlined != bOutlined
        || !(m_cacheClrTxt == clrTxt)
        || (bOutlined && !(m_cacheClrBorder == clrBorder));

    if (!bStale) {
        return m_pTextCache;
    }

    delete m_pTextCache;
    m_pTextCache = nullptr;

    if (m_nWidthText <= 0 || m_strText.empty()) {
        return nullptr;
    }

    int nH = m_font.getHeight();   // 与 draw() 中 rc 的垂直盒高一致，保证 blt 顶对齐 rc.top 即原摆位

    CRawGraph *pImg = new CRawGraph(canvas->getScaleFactor());
    if (!pImg->create(m_nWidthText, nH, (WindowHandle)0, 32)) {
        delete pImg;
        return nullptr;
    }
    pImg->fillRect(CRect(0, 0, m_nWidthText, nH), CColor(0, 0), BPM_COPY);
    pImg->setFont(m_font.getFont());

    // 与动态路径同参渲染整串（rc 原点化、nLeftClip=0），切片 blt 即与原绘制逐像素一致。
    if (bOutlined) {
        pImg->drawTextClipOutlined(m_strText.c_str(), (int)m_strText.size(), CRect(0, 0, m_nWidthText, nH), clrTxt, clrBorder, 0);
    } else {
        pImg->setTextColor(clrTxt);
        pImg->drawTextClip(m_strText.c_str(), (int)m_strText.size(), CRect(0, 0, m_nWidthText, nH), 0);
    }

    m_pTextCache = pImg;
    m_nCacheScale = nScale;
    m_cacheFont = pFont;
    m_nCacheWidth = m_nWidthText;
    m_cacheOutlined = bOutlined;
    m_cacheClrTxt = clrTxt;
    m_cacheClrBorder = clrBorder;
    return pImg;
}

bool CSkinScrollText::onLButtonUp(uint32_t nFlags, CPoint point) {
    return true;
}

bool CSkinScrollText::onLButtonDown(uint32_t nFlags, CPoint point) {
    return true;
}

// 当前帧实际使用的水平滚动偏移（逻辑像素），draw() 与 onTimer() 共用同一份计算。
// m_nPosScroll <= 0（左端静止）以及滚到最右被钳位时，画面不再变化。
int CSkinScrollText::getDrawOffset(int nVisibleWidth) const {
    if (m_nTimerIDScroll == 0 || m_nPosScroll <= 0) {
        return 0;
    }

    if (m_nPosScroll + nVisibleWidth >= m_nWidthText) {
        int nMax = m_nWidthText - nVisibleWidth;
        return nMax > 0 ? nMax : 0;
    }

    return m_nPosScroll;
}

void CSkinScrollText::onTimer(int nId) {
    if (nId != m_nTimerIDScroll) {
        return;
    }

    // 静止阶段（m_nPosScroll 在 <=0 或超过最右端被钳位的那一段）画面与上一帧逐像素
    // 完全相同，原来每 tick 都 invalidate()，等于用 30ms 的节奏白刷一屏。WIDTH_TXT_STILL
    // 让两端各静止 2000/TIMER_SPAN_SCROLL 个 tick，这部分帧现在直接跳过。
    int nOffsetBefore = getDrawOffset(m_rcObj.width() - m_nLeftMargin);

    if (m_bToLeft) {
        m_nPosScroll--;
        if (m_nPosScroll <= -WIDTH_TXT_STILL) {
            m_bToLeft = false;
            m_nPosScroll = 0;
        }
    } else {
        // maxOffset = m_nWidthText - nVisibleWidth，nVisibleWidth = m_rcObj.width() - m_nLeftMargin。
        // 注意是 +m_nLeftMargin：若误写成 -，最大偏移会少 2*m_nLeftMargin，
        // 左margin 较大（≥ WIDTH_TXT_STILL/2）时根本滚不到最右端，最后一个词永远显示不完整。
        int nMaxScroll = m_nWidthText - (m_rcObj.width() - m_nLeftMargin);
        m_nPosScroll++;
        if (m_nPosScroll >= nMaxScroll + WIDTH_TXT_STILL) {
            m_bToLeft = true;
            m_nPosScroll = nMaxScroll;
        }
    }

    if (getDrawOffset(m_rcObj.width() - m_nLeftMargin) != nOffsetBefore) {
        invalidate();
    }
}
