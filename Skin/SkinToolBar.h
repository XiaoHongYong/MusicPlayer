#pragma once

#ifndef Skin_SkinToolBar_h
#define Skin_SkinToolBar_h


class CSkinToolbar : public CUIObject {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    struct Button {
        Button();

        enum {
            STATUS_NORMAL               = 0,
            STATUS_CHECKED              = 1,
            STATUS_MAX                  = 4
        };

        int                         nID;
        int                         nAutoUniID;
        int16_t                     nWidth;
        int16_t                     nWidthImage;
        bool                        bSeperator;

        int8_t                      nGroup;

        string                      strText, strTextEnglish;

        uint8_t                     nCurStatus;
        uint8_t                     nStatusMax;
        int16_t                     nLeftPositon[STATUS_MAX + 1];

        bool                        bEnabled;
        int                         nLeftOfDiabled;     // Image position of disabled

        string                      strTooltip;
        bool                        bTempToolTip;
        bool                        bContinuousCmd;     // Will the cmd be triggered continuous?

        void fromXML(CSkinToolbar *pToolbar, SXNode *pNode);
#ifdef _SKIN_EDITOR_
        void toXML(CSkinToolbar *pToolbar, CXMLWriter &xml);
#endif // _SKIN_EDITOR_

    };

    friend struct Button;

    typedef vector<Button>        vec_buttons;

public:
    CSkinToolbar();
    virtual ~CSkinToolbar();

    void onCreate() override;

    void onAdjustHue(float hue, float saturation, float luminance) override;

    void onSize() override;
    void onTimer(int nId) override;

    bool onMouseDrag(CPoint point) override;
    bool onMouseMove(CPoint point) override;
    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;

    bool onCommand(uint32_t nId) override;

    void onLanguageChanged() override;

    void draw(CRawGraph *canvas) override;

    bool setProperty(cstr_t szProperty, cstr_t szValue) override;
#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    virtual void setVisible(bool bVisible, bool bRedraw = false) override;

    virtual int fromXML(SXNode *pXmlNode) override;

    bool isCheck(int nCmdID);
    void setCheck(int nCmdID, bool bCheck, bool bRedraw = true);

    void setBtStatus(int nCmdID, int nStatus, bool bRedraw);
    int getBtStatus(int nCmdID);

    bool isBtEnabled(int nCmdID);
    void enableBt(int nCmdID, bool bEnable);

    bool isButtonExist(int nCmdID);

#ifdef _SKIN_EDITOR_
    // 在导出到xml时，写完属性，写内容时调用
    virtual bool hasXMLChild() { return true; }
    virtual void onToXMLChild(CXMLWriter &xmlStream);
#endif // _SKIN_EDITOR_

protected:
    void invalidateButton(int nButton);
    int getCurButton(CPoint pt);

    void groupButtonUncheckOld(int nGroup, int nCurButton);

    void addTooltip();
    void delTooltip();

    enum BT_DRAW_STATE {
        ROW_NORMAL,
        ROW_HOVER,
        ROW_DOWN,
        ROW_MAX,
    };

    void calculateBtWidth(CRawGraph *canvas, Button &bt);

    virtual void drawButton(CRawGraph *canvas, int nButton, BT_DRAW_STATE btDrawState, int x, int y, int nHeight, int nBtImageLeft);

    void drawCurButtonFadein(CRawGraph *canvas, int nButton, BT_DRAW_STATE btDrawState, int x, int y, int nHeight, int nBtImageLeft);

protected:
    string                      m_strImageFile;
    CSFImage                    m_image;
    int                         m_nImageHeight;

    CColor                      m_clrTextSelected;

    // Background image of normal state, and background image of checked button.
    string                      m_strImageBgFile, m_strImageBgCheckedFile;
    CSFImage                    m_imageBg, m_imageBgChecked;

    // The actual position or size of button will be multipled by m_nUnitsOfX.
    int                         m_nUnitsOfX;

    bool                        m_bEnableHover;
    bool                        m_bHover;
    bool                        m_bLBtDown;

    // blank images position
    int                         m_nBlankX;
    int                         m_nBlankCX;

    // seperator images position
    int                         m_nSeperatorX;
    int                         m_nSeperatorCX;

    // The spaces between two buttons.
    int                         m_nBtSpacesCX;

    int                         m_nMarginX, m_nMarginY;

    vec_buttons                 m_vButtons;

    int                         m_nCurButton;

    CSkinFontProperty           m_font;
    bool                        m_bDrawBtText;

    // Has full status image (normal, down, hover)
    bool                        m_bFullStatusImage;

    // Sending continuous command message, if the button is down.
    bool                        m_bContinuousBegin;

    bool                        m_bFadein;
    int64_t                     m_timeBeginFadein;
    int                         m_nLastImageLeft;
    BT_DRAW_STATE               m_nLastImageDrawState;

};

#endif // !defined(Skin_SkinToolBar_h)
