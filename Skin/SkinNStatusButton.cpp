#include "SkinTypes.h"
#include "Skin.h"
#include "SkinNStatusButton.h"


#define FADE_BUTTON_TIME_OUT 300



UIOBJECT_CLASS_NAME_IMP(CSkinNStatusButton, "NStatusButton")

CSkinNStatusButton::CSkinNStatusButton() {
    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON | UO_MSG_WANT_ENTER_KEY;

    m_xExtendStart = m_xExtendEnd = -1;
    m_bTile = true;

    m_btnState = BS_NORMAL;
    m_nCurStatus = 0;

    m_bEnableHover = false;

    m_bContinuousBegin = false;
    m_bContinuousCmd = false;
    m_bpm = BPM_BLEND;
    m_nTimerIdContinuous = 0;

    m_bFadein = true;
    m_nTimerIdFadein = 0;
    m_timeBeginFadein = 0;
    m_pLastImage = nullptr;
}

CSkinNStatusButton::~CSkinNStatusButton() {
    destroy();
}

bool CSkinNStatusButton::onLButtonDown(uint32_t nFlags, CPoint point) {
    assert(m_pSkin);

    if (!isPtIn(point)) {
        return false;
    }

    buttonDownAction();

    return true;
}

void CSkinNStatusButton::onTimer(int nId) {
    if (m_nTimerIdFadein != 0) {
        // In fade in, just redraw
        invalidate();
        return;
    }

    assert(nId == m_nTimerIdContinuous);

    if (m_btnState == BS_PRESSED) {
        CPoint pt = getCursorPos();
        m_pSkin->screenToClient(pt);

        if (isPtIn(pt)) {
            if (m_id != ID_INVALID) {
                m_pSkin->postCustomCommandMsg(m_id);
            }

            if (m_nCurStatus >= 0 && m_nCurStatus < (int)m_vBtStatImg.size()) {
                if (m_vBtStatImg[m_nCurStatus]->nIDCmd != ID_INVALID) {
                    m_pSkin->postCustomCommandMsg(m_vBtStatImg[m_nCurStatus]->nIDCmd);
                }
            }
        }

        if (m_bContinuousBegin) {
            if (m_nTimerIdContinuous) {
                m_pSkin->unregisterTimerObject(this, m_nTimerIdContinuous);
            }
            m_nTimerIdContinuous = m_pSkin->registerTimerObject(this, 100);
            m_bContinuousBegin = false;
        }
    } else {
        m_pSkin->unregisterTimerObject(this, m_nTimerIdContinuous);
        m_nTimerIdContinuous = 0;
    }
}

bool CSkinNStatusButton::onLButtonUp(uint32_t nFlags, CPoint point) {
    // 点击动作由 SkinWnd 派发的 onLButtonClick → buttonUpAction 处理
    if (m_bContinuousCmd && m_btnState == BS_PRESSED) {
        m_pSkin->unregisterTimerObject(this, m_nTimerIdContinuous);
        m_nTimerIdContinuous = 0;
    }
    return true;
}

bool CSkinNStatusButton::onLButtonClick(uint32_t nFlags, CPoint point) {
    buttonUpAction();
    return true;
}

void CSkinNStatusButton::onMouseEnter(CPoint point) {
    changeButtonState(BS_HOVER);
}

void CSkinNStatusButton::onMouseLeave(CPoint point) {
    changeButtonState(BS_NORMAL);
}

bool CSkinNStatusButton::onKeyUp(uint32_t nChar, uint32_t nFlags) {
    if (nChar == VK_SPACE) {
        buttonUpAction();
        return true;
    }

    return false;
}

bool CSkinNStatusButton::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (nChar == VK_SPACE && m_btnState != BS_PRESSED) {
        buttonDownAction();
    } else if (nChar == VK_RETURN) {
        buttonDownAction();
        buttonUpAction();
    } else {
        return false;
    }

    return true;
}

void CSkinNStatusButton::onSetFocus() {
    changeButtonState(BS_HOVER);

    invalidate();
}

void CSkinNStatusButton::onKillFocus() {
    if (!m_isMouseIn) {
        changeButtonState(BS_NORMAL);
    }
}

