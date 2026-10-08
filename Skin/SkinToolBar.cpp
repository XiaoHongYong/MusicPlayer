#include "SkinTypes.h"
#include "Skin.h"
#include "SkinToolBar.h"


#define INVALID_POS        (-1)

#define FADE_BUTTON_TIME_OUT 300



// <Toolbar name="selfedit" image="toolbar.bmp" enablehover="1" height="22"
//     units_x="22" blank_x="638">
//   <buttons ID="ID_NEW" left="1" width="1" CanCheck="0" Checked="0"/>
//     <buttons ID="ID_OPEN" left="3" width="1" CanCheck="0" Checked="0"/>
//     <buttons ID="ID_EDIT_MODE" left="6" width="1" CanCheck="1" Checked="0"
//     checked_left="7" disabled_left="8"/>
//   <seperator left="1" width="1"/>
// </Toolbar>

#define SZ_LEFT_OF_STATUS   "left_of_status_"
#define LEN_LEFT_OF_STATUS    (CountOf(SZ_LEFT_OF_STATUS)   - 1)

UIOBJECT_CLASS_NAME_IMP(CSkinToolbar, "Toolbar")

CSkinToolbar::CSkinToolbar() {
    // UO_MSG_WANT_COMMAND: 容器派发命令时也会送到本控件(onCommand 里同步
    // 自定义按钮的选中态, 供脚本 document.postCommand 恢复状态用)
    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON | UO_MSG_WANT_COMMAND;

    m_nUnitsOfX = 1;

    m_bLBtDown = false;
    m_bHover = false;
    m_bEnableHover = false;

    m_nBlankX = 0;
    m_nBlankCX = 0;

    m_nSeperatorX = -1;
    m_nSeperatorCX = 0;

    m_nBtSpacesCX = 0;

    m_nMarginX = 1;
    m_nMarginY = 1;

    m_nCurButton = -1;

    m_nImageHeight = 0;

    m_bFullStatusImage = false;
    m_bContinuousBegin = false;

    m_bFadein = true;
    m_timeBeginFadein = 0;

    m_clrTextSelected.setAlpha(0);
}

CSkinToolbar::~CSkinToolbar() {
    delTooltip();
}

CSkinToolbar::Button::Button() {
    nID = ID_INVALID;
    nWidth = 1;
    nWidthImage = 1;
    bSeperator = false;
    nGroup = -1;
    nStatusMax = STATUS_NORMAL;
    nCurStatus = STATUS_NORMAL;
    bEnabled = true;
    bTempToolTip = true;
    nLeftOfDiabled = -1; // Image position of disabled
    bContinuousCmd = false;
    nAutoUniID = 0;
}

