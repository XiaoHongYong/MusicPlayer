#include "SkinTypes.h"
#include "Skin.h"
#include "SkinTabButton.h"


UIOBJECT_CLASS_NAME_IMP(CSkinTabButton,  "TabButton")

CSkinTabButton::CSkinTabButton() {
    m_bDrawBtText = true;

    m_nSperatorLineWidth = 1;
    m_nButtonFaceWidth = 1;
    m_nButtonBorderWidth = 1;
}

CSkinTabButton::~CSkinTabButton() {
}

void CSkinTabButton::draw(CRawGraph *canvas) {
    CSkinToolbar::draw(canvas);

    int nWidth = 0;
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        nWidth += m_vButtons[i].nWidth;
    }
    m_rcObj.right = m_rcObj.left + nWidth;
}

void CSkinTabButton::drawButton(CRawGraph *canvas, int nButton, BT_DRAW_STATE btDrawState, int x, int y, int nHeight, int nBtImageLeft) {
    Button &bt = m_vButtons[nButton];
    int xOrg = x;

    calculateBtWidth(canvas, bt);

    int nBtImageTop = m_nImageHeight * btDrawState;
    int nButtonWidth = bt.nWidth;

    nBtImageLeft = 0;
    if (nButton != m_vButtons.size() - 1) {
        nButtonWidth--;
    }

    if (bt.nCurStatus == Button::STATUS_CHECKED) {
        nBtImageTop += m_nImageHeight * ROW_INTERACTIVE_COUNT;
    }

    // draw button left border
    int nBorderLeft = m_nMarginX + nBtImageLeft;
    if (nButton != 0) {
        nBorderLeft += m_nButtonBorderWidth * 2 + m_nButtonFaceWidth + m_nSperatorLineWidth;
    }
    m_image.blt(canvas, x, y, m_nButtonBorderWidth, nHeight, nBorderLeft, nBtImageTop);
    x += m_nButtonBorderWidth;

    // draw button face
    int nFaceLeft = nBorderLeft + m_nButtonBorderWidth;
    int nFaceWidth = nButtonWidth - m_nButtonBorderWidth * 2;
    m_image.xTileBlt(canvas, x, y, nFaceWidth, nHeight, nFaceLeft, nBtImageTop,
        m_nButtonFaceWidth, m_nImageHeight);
    x += nFaceWidth;

    // draw button right border
    int nRightBorderLeft = nBtImageLeft;
    if (nButton == m_vButtons.size() - 1) {
        nRightBorderLeft += m_image.m_cx - m_nMarginX - m_nButtonBorderWidth;
    } else {
        nRightBorderLeft += m_nMarginX + m_nButtonBorderWidth + m_nButtonFaceWidth;
    }
    m_image.blt(canvas, x, y, m_nButtonBorderWidth, nHeight, nRightBorderLeft, nBtImageTop);
    x += m_nButtonBorderWidth;

    // draw button sepeartor
    if (nButton != m_vButtons.size() - 1) {
        // If next button is checked, draw seperator highlight.
//        if (m_vButtons[nButton + 1].nCurStatus == Button::STATUS_CHECKED) {
//            nBtImageLeft = m_image.m_cx;
//        }
        int nSeperatorLeft = nBtImageLeft + m_nMarginX
        + m_nButtonBorderWidth * 2 + m_nButtonFaceWidth;
        m_image.blt(canvas, x, y, m_nSperatorLineWidth, nHeight, nSeperatorLeft, nBtImageTop);
    }

    // draw Button text
    if (bt.strText.size()) {
        CRect rcText(xOrg, y, xOrg + bt.nWidth, y + nHeight);
        CColor clrText = m_font.getTextColor(m_enable);
        if (btDrawState == ROW_DOWN || bt.nCurStatus == Button::STATUS_CHECKED) {
            clrText = m_clrTextSelected;
        }
        canvas->setTextColor(clrText);

        canvas->drawText(bt.strText.c_str(), (int)bt.strText.size(), rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void CSkinTabButton::onCreate() {
    CSkinToolbar::onCreate();

    m_nBlankX = -1;

    // 2 * ( <MarginX> <ButtonBorder> <ButtonFace> <ButtonBorder> <SperatorLine> <ButtonBorder> <ButtonFace> <ButtonBorder> <MarginX>)
    m_nButtonFaceWidth = (m_image.m_cx - m_nMarginX * 2 - m_nSperatorLineWidth) / 2
    - m_nButtonBorderWidth * 2;

    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        m_vButtons[i].nWidthImage = m_vButtons[i].nWidth = m_nButtonBorderWidth * 2 + m_nButtonFaceWidth;
        m_vButtons[i].nGroup = 1;
        m_vButtons[i].nStatusMax = Button::STATUS_CHECKED;
    }
}

bool CSkinTabButton::onMenuKey(uint32_t nChar, uint32_t nFlags) {
    //     if (toUpper(m_chMenuKey) == toUpper(nChar))
    //     {
    //         if (m_id != ID_INVALID)
    //             m_pSkin->postCustomCommandMsg(m_id);
    //
    //         if (m_vBtStatImg[m_nCurStatus]->nIDCmd != ID_INVALID)
    //             m_pSkin->postCustomCommandMsg(m_vBtStatImg[m_nCurStatus]->nIDCmd);
    //         return true;
    //     }

    return false;
}

bool CSkinTabButton::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CSkinToolbar::setProperty(szProperty, szValue)) {
        return true;
    }

    if (isPropertyName(szProperty, "SperatorLineWidth")) {
        m_nSperatorLineWidth = atoi(szValue);
    } else if (isPropertyName(szProperty, "ButtunBorderWidth")) {
        m_nButtonBorderWidth = atoi(szValue);
    }

    return true;
}