void CSkinNStatusButton::draw(CRawGraph *canvas) {
    if (m_vBtStatImg.empty()) {
        return;
    }

    if (m_nCurStatus < 0 || m_nCurStatus >= (int)m_vBtStatImg.size()) {
        m_nCurStatus = 0;
    }

    BtStatImg *btimg = m_vBtStatImg[m_nCurStatus];
    CSFImage *pImage = nullptr;

    if (m_enable) {
        if (m_btnState == BS_PRESSED && btimg->imgSel.isValid()) {
            pImage = &btimg->imgSel;
        } else if (m_bEnableHover && m_btnState == BS_HOVER && btimg->imgHover.isValid()) {
            pImage = &btimg->imgHover;
        } else if (btimg->imgBk.isValid()) {
            pImage = &btimg->imgBk;
        }
    } else {
        if (btimg->imgDisabled.isValid()) {
            pImage = &btimg->imgDisabled;
        } else if (btimg->imgBk.isValid()) {
            pImage = &btimg->imgBk;
        }
    }

    if (!pImage) {
        return;
    }


    if (m_nTimerIdFadein != 0) {
        auto now = getTickCount();

        if (now - m_timeBeginFadein >= FADE_BUTTON_TIME_OUT) {
            m_pSkin->unregisterTimerObject(this, m_nTimerIdFadein);
            m_nTimerIdFadein = 0;
        } else {
            int nAlpha = int(now - m_timeBeginFadein) * 255 / FADE_BUTTON_TIME_OUT;
            fadeInDrawBt(canvas, pImage, nAlpha);
            return;
        }
    }

    if (m_xExtendStart == -1) {
        if (m_imgMask.isValid()) {
            pImage->maskBlt(canvas, m_rcObj.left, m_rcObj.top, &m_imgMask, m_bpm);
        } else {
            pImage->blt(canvas, m_rcObj.left, m_rcObj.top, m_bpm);
        }
    } else {
        pImage->xScaleBlt(canvas, m_rcObj.left, m_rcObj.top,
            m_rcObj.width(), m_rcObj.height(),
            m_xExtendStart, m_xExtendEnd, m_bTile, m_bpm);
    }
}

void CSkinNStatusButton::fadeInDrawBt(CRawGraph *canvas, CSFImage *pImage, int nAlpha) {
    assert(m_pLastImage && m_bFadein);

    int nAlphaOld = canvas->getOpacityPainting();

    canvas->setOpacityPainting((255 - nAlpha) * nAlphaOld / 255);

    if (m_xExtendStart == -1) {
        if (m_imgMask.isValid()) {
            m_pLastImage->maskBlt(canvas, m_rcObj.left, m_rcObj.top, &m_imgMask, m_bpm);
        } else {
            m_pLastImage->blt(canvas, m_rcObj.left, m_rcObj.top, m_bpm);
        }
    } else {
        m_pLastImage->xScaleBlt(canvas, m_rcObj.left, m_rcObj.top,
            m_rcObj.width(), m_rcObj.height(),
            m_xExtendStart, m_xExtendEnd, m_bTile, m_bpm);
    }

    canvas->setOpacityPainting(nAlpha * nAlphaOld / 255);

    if (m_xExtendStart == -1) {
        if (m_imgMask.isValid()) {
            pImage->maskBlt(canvas, m_rcObj.left, m_rcObj.top, &m_imgMask, m_bpm);
        } else {
            pImage->blt(canvas, m_rcObj.left, m_rcObj.top, m_bpm);
        }
    } else {
        pImage->xScaleBlt(canvas, m_rcObj.left, m_rcObj.top,
            m_rcObj.width(), m_rcObj.height(),
            m_xExtendStart, m_xExtendEnd, m_bTile, m_bpm);
    }

    canvas->setOpacityPainting(nAlphaOld);
}