void CSkinToolbar::Button::fromXML(CSkinToolbar *pToolbar, SXNode *pNode) {
    nAutoUniID = pToolbar->m_pSkin->allocAutoUniID();

    SXNode::iterProperties it, itEnd;

    itEnd = pNode->listProperties.end();
    for (it = pNode->listProperties.begin(); it != itEnd; ++it) {
        SXNode::Property &prop = *it;
        cstr_t szProperty = prop.name.c_str();
        cstr_t szValue = prop.strValue.c_str();

        if (strcasecmp(szProperty, "CanCheck") == 0) {
            nStatusMax = isTRUE(szValue);
        } else if (strcasecmp(szProperty, "Checked") == 0) {
            nCurStatus = isTRUE(szValue);
        } else if (strcasecmp(szProperty, "Enabled") == 0) {
            bEnabled = isTRUE(szValue);
        } else if (isPropertyName(szProperty, "Text")) {
            strTextEnglish = szValue;
            strText = _TL(szValue);
        } else if (isPropertyName(szProperty, "RadioGroup")) {
            nGroup = atoi(szValue);
        } else if (strcasecmp(szProperty, "left") == 0) {
            nLeftPositon[STATUS_NORMAL] = atoi(szValue) * pToolbar->m_nUnitsOfX;
        } else if (strcasecmp(szProperty, "width") == 0) {
            nWidthImage = nWidth = atoi(szValue) * pToolbar->m_nUnitsOfX;
        } else if (strcasecmp(szProperty, "checked_left") == 0) {
            nLeftPositon[STATUS_CHECKED] = atoi(szValue) * pToolbar->m_nUnitsOfX;
            if (nStatusMax < STATUS_CHECKED) {
                nStatusMax = STATUS_CHECKED;
            }
        } else if (strcasecmp(szProperty, "disabled_left") == 0) {
            nLeftOfDiabled = atoi(szValue) * pToolbar->m_nUnitsOfX;
        } else if (strncasecmp(szProperty, SZ_LEFT_OF_STATUS, LEN_LEFT_OF_STATUS) == 0) {
            // "left_of_status_" property
            int n = atoi(szProperty + LEN_LEFT_OF_STATUS);
            if (n > Button::STATUS_MAX) {
                return;
            }
            if (n > nStatusMax) {
                nStatusMax = n;
            }
            nLeftPositon[n] = atoi(szValue) * pToolbar->m_nUnitsOfX;
        } else if (strcasecmp(szProperty, "Continuous") == 0) {
            bContinuousCmd = isTRUE(szValue);
        } else if (strcasecmp(szProperty, SZ_PN_ID) == 0) {
            if (strTooltip.empty()) {
                nID = pToolbar->m_pSkin->getSkinFactory()->getIDByNameEx(szValue, strTooltip);
                if (!strTooltip.empty()) {
                    bTempToolTip = true;
                    strTooltipEnglish = strTooltip;
                }
            } else {
                nID = pToolbar->m_pSkin->getSkinFactory()->getIDByName(szValue);
            }
        } else if (strcasecmp(szProperty, "ToolTip") == 0) {
            strTooltipEnglish = szValue;
            string str = _TL(szValue);
            if (strcmp(strTooltip.c_str(), str.c_str()) != 0) {
                strTooltip = str.c_str();
                bTempToolTip = false;
            }
        }
    }
}

#ifdef _SKIN_EDITOR_
void CSkinToolbar::Button::toXML(CSkinToolbar *pToolbar, CXMLWriter &xml) {
    if (bSeperator) {
        xml.writeStartElement("seperator");
        xml.writeEndElement();
    } else {
        xml.writeStartElement("button");

        if (nID != ID_INVALID) {
            xml.writeAttribute(SZ_PN_ID, pToolbar->m_pSkin->getSkinFactory()->getStringOfID(nID).c_str());
        }

        if (bCanCheck) {
            xml.writeAttribute("CanCheck", bCanCheck);
        }
        if (bCanCheck) {
            xml.writeAttribute("Checked", bChecked);
        }
        if (!bEnabled) {
            xml.writeAttribute("Enabled", bEnabled);
        }

        if (pToolbar->m_nUnitsOfX <= 0) {
            pToolbar->m_nUnitsOfX = 1;
        }
        xml.writeAttribute("left", nLeft / pToolbar->m_nUnitsOfX);
        if (nWidth != pToolbar->m_nUnitsOfX) {
            xml.writeAttribute("width", nWidth / pToolbar->m_nUnitsOfX);
        }
        if (nLeftOfChecked != -1) {
            xml.writeAttribute("checked_left", nLeftOfChecked / pToolbar->m_nUnitsOfX);
        }
        if (nLeftOfDiabled != -1) {
            xml.writeAttribute("disabled_left", nLeftOfDiabled / pToolbar->m_nUnitsOfX);
        }
        if (bContinuousCmd) {
            xml.writeAttribute("Continuous", bContinuousCmd);
        }
        if (!strTooltip.empty() && !bTempToolTip) {
            xml.writeAttribute("ToolTip", strTooltip.c_str());
        }

        xml.writeEndElement();
    }
}
#endif // _SKIN_EDITOR_

