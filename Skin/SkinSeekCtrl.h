#pragma once

#ifndef Skin_SkinSeekCtrl_h
#define Skin_SkinSeekCtrl_h


class CSkinSeekCtrlEventNotify : public IUIObjNotify {
public:
    CSkinSeekCtrlEventNotify(CUIObject *pObject) : IUIObjNotify(pObject) { cmd = C_BEG_DRAG; }
    enum Command {
        C_BEG_DRAG,
        C_END_DRAG,
    };

    Command                     cmd;

};

class CSkinSeekCtrl : public CUIObject, public IScrollBar {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    enum PUSH_DOWN_POS {
        PUSH_DOWN_NONE,
        PUSH_DOWN_TOPTRACK,              // 滚动条的空白位置，进行翻页操作
        PUSH_DOWN_BOTTOMTRACK,           // 滚动条的空白位置，进行翻页操作
        PUSH_DOWN_THUMB,                 // 滚动条的拖动box(thumb)
    };

public:
    CSkinSeekCtrl();
    virtual ~CSkinSeekCtrl();

    void setScrollInfo(int nMin, int nMax, int nPage = 0, int nPos = 0, int nLine = 1, bool bRedraw = true) override;
    int setScrollPos(int nPos, bool bRedraw = true) override;

    int getScrollPos() const override { return m_nVirtualCurPos; }
    int getPage() const override { return 0; }
    int getMin() const override { return m_nVirtualMin; }
    int getMax() const override;
    int getID() const override { return m_id; }

    // 异步 ID_SEEK 到达前缓存目标位置，避免进度定时器用旧 getPlayPos 覆盖后再 seek 回去
    void setPendingSeekPos(int nPos) { m_pendingSeekPos = nPos; }
    int getPendingSeekPos() const { return m_pendingSeekPos; }
    // 取出并清除 pending；无 pending 时返回 -1
    int takePendingSeekPos();

    void setScrollNotify(IScrollNotify *pNofity) override;

    virtual bool isEnabled() const override { return m_enable; }
    virtual void disableScrollBar() override { }
    virtual bool handleScrollCode(uint32_t nSBCode, int nPos) override { return true; }

    bool onKeyDown(uint32_t nChar, uint32_t nFlags) override;
    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    bool onMouseDrag(CPoint point) override;
    bool onMouseMove(CPoint point) override;
    void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) override;
    void onMouseEnter(CPoint point) override;
    void onMouseLeave(CPoint point) override;

    void onVScroll(uint32_t nSBCode);

    void onSize() override;

    void draw(CRawGraph *canvas) override;

    virtual bool setProperty(cstr_t szProperty, cstr_t szValue) override;
#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

protected:
    PUSH_DOWN_POS getPushDownPos(CPoint pt);

    int getTrackRealWidth() { return m_rcObj.width() - m_nWidthThumb; }

    int virtualPosToObjectPos(int nVirtualPos);
    int objectPosToVirtualPos(int nThumbPos);

    void thumbOnLButtonDown(uint32_t nFlags, CPoint point);
    void trackOnLButtonDown(uint32_t nFlags, CPoint point);

    void thumbOnMouseMove(CPoint point);

    void notifyEvent(CSkinSeekCtrlEventNotify::Command cmd);

protected:
    // 基本设置
    int                         m_nWidthEnd;        // 两边端点的宽度
    int                         m_nWidthThumb;      // 拖动块的宽度

    int                         m_nPosThumb;        // 滚动条的位置(上面按钮的下方 －> thumb的上方的距离)

    // 虚拟滚动位置
    int                         m_nVirtualMin;      // 最小的值
    int                         m_nVirtualMax;      // 最大的值
    int                         m_nVirtualLine;     // 滚动一行的值，缺省为1
    int                         m_nVirtualPage;
    int                         m_nVirtualCurPos;

    int                         m_nXCursorToThumbBeg; // 鼠标点下位置距拖动ThumbBox 距离: point.y - m_rcObj.top - m_nHeightEnd;
    PUSH_DOWN_POS               m_pushDownPos;      // 鼠标按下的位置
    PUSH_DOWN_POS               m_cursorPosLatest;  // 鼠标按下的位置
    int                         m_pendingSeekPos = -1; // >=0 表示有待处理的 seek 目标

    CSFImage                    m_imgTrackNormal, m_imgTrackPlayed;
    CSFImage                    m_imgThumb;
    string                      m_strImageTrack, m_strImageThumb;

    IScrollNotify               *m_pScrollNotify;

};

#endif // !defined(Skin_SkinSeekCtrl_h)