bool CSkinNStatusButton::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (strcasecmp(szProperty, "CurStatus") == 0) {
        m_nCurStatus = atoi(szValue);
    } else if (szProperty[0] == 'S' && isDigit(szProperty[1]) && szProperty[2] == '_') {
        cstr_t szPropertyNew = szProperty + 3;
        int n = szProperty[1] - '0';
        if (n >= (int)m_vBtStatImg.size()) {
            setMaxStat(n + 1);
        }
        BtStatImg *btimg = m_vBtStatImg[n];

        if (strcasecmp(szPropertyNew, SZ_PN_IMAGE) == 0) {
            btimg->strBkFile = szValue;
            btimg->imgBk.loadFromSRM(m_pSkin, szValue);
            if (!btimg->imgBk.isValid()) {
                DBG_LOG1("load image file: %s, FAILED", szValue);
            }
            assert(btimg->imgBk.isValid());
        } else if (isPropertyName(szPropertyNew, SZ_PN_IMAGE_SIZE)) {
            scan2IntX(szValue, btimg->imgBk.m_cx, btimg->imgBk.m_cy);
        } else if (isPropertyName(szPropertyNew, SZ_PN_IMAGE_POS)) {
            scan2IntX(szValue, btimg->imgBk.m_x, btimg->imgBk.m_y);
        } else if (isPropertyName(szPropertyNew, "ImageSelPos")) {
            btimg->imgSel.loadFromSRM(m_pSkin, btimg->strBkFile.c_str());
            btimg->imgSel.m_cx = btimg->imgBk.m_cx;
            btimg->imgSel.m_cy = btimg->imgBk.m_cy;
            scan2IntX(szValue, btimg->imgSel.m_x, btimg->imgSel.m_y);
        } else if (isPropertyName(szPropertyNew, "ImageFocusPos")) {
            btimg->imgHover.loadFromSRM(m_pSkin, btimg->strBkFile.c_str());
            btimg->imgHover.m_cx = btimg->imgBk.m_cx;
            btimg->imgHover.m_cy = btimg->imgBk.m_cy;
            scan2IntX(szValue, btimg->imgHover.m_x, btimg->imgHover.m_y);
            m_bEnableHover = true;
        } else if (isPropertyName(szPropertyNew, "ImageDisabledPos")) {
            btimg->imgDisabled.loadFromSRM(m_pSkin, btimg->strBkFile.c_str());
            btimg->imgDisabled.m_cx = btimg->imgBk.m_cx;
            btimg->imgDisabled.m_cy = btimg->imgBk.m_cy;
            scan2IntX(szValue, btimg->imgDisabled.m_x, btimg->imgDisabled.m_y);
        } else if (isPropertyName(szPropertyNew, "IDCmd")) {
            btimg->nIDCmd = m_pSkin->getSkinFactory()->getIDByName(szValue);
        }

        // Old property styles, decrepted
        else if (strcasecmp(szPropertyNew, SZ_PN_IMAGERECT) == 0) {
            if (!getRectValue(szValue, btimg->imgBk)) {
                goto PARSE_VALUE_FAILED;
            }
        } else if (strcasecmp(szPropertyNew, "ImageSel") == 0) {
            btimg->strSelFile = szValue;
            btimg->imgSel.loadFromSRM(m_pSkin, szValue);
        } else if (strcasecmp(szPropertyNew, "ImageSelRect") == 0) {
            if (!getRectValue(szValue, btimg->imgSel)) {
                goto PARSE_VALUE_FAILED;
            }
        } else if (strcasecmp(szPropertyNew, "ImageFocus") == 0) {
            btimg->strHoverFile = szValue;
            btimg->imgHover.loadFromSRM(m_pSkin, szValue);
            if (n == m_nCurStatus) {
                m_bEnableHover = true;
            }
        } else if (strcasecmp(szPropertyNew, "ImageFocusRect") == 0) {
            if (!getRectValue(szValue, btimg->imgHover)) {
                goto PARSE_VALUE_FAILED;
            }
        } else if (strcasecmp(szPropertyNew, "ImageDisabled") == 0) {
            btimg->strDisabledFile = szValue;
            btimg->imgDisabled.loadFromSRM(m_pSkin, szValue);
        } else if (strcasecmp(szPropertyNew, "ImageDisabledRect") == 0) {
            if (!getRectValue(szValue, btimg->imgDisabled)) {
                goto PARSE_VALUE_FAILED;
            }
        } else if (strcasecmp(szProperty, "IDCmd") == 0) {
            if (m_vBtStatImg.size() == 0) {
                m_vBtStatImg.push_back(new BtStatImg);
            }
            m_vBtStatImg[0]->nIDCmd = m_pSkin->getSkinFactory()->getIDByName(szValue);
        } else {
            return false;
        }
    } else if (strcasecmp(szProperty, "Continuous") == 0) {
        m_bContinuousCmd = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "ExtendPos")) {
        scan2IntX(szValue, m_xExtendStart, m_xExtendEnd);
    } else if (isPropertyName(szProperty, "Tile")) {
        m_bTile = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "BlendPixMode")) {
        m_bpm = blendPixModeFromStr(szValue);
    } else if (isPropertyName(szProperty, "Fadein")) {
        m_bFadein = isTRUE(szValue);
    } else {
        return false;
    }

    return true;

PARSE_VALUE_FAILED:
    ERR_LOG2("Analyse Value: %s = %s FAILED.", szProperty, szValue);
    return true;
}