void CSkinToolbar::onCreate() {
    CUIObject::onCreate();

    m_font.setParent(m_pSkin);
    if (m_nImageHeight == 0) {
        m_nImageHeight = atoi(m_formHeight.getFormula());
    }

    if (m_clrTextSelected.getAlpha() == 0) {
        m_clrTextSelected = m_font.getTextColor();
    }
}

void CSkinToolbar::onAdjustHue(float hue, float saturation, float luminance) {
    m_font.onAdjustHue(m_pSkin, hue, saturation, luminance);
}

bool CSkinToolbar::onLButtonDown(uint32_t nFlags, CPoint point) {
    m_nCurButton = getCurButton(point);
    if (m_nCurButton == -1) {
        return false;
    }

    if (m_bFadein && m_bFullStatusImage) {
        m_pSkin->registerTimerObject(this, 50);
        m_timeBeginFadein = getTickCount();
        Button &bt = m_vButtons[m_nCurButton];
        m_nLastImageLeft = bt.nLeftPositon[bt.nCurStatus];
        m_nLastImageDrawState = ROW_HOVER;
    } else if (m_vButtons[m_nCurButton].bContinuousCmd) {
        m_bContinuousBegin = true;
        m_pSkin->registerTimerObject(this, 500);
    }

    assert(m_pSkin);

    m_bLBtDown = true;

    if (m_bHover) {
        m_bHover = false;
    } else {
        // 捕捉鼠标输入
        m_pSkin->setCaptureMouse(this);
    }

    invalidate();

    return true;
}

bool CSkinToolbar::onLButtonUp(uint32_t nFlags, CPoint point) {
    m_pSkin->unregisterTimerObject(this);

    if (m_bFadein && m_bFullStatusImage && m_nCurButton != -1) {
        m_pSkin->registerTimerObject(this, 50);
        m_timeBeginFadein = getTickCount();
        Button &bt = m_vButtons[m_nCurButton];
        m_nLastImageLeft = bt.nLeftPositon[bt.nCurStatus];
        m_nLastImageDrawState = ROW_DOWN;
    }

    m_bLBtDown = false;

    // 释放鼠标输入
    m_pSkin->releaseCaptureMouse(this);

    // 转换按钮的状态
    if (m_nCurButton != -1) {
        assert(m_nCurButton >= 0 && m_nCurButton <= (int)m_vButtons.size());
        Button &bt = m_vButtons[m_nCurButton];
        if (!(bt.nGroup != -1 && bt.nCurStatus == Button::STATUS_CHECKED)) {
            if (bt.nCurStatus < bt.nStatusMax) {
                bt.nCurStatus++;
            } else if (bt.nCurStatus > 0) {
                bt.nCurStatus = 0;
            }

            if (bt.nGroup != -1) {
                groupButtonUncheckOld(bt.nGroup, m_nCurButton);
            }

            invalidateButton(m_nCurButton);

            if (bt.nID != ID_INVALID) {
                m_pSkin->postCustomCommandMsg(bt.nID);
            }
        }
    }

    return true;
}

void CSkinToolbar::onSize() {
    // The whole toolbar can't have the tooltip
    if (m_strTooltip.size()) {
        m_strTooltip.resize(0);
    }

    CUIObject::onSize();

    if (m_visible) {
        addTooltip();
    }
}

void CSkinToolbar::onTimer(int nId) {
    if (m_timeBeginFadein != 0) {
        // In fade in, just redraw
        invalidate();
        return;
    }

    if (m_bLBtDown) {
        if (m_nCurButton != -1) {
            if (m_vButtons[m_nCurButton].nID != ID_INVALID) {
                m_pSkin->postCustomCommandMsg(m_vButtons[m_nCurButton].nID);
            }
        }

        if (m_bContinuousBegin) {
            m_pSkin->unregisterTimerObject(this);
            m_pSkin->registerTimerObject(this, 100);
            m_bContinuousBegin = false;
        }
    } else {
        m_pSkin->unregisterTimerObject(this);
    }
}

