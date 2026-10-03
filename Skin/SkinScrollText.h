#pragma once


class CSkinScrollText : public CSkinStaticText {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinStaticText)
public:
    CSkinScrollText();
    virtual ~CSkinScrollText();

    void draw(CRawGraph *canvas) override;

    //     bool setProperty(cstr_t szProperty, cstr_t szValue) override;
    //     void enumProperties(CUIObjProperties &listProperties);

    //    void onCreate();

    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;

    void onTimer(int nId) override;

protected:
    // 滚动文本的整串离屏缓存 + 缓存签名，见 getOrBuildTextCache()。
    CRawGraph *getOrBuildTextCache(CRawGraph *canvas);

    // 当前帧的水平滚动偏移；draw() 与 onTimer() 共用，保证偏移不变即画面不变。
    int getDrawOffset(int nVisibleWidth) const;

    int                         m_nTimerIDScroll;
    int                         m_nPosScroll;
    bool                        m_bToLeft;
    int                         m_nWidthText;

    CRawGraph                   *m_pTextCache;      // 整串离屏渲染（scale 后 backing 空间）
    int                         m_nCacheWidth;      // 构建时的逻辑文本宽
    int                         m_nCacheScale;
    void                        *m_cacheFont;       // CRawBmpFont* 身份
    CColor                      m_cacheClrTxt, m_cacheClrBorder;
    bool                        m_cacheOutlined;

};