#ifdef _SKIN_EDITOR_
void CSkinNStatusButton::enumProperties(CUIObjProperties &listProperties) {
    CUIObject::enumProperties(listProperties);

    listProperties.addPropInt("CurStatus", m_nCurStatus);

    for (int i = 0; i < (int)m_vBtStatImg.size(); i++) {
        BtStatImg *btimg = m_vBtStatImg[i];
        string name, strNameRect;
        string strPrefix = "S0_";
        strPrefix[1] = '0' + i;

        name = strPrefix + "Image";
        strNameRect = strPrefix + "ImageRect";
        listProperties.addPropImage(name.c_str(), strNameRect.c_str(), btimg->strBkFile.c_str(), btimg->imgBk);

        name = strPrefix + "ImageSel";
        strNameRect = strPrefix + "ImageSelRect";
        listProperties.addPropImage(name.c_str(), strNameRect.c_str(), btimg->strSelFile.c_str(), btimg->imgSel, !btimg->strSelFile.empty());

        name = strPrefix + "ImageFocus";
        strNameRect = strPrefix + "ImageFocusRect";
        listProperties.addPropImage(name.c_str(), strNameRect.c_str(), btimg->strHoverFile.c_str(), btimg->imgHover, !btimg->strHoverFile.empty());

        name = strPrefix + "ImageDisabled";
        strNameRect = strPrefix + "ImageDisabledRect";
        listProperties.addPropImage(name.c_str(), strNameRect.c_str(), btimg->strDisabledFile.c_str(), btimg->imgDisabled, !btimg->strDisabledFile.empty());

        listProperties.addPropID("IDCmd", m_pSkin->getSkinFactory()->getStringOfID(btimg->nIDCmd).c_str(), btimg->nIDCmd != ID_INVALID);
    }

    listProperties.addPropBoolStr("Continuous", m_bContinuousCmd, m_bContinuousCmd);

    listProperties.addPropBlendPixMode("BlendPixMode", m_bpm, m_bpm != BPM_BLEND);
}
#endif // _SKIN_EDITOR_

int CSkinNStatusButton::getStatus() {
    return m_nCurStatus;
}

void CSkinNStatusButton::setStatus(int nStatus) {
    if (nStatus == m_nCurStatus) {
        return;
    }
    if (nStatus >= 0 && nStatus < (int)m_vBtStatImg.size()) {
        m_nCurStatus = nStatus;
        invalidate();
    }
}

void CSkinNStatusButton::setMaxStat(int nMaxStat) {
    if ((int)m_vBtStatImg.size() > nMaxStat) {
        // remove extra stat
        for (int i = (int)m_vBtStatImg.size() - 1; i >= nMaxStat; i--) {
            delete m_vBtStatImg.back();
            m_vBtStatImg.pop_back();
        }
    } else if ((int)m_vBtStatImg.size() < nMaxStat) {
        // add extra stat
        for (int i = (int)m_vBtStatImg.size(); i < nMaxStat; i++) {
            m_vBtStatImg.push_back(new BtStatImg);
        }
    }
}

void CSkinNStatusButton::buttonDownAction() {
    changeButtonState(BS_PRESSED);

    if (m_bContinuousCmd) {
        m_bContinuousBegin = true;
        m_nTimerIdContinuous = m_pSkin->registerTimerObject(this, 500);
    }
}

void CSkinNStatusButton::buttonUpAction() {
    if (m_bContinuousCmd) {
        m_pSkin->unregisterTimerObject(this, m_nTimerIdContinuous);
        m_nTimerIdContinuous = 0;
    }

    if (m_nCurStatus >= 0 && m_nCurStatus < (int)m_vBtStatImg.size() - 1) {
        m_nCurStatus++;
    } else {
        m_nCurStatus = 0;
    }

    if (m_nCurStatus < (int)m_vBtStatImg.size()) {
        m_bEnableHover = m_vBtStatImg[m_nCurStatus]->imgHover.isValid();
    }

    changeButtonState(BS_HOVER);

    if (m_id != ID_INVALID) {
        m_pSkin->postCustomCommandMsg(m_id);
    }

    if (m_vBtStatImg[m_nCurStatus]->nIDCmd != ID_INVALID) {
        m_pSkin->postCustomCommandMsg(m_vBtStatImg[m_nCurStatus]->nIDCmd);
    }
}

void CSkinNStatusButton::changeButtonState(ButtonState state) {
    if (!m_enable) {
        return;
    }

    if (m_btnState == state) {
        invalidate(); // m_nCurStatus 可能改变了
        return;
    }

    assert(m_nCurStatus >= 0 && m_nCurStatus <= (int)m_vBtStatImg.size());
    if (m_nTimerIdFadein != 0) {
        m_pSkin->unregisterTimerObject(this, m_nTimerIdFadein);
    }
    m_nTimerIdFadein = m_pSkin->registerTimerObject(this, 50);
    m_timeBeginFadein = getTickCount();

    BtStatImg *btimg = m_vBtStatImg[m_nCurStatus];

    m_pLastImage = nullptr;
    if (m_btnState == BS_PRESSED && btimg->imgSel.isValid()) {
        m_pLastImage = &btimg->imgSel;
    } else if (m_btnState == BS_HOVER && btimg->imgHover.isValid()) {
        m_pLastImage = &btimg->imgHover;
    } else if (btimg->imgBk.isValid()) {
        m_pLastImage = &btimg->imgBk;
    }

    m_btnState = state;
}

void CSkinNStatusButton::destroy() {
    for (int i = 0; i < (int)m_vBtStatImg.size(); i++) {
        delete m_vBtStatImg[i];
    }
    m_vBtStatImg.clear();
}