bool CSkinToolbar::onMouseDrag(CPoint point) {
    int nOldCurButton = m_nCurButton;
    bool bUpdate = false;

    m_nCurButton = getCurButton(point);

    if (isPtIn(point)) {
        if (!m_bLBtDown) {
            m_bLBtDown = true;
            bUpdate = true;

            if (m_bHover) {
                m_bHover = false;
            } else {
                m_pSkin->setCaptureMouse(this);
            }
        }
    } else {
        if (m_bLBtDown) {
            m_bLBtDown = false;
            bUpdate = true;
        }
    }

    if (m_nCurButton != nOldCurButton || bUpdate) {
        invalidate();
    }

    return true;
}

bool CSkinToolbar::onMouseMove(CPoint point) {
    int nOldCurButton = m_nCurButton;
    bool bHoverOld = m_bHover;
    m_nCurButton = getCurButton(point);

    if (isPtIn(point)) {
        if (m_bEnableHover) {
            m_bHover = true;
            m_pSkin->setCaptureMouse(this);
        }
    } else {
        if (m_bHover) {
            m_bHover = false;
            m_pSkin->releaseCaptureMouse(this);
        }
    }

    if (m_nCurButton != nOldCurButton || bHoverOld != m_bHover) {
        invalidate();
    }

    return true;
}

void CSkinToolbar::onLanguageChanged() {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        m_vButtons[i].strText = _TL(m_vButtons[i].strTextEnglish.c_str());
        if (m_vButtons[i].strTooltipEnglish.size()) {
            m_vButtons[i].strTooltip = _TL(m_vButtons[i].strTooltipEnglish.c_str());
        } else if (m_vButtons[i].strTooltip.size()) {
            m_vButtons[i].strTooltip = m_pSkin->getSkinFactory()->getTooltip(m_vButtons[i].nID);
        }
    }
}

void draw3DFrame(CRawGraph *canvas, CRect &rc, CColor &clrHigh, CColor &clrLow) {
    CRawPen penLow, penHigh;

    penLow.createSolidPen(1, clrLow);
    penHigh.createSolidPen(1, clrHigh);

    canvas->setPen(penHigh);
    canvas->line(rc.left, rc.bottom, rc.left, rc.top);
    canvas->line(rc.left, rc.top, rc.right, rc.top);

    canvas->setPen(penLow);
    canvas->line(rc.right, rc.top, rc.right, rc.bottom);
    canvas->line(rc.right, rc.bottom, rc.left, rc.bottom);
}

void drawSeperator(CRawGraph *canvas, CRect &rc, CColor &clrHigh, CColor &clrLow) {
    CRawPen penLow, penHigh;
    int x;

    penLow.createSolidPen(1, clrLow);
    penHigh.createSolidPen(1, clrHigh);

    x = (rc.right + rc.left) / 2;

    canvas->setPen(penLow);
    canvas->line(x, rc.bottom, x, rc.top);

    x++;
    canvas->setPen(penHigh);
    canvas->line(x, rc.top, x, rc.bottom);
}

void CSkinToolbar::draw(CRawGraph *canvas) {
    BT_DRAW_STATE btDrawState = ROW_NORMAL;

    CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
    canvas->setClipBoundBox(m_rcObj);

    int x, y;
    int nHeight = m_rcObj.height();

    if (m_bDrawBtText && m_font.isGood()) {
        canvas->setFont(m_font.getFont());
        canvas->setTextColor(m_font.getTextColor(m_enable));
    }

    if (m_imageBg.isValid()) {
        m_imageBg.blt(canvas, m_rcObj.left, m_rcObj.top, m_nMarginX, m_rcObj.height(), 0, 0);
    }

    if (m_nBlankX != INVALID_POS) {
        m_image.blt(canvas, m_rcObj.left, m_rcObj.top, m_nMarginX, m_nImageHeight, m_nBlankX, 0);
    }

    x = m_rcObj.left + m_nMarginX;
    y = m_rcObj.top;

    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        int nBtImageLeft;

        if (bt.bEnabled || bt.nLeftOfDiabled == -1) {
            assert(bt.nCurStatus <= bt.nStatusMax);
            nBtImageLeft = bt.nLeftPositon[bt.nCurStatus];

            if (i == m_nCurButton) {
                if (m_bLBtDown) {
                    btDrawState = ROW_DOWN;
                } else if (m_bHover) {
                    btDrawState = ROW_HOVER;
                } else {
                    btDrawState = ROW_NORMAL;
                }
            } else {
                btDrawState = ROW_NORMAL;
            }
        } else {
            nBtImageLeft = bt.nLeftOfDiabled;
            btDrawState = ROW_NORMAL;
        }

        if (bt.bSeperator) {
            btDrawState = ROW_NORMAL;
        }

        if (i == m_nCurButton && m_timeBeginFadein != 0) {
            drawCurButtonFadein(canvas, i, btDrawState, x, y, nHeight, nBtImageLeft);
        } else {
            drawButton(canvas, i, btDrawState, x, y, nHeight, nBtImageLeft);
        }

        x += bt.nWidth + m_nBtSpacesCX;
        if (x >= m_rcObj.right) {
            break;
        }

        // draw button spaces?
        if (m_nBtSpacesCX > 0 && m_nBlankX != INVALID_POS) {
            m_image.blt(canvas, x - m_nBtSpacesCX, y, m_nBtSpacesCX, m_nImageHeight, m_nBlankX, 0);
        }
    }

    if (x < m_rcObj.right && m_nBlankX != INVALID_POS) {
        m_image.xTileBlt(canvas, x, y, m_rcObj.right - x, nHeight, m_nBlankX, 0, m_nBlankCX, nHeight);
        if (m_imageBg.isValid()) {
            m_imageBg.xTileBlt(canvas, x, m_rcObj.top, m_rcObj.right - x, m_rcObj.height());
        }
    }
}

void CSkinToolbar::setVisible(bool bVisible, bool bRedraw) {
    CUIObject::setVisible(bVisible, bRedraw);

    if (bVisible) {
        addTooltip();
    } else {
        delTooltip();
    }
}

bool CSkinToolbar::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (isPropertyName(szProperty, "DrawBtText")) {
        m_bDrawBtText = isTRUE(szValue);
    } else if (m_font.setProperty(szProperty, szValue)) {
        return true;
    } else if (isPropertyName(szProperty, "BgImage")) {
        m_strImageBgFile = szValue;
        m_imageBg.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "BgImageChecked")) {
        m_strImageBgCheckedFile = szValue;
        m_imageBgChecked.loadFromSRM(m_pSkin, szValue);
    } else if (strcasecmp(szProperty, SZ_PN_IMAGE) == 0) {
        m_strImageFile = szValue;
        m_image.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "ImageHeight")) {
        m_nImageHeight = atoi(szValue);
    } else if (strcasecmp(szProperty, "units_x") == 0) {
        m_nUnitsOfX = atoi(szValue);
    } else if (strcasecmp(szProperty, "blank_x") == 0) {
        m_nBlankX = atoi(szValue);
    } else if (strcasecmp(szProperty, "blank_cx") == 0) {
        m_nBlankCX = atoi(szValue);
    } else if (strcasecmp(szProperty, "seperator_x") == 0) {
        m_nSeperatorX = atoi(szValue);
    } else if (strcasecmp(szProperty, "seperator_cx") == 0) {
        m_nSeperatorCX = atoi(szValue);
    } else if (strcasecmp(szProperty, "enablehover") == 0) {
        m_bEnableHover = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "MarginX") == 0) {
        m_nMarginX = atoi(szValue);
    } else if (strcasecmp(szProperty, "MarginY") == 0) {
        m_nMarginY = atoi(szValue);
    } else if (isPropertyName(szProperty, "SelTextColor")) {
        m_clrTextSelected = parseColorString(szValue);
    } else if (strcasecmp(szProperty, "ButtonSpacesCX") == 0) {
        m_nBtSpacesCX = atoi(szValue);
    } else if (strcasecmp(szProperty, "FullStatusImage") == 0) {
        m_bFullStatusImage = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "Fadein")) {
        m_bFadein = isTRUE(szValue);
    } else {
        return false;
    }

    return true;
}

#ifdef _SKIN_EDITOR_
void CSkinToolbar::enumProperties(CUIObjProperties &listProperties) {
    CUIObject::enumProperties(listProperties);

    listProperties.addPropImageFile(SZ_PN_IMAGE, m_strImageFile.c_str());

    listProperties.addPropInt( "units_x", m_nUnitsOfX);
    listProperties.addPropInt( "blank_x", m_nBlankX);
    listProperties.addPropInt( "blank_cx", m_nBlankCX);
    listProperties.addPropInt( "seperator_x", m_nSeperatorX);
    listProperties.addPropInt( "seperator_cx", m_nSeperatorCX);
    listProperties.addPropBoolStr("enablehover", m_bEnableHover);
    listProperties.addPropInt( "MarginX", m_nMarginX);
    listProperties.addPropInt( "MarginY", m_nMarginY);
}
#endif // _SKIN_EDITOR_

int CSkinToolbar::fromXML(SXNode *pXmlNode) {
    int nRet;

    nRet = CUIObject::fromXML(pXmlNode);
    if (nRet != ERR_OK) {
        return nRet;
    }

    for (SXNode::iterator it = pXmlNode->listChildren.begin();
    it != pXmlNode->listChildren.end(); ++it)
        {
        SXNode *pChild = *it;
        if (!isSkinXmlNodeForCurrentOs(pChild)) {
            continue;
        }

        if (strcasecmp(pChild->name.c_str(), "button") == 0) {
            Button bt;
            bt.nWidthImage = bt.nWidth = m_nUnitsOfX;
            bt.fromXML(this, pChild);
            m_vButtons.push_back(bt);
        } else if (strcasecmp(pChild->name.c_str(), "seperator") == 0) {
            Button bt;
            bt.bSeperator = true;
            bt.nLeftPositon[0] = bt.nLeftOfDiabled = (m_nSeperatorX == -1 ? m_nBlankX : m_nSeperatorX);
            bt.nWidthImage = bt.nWidth = (m_nSeperatorX == -1 ? m_nBlankCX : m_nSeperatorCX);
            m_vButtons.push_back(bt);
        }
    }

    return ERR_OK;
}

#ifdef _SKIN_EDITOR_
void CSkinToolbar::onToXMLChild(CXMLWriter &xmlStream) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];

        bt.toXML(this, xmlStream);
    }
}
#endif // _SKIN_EDITOR_

void CSkinToolbar::invalidateButton(int nButton) {
    invalidate();
}

int CSkinToolbar::getCurButton(CPoint pt) {
    if (pt.y <= m_rcObj.top || pt.y >= m_rcObj.bottom) {
        return -1;
    }
    if (pt.x <= m_rcObj.left + m_nMarginX || pt.x >= m_rcObj.right) {
        return -1;
    }

    pt.x -= m_rcObj.left + m_nMarginX;
    pt.y -= m_rcObj.top;

    int x = 0;

    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        x += bt.nWidth;
        if (pt.x < x) {
            if (bt.bSeperator) {
                return -1;
            }
            return i;
        }
        x += m_nBtSpacesCX;
        if (pt.x < x) {
            return -1;
        }
    }

    return -1;
}

void CSkinToolbar::groupButtonUncheckOld(int nGroup, int nCurButton) {
    assert(nGroup != -1);
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nGroup == nGroup && i != nCurButton) {
            bt.nCurStatus = 0;
        }
    }
}

bool CSkinToolbar::onCommand(uint32_t nId) {
    // 皮肤脚本可用 document.postCommand(CID_XXX) 同步 RadioGroup 按钮选中态
    // (如启动时恢复上次选中的 tab)。仅限脚本自定义 ID(ID_ID_USER_BASE 起),
    // 且只对 RadioGroup 强制 checked——独立 CanCheck 开关由 onLButtonUp 自行翻转。
    // 内置命令按钮(如 ID_MAXIMIZE)的 check/uncheck 由点击流程自己管理, 不参与。
    // 返回 false: 不吞掉命令, 让其继续派发给 JS oncommand 等其它处理者。
    if (nId >= ID_ID_USER_BASE) {
        for (int i = 0; i < (int)m_vButtons.size(); i++) {
            Button &bt = m_vButtons[i];
            if (bt.nID == nId && bt.nGroup != -1) {
                setCheck(nId, true);
                break;
            }
        }
    }
    return false;
}

bool CSkinToolbar::isCheck(int nCmdID) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            return bt.nCurStatus == Button::STATUS_CHECKED;
        }
    }

    return false;
}

void CSkinToolbar::setCheck(int nCmdID, bool bCheck, bool bRedraw) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            if (bCheck) {
                if (bt.nStatusMax >= Button::STATUS_CHECKED) {
                    bt.nCurStatus = Button::STATUS_CHECKED;
                    if (bt.nGroup != -1) {
                        groupButtonUncheckOld(bt.nGroup, i);
                    }
                    if (bRedraw) {
                        invalidate();
                    }
                }
            } else {
                bt.nCurStatus = Button::STATUS_NORMAL;
            }
            return;
        }
    }
}

void CSkinToolbar::setBtStatus(int nCmdID, int nStatus, bool bRedraw) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            if (nStatus <= bt.nStatusMax) {
                bt.nCurStatus = nStatus;
                if (bRedraw) {
                    invalidate();
                }
            }
            return;
        }
    }
}

int CSkinToolbar::getBtStatus(int nCmdID) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            return bt.nCurStatus;
        }
    }

    return 0;
}

bool CSkinToolbar::isBtEnabled(int nCmdID) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            return bt.bEnabled;
        }
    }

    return false;
}

void CSkinToolbar::enableBt(int nCmdID, bool bEnable) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            bt.bEnabled = bEnable;
            return;
        }
    }
}

bool CSkinToolbar::isButtonExist(int nCmdID) {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.nID == nCmdID) {
            return true;
        }
    }

    return false;
}

void CSkinToolbar::calculateBtWidth(CRawGraph *canvas, Button &bt) {
    if (m_bDrawBtText && bt.strText.size()) {
        CSize size;
        if (canvas->getTextExtentPoint32(bt.strText.c_str(), bt.strText.size(), &size)) {
            const int TEXT_SEPERATOR_WIDTH = 20;
            if (size.cx + TEXT_SEPERATOR_WIDTH > bt.nWidthImage) {
                bt.nWidth = (int16_t)size.cx + TEXT_SEPERATOR_WIDTH;
            } else {
                bt.nWidthImage = bt.nWidth;
            }
        }
    }
}

void CSkinToolbar::drawButton(CRawGraph *canvas, int nButton, BT_DRAW_STATE btDrawState, int x, int y, int nHeight, int nBtImageLeft) {
    Button &bt = m_vButtons[nButton];

    calculateBtWidth(canvas, bt);

    int xDrawImage = x + (bt.nWidth - bt.nWidthImage) / 2;

    CSFImage *pImage = nullptr;
    if (bt.nCurStatus == Button::STATUS_CHECKED) {
        pImage = &m_imageBgChecked;
    } else {
        pImage = &m_imageBg;
    }

    const int BUTTON_BG_BORDER = 5;
    if (pImage->isValid()) {
        pImage->xScaleBlt(canvas, x, y, bt.nWidth, m_rcObj.height(),
            BUTTON_BG_BORDER, pImage->m_cx - BUTTON_BG_BORDER, true);
    }

    // draw Button text
    if (m_bDrawBtText && bt.strText.size()) {
        CRect rcText(x, y + m_nImageHeight, x + bt.nWidth, y + nHeight);
        CColor clrText = btDrawState == ROW_DOWN ? m_clrTextSelected : m_font.getTextColor(m_enable);
        canvas->setTextColor(clrText);
        canvas->drawText(bt.strText.c_str(), bt.strText.size(), rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (m_bFullStatusImage) {
        //
        int nBtImageTop;

        nBtImageTop = m_nImageHeight * btDrawState;

        m_image.blt(canvas, xDrawImage, y, bt.nWidthImage, m_nImageHeight,
            nBtImageLeft, nBtImageTop);

        return;
    }

    CColor clrHigh(RGB(255, 255, 255)), clrLow(RGB(128, 128,128));

    if (btDrawState == ROW_DOWN) {
        m_image.blt(canvas, xDrawImage + 1, y + 1, bt.nWidthImage - 1, m_nImageHeight - 1,
            nBtImageLeft, 0);
    } else {
        m_image.blt(canvas, xDrawImage, y, bt.nWidthImage, m_nImageHeight,
            nBtImageLeft, 0);
    }

    if (btDrawState != ROW_NORMAL) {
        CRect rc;
        rc.left = x;
        rc.top = y + m_nMarginY;
        rc.right = rc.left + bt.nWidth - 1;
        rc.bottom = y + nHeight - m_nMarginY - 1;
        if (btDrawState == ROW_HOVER) {
            draw3DFrame(canvas, rc, clrHigh, clrLow);
        } else if (btDrawState == ROW_DOWN) {
            draw3DFrame(canvas, rc, clrLow, clrHigh);
        }
    }

    if (bt.bSeperator) {
        CRect rc;
        rc.left = x;
        rc.top = y + m_nMarginY;
        rc.right = rc.left + bt.nWidth - 1;
        rc.bottom = y + nHeight - m_nMarginY;
        drawSeperator(canvas, rc, clrHigh, clrLow);
    }
}

void CSkinToolbar::drawCurButtonFadein(CRawGraph *canvas, int nButton, BT_DRAW_STATE btDrawState, int x, int y, int nHeight, int nBtImageLeft) {
    assert(m_bFullStatusImage);

    auto now = getTickCount();

    if (now - m_timeBeginFadein >= FADE_BUTTON_TIME_OUT) {
        m_timeBeginFadein = 0;
        m_pSkin->unregisterTimerObject(this);
        drawButton(canvas, nButton, btDrawState, x, y, nHeight, nBtImageLeft);
        return;
    }
    int nAlpha = (int)(now - m_timeBeginFadein) * 255 / FADE_BUTTON_TIME_OUT;
    int nAlphaOld = canvas->getOpacityPainting();

    canvas->setOpacityPainting((255 - nAlpha) * nAlphaOld / 255);

    drawButton(canvas, nButton, m_nLastImageDrawState, x, y, nHeight, nBtImageLeft);

    canvas->setOpacityPainting(nAlpha * nAlphaOld / 255);

    drawButton(canvas, nButton, btDrawState, x, y, nHeight, nBtImageLeft);

    canvas->setOpacityPainting(nAlphaOld);
}

void CSkinToolbar::addTooltip() {
    CRect rcBt;
    int i;

    // add tooltip of all the buttons.
    rcBt.left = m_rcObj.left + m_nMarginX;
    rcBt.top = m_rcObj.top;
    rcBt.bottom = m_rcObj.bottom;

    for (i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.strTooltip.size()) {
            if (rcBt.left + bt.nWidth < m_rcObj.right) {
                rcBt.right = rcBt.left + bt.nWidth;
            } else {
                rcBt.right = m_rcObj.right;
            }

            m_pSkin->delToolTip(bt.nAutoUniID);
            m_pSkin->addToolTip(_TL(bt.strTooltip.c_str()), rcBt, bt.nAutoUniID);
        }

        rcBt.left += bt.nWidth + m_nBtSpacesCX;
        if (rcBt.left >= m_rcObj.right) {
            break;
        }
    }
}

void CSkinToolbar::delTooltip() {
    for (int i = 0; i < (int)m_vButtons.size(); i++) {
        Button &bt = m_vButtons[i];
        if (bt.strTooltip.size()) {
            m_pSkin->delToolTip(bt.nAutoUniID);
        }
    }
}
