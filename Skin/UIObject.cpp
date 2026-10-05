/********************************************************************
    Created  :    2001/12/15 0:28:39
    FileName :    UIObject.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include "SkinTypes.h"
#include "Skin.h"
#include "UIObject.h"
#include "api-js/JsUIObject.hpp"
#include "../Utils/Utils.h"


IdToString    __idszLayoutParams[] = {
    { LAYOUT_WIDTH_MATCH_PARENT, "match_parent_width" },
    { LAYOUT_WIDTH_WRAP_CONENT, "wrap_content_width" },
    { LAYOUT_HEIGHT_MATCH_PARENT, "match_parent_height" },
    { LAYOUT_HEIGHT_WRAP_CONENT, "wrap_content_height" },
    { LAYOUT_ALIN_HORZ_CENTER, "align_horz_center" },
    { LAYOUT_ALIN_VERT_CENTER, "align_vert_center" },
    { LAYOUT_ALIN_BOTTOM, "align_bottom" },
    { LAYOUT_ALIN_RIGHT, "align_right" },
    { 0, nullptr }
};

IdToString    __idszBltMode[] = {
    { BLT_STRETCH, "stretch" },
    { BLT_TILE, "tile" },
    { BLT_COPY, "copy" },
    { 0, nullptr }
};

BLT_MODE bltModeFromStr(cstr_t szBltMode) {
    return (BLT_MODE)stringToID(__idszBltMode, szBltMode, BLT_COPY);
}

IdToString    __idszBlendPixMode[] = {
    { BPM_BLEND, "alpha_blend" },
    { BPM_MULTIPLY, "alpha_mask" },
    { BPM_COPY, "copy" },
    { 0, nullptr }
};

BlendPixMode blendPixModeFromStr(cstr_t szBpm) {
    return (BlendPixMode)stringToID(__idszBlendPixMode, szBpm, BPM_COPY);
}

IdToString __AlignText[] = {
    // DEFINE_IDT(AT_LEFT),        // left and top is 0.
    DEFINE_IDT(AT_CENTER),
    DEFINE_IDT(AT_RIGHT),
    // DEFINE_IDT(AT_TOP),
    DEFINE_IDT(AT_VCENTER),
    DEFINE_IDT(AT_BOTTOM),
    { 0, nullptr },
};

uint32_t alignTextFromStr(cstr_t szAlignText) {
    return getCombinationValue(__AlignText, szAlignText);
}

void alignTextToStr(uint32_t dwAlignTextFlags, string &str) {
    getCombinationStrValue(__AlignText, dwAlignTextFlags, str);
}

void CUIObjProperty::setValue(int value) {
    strValue = itos(value);
}

void CUIObjProperty::toUIObjProperties(vector<string> &properties) {
    properties.push_back(name);
    properties.push_back(strValue);
    if (valueType == VT_IMAGE) {
        properties.insert(properties.end(), options.begin(), options.end());
    }
}

void CUIObjProperties::addPropStr(cstr_t szName, cstr_t szValue, bool bToXmlAttrib) {
    assert(!isPropExist(szName));

    CUIObjProperty property;

    property.name = szName;
    property.strValue = szValue;
    property.valueType = CUIObjProperty::VT_STR;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}


void CUIObjProperties::addPropID(cstr_t szName, cstr_t szValue, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = szValue;
    property.valueType = CUIObjProperty::VT_ID;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropInt(cstr_t szName, int value, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = stringFromInt(value);
    property.valueType = CUIObjProperty::VT_INT;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropVar(cstr_t szName, CFormula &formular, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = formular.getFormula();
    property.valueType = CUIObjProperty::VT_VAR_INT;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropBoolStr(cstr_t szName, bool bValue, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = BOOLTOSTR(bValue);
    property.valueType = CUIObjProperty::VT_BOOL_STR;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropImage(cstr_t szName, cstr_t szRectName, cstr_t szImage, CSFImage &image, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.options.push_back(szRectName);
    property.options.push_back(stringPrintf("%d,%d,%d,%d",
        image.x(), image.y(), image.width(), image.height()));

    property.name = szName;
    property.strValue = szImage;
    property.valueType = CUIObjProperty::VT_IMAGE;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropImageEx(cstr_t szName, cstr_t szRectName, cstr_t szImage, CSFImage &image, vector<string> &vExtraProperies, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.options.push_back(szRectName);
    property.options.push_back(stringPrintf("%d,%d,%d,%d",
        image.x(), image.y(), image.width(), image.height()));
    property.options.insert(property.options.end(), vExtraProperies.begin(), vExtraProperies.end());

    property.name = szName;
    property.strValue = szImage;
    property.valueType = CUIObjProperty::VT_IMAGE;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropImageFile(cstr_t szName, cstr_t szImageFile, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = szImageFile;
    property.valueType = CUIObjProperty::VT_IMG_FILE;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropFile(cstr_t szName, cstr_t szFile, cstr_t szExtFilter, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = szFile;
    property.options.push_back(szExtFilter);
    property.valueType = CUIObjProperty::VT_FILE;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropColor(cstr_t szName, CColor &clrValue, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = colorToStr(clrValue);
    property.valueType = CUIObjProperty::VT_COLOR;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropFontName(cstr_t szName, cstr_t szFont, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = szFont;
    property.valueType = CUIObjProperty::VT_FONT_NAME;
    property.bToXmlAttrib = bToXmlAttrib;

    push_back(property);
}

void CUIObjProperties::addPropBltMode(cstr_t szName, BLT_MODE bltMode, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = idToString(__idszBltMode, bltMode, __idszBltMode[0].szId);
    property.valueType = CUIObjProperty::VT_COMB_STR;
    property.bToXmlAttrib = bToXmlAttrib;
    property.options.clear();
    for (int i = 0; __idszBltMode[i].szId != nullptr; i++) {
        property.options.push_back(__idszBltMode[i].szId);
    }

    push_back(property);
}

void CUIObjProperties::addPropBlendPixMode(cstr_t szName, BlendPixMode bpm, bool bToXmlAttrib) {
    assert(!isPropExist(szName));
    CUIObjProperty property;

    property.name = szName;
    property.strValue = idToString(__idszBlendPixMode, bpm, __idszBlendPixMode[0].szId);
    property.valueType = CUIObjProperty::VT_COMB_STR;
    property.bToXmlAttrib = bToXmlAttrib;
    property.options.clear();
    for (int i = 0; __idszBlendPixMode[i].szId != nullptr; i++) {
        property.options.push_back(__idszBlendPixMode[i].szId);
    }

    push_back(property);
}

void CUIObjProperties::delProp(cstr_t szName) {
    iterator it, itEnd;
    itEnd = end();
    for (it = begin(); it != itEnd; ++it) {
        CUIObjProperty &property = *it;

        if (strcasecmp(szName, property.name.c_str()) == 0) {
            erase(it);
            return;
        }
    }
}

void CUIObjProperties::toXMLCategoryAttrib(CXMLWriter &xmlStream) {
    iterator it, itEnd;
    itEnd = end();
    for (it = begin(); it != itEnd; ++it) {
        CUIObjProperty &property = *it;

        if (!property.bToXmlAttrib) {
            continue;
        }

        xmlStream.writeAttribute(property.name.c_str(), property.strValue.c_str());

        if (property.valueType == CUIObjProperty::VT_IMAGE) {
            for (int i = 0; i < (int)property.options.size(); i += 2) {
                xmlStream.writeAttribute(property.options[i].c_str(), property.options[i + 1].c_str());
            }
        }
    }
}

void CUIObjProperties::toUIObjProperties(vector<string> &properties) {
    iterator it, itEnd;
    itEnd = end();
    for (it = begin(); it != itEnd; ++it) {
        (*it).toUIObjProperties(properties);
    }
}

bool CUIObjProperties::isPropExist(cstr_t szName) {
    iterator it, itEnd;
    itEnd = end();
    for (it = begin(); it != itEnd; ++it) {
        CUIObjProperty &prop = *it;
        if (strcasecmp(szName, prop.name.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

void readMarginValue(cstr_t szValue, CRect &rc) {
    VecStrings vRect;

    StringView(szValue).split(',', vRect);
    for (auto &s : vRect) {
        trimStr(s);
    }

    if (vRect.size() == 1) {
        rc.left = rc.top = rc.right = rc.bottom = atoi(vRect[0].c_str());
    } else if (vRect.size() == 2) {
        rc.left = rc.right = atoi(vRect[0].c_str());
        rc.top = rc.bottom = atoi(vRect[1].c_str());
    } else if (vRect.size() == 4) {
        rc.left = atoi(vRect[0].c_str());
        rc.top = atoi(vRect[1].c_str());
        rc.right = atoi(vRect[2].c_str());
        rc.bottom = atoi(vRect[3].c_str());
    } else {
        DBG_LOG1("Maring/Padding value isn't correct: %s.", szValue);
    }
}

bool isTRUE(cstr_t szValue) {
    return (strcasecmp("true", szValue) == 0 || strcasecmp("1", szValue) == 0);
}

//////////////////////////////////////////////////////////////////////

CSXNodeProperty::CSXNodeProperty(SXNode *pNode1, SXNode *pNode2) {
    m_pNode1 = pNode1;
    m_pNode2 = pNode2;
}

// If not exists, nullptr will be returned
cstr_t CSXNodeProperty::getProperty(cstr_t szPropName) {
    cstr_t szValue = m_pNode1->getProperty(szPropName);
    if (szValue != nullptr) {
        return szValue;
    }

    if (m_pNode2 == nullptr) {
        return nullptr;
    }

    return m_pNode2->getProperty(szPropName);
}

// If not exists, "" will be returned.
cstr_t CSXNodeProperty::getPropertySafe(cstr_t szPropName) {
    cstr_t szValue = getProperty(szPropName);
    if (szValue == nullptr) {
        szValue = "";
    }
    return szValue;
}

int CSXNodeProperty::getPropertyInt(cstr_t szPropName) {
    return getPropertyInt(szPropName, 0);
}

int CSXNodeProperty::getPropertyInt(cstr_t szPropName, int nValueIfInexist) {
    cstr_t szValue = getProperty(szPropName);
    if (szValue != nullptr) {
        return atoi(szValue);
    }
    return nValueIfInexist;
}


cstr_t CUIObject::ms_szClassName = "UIObject";

CUIObject::CUIObject() {
    m_pSkin = nullptr;
    m_pContainer = nullptr;

    m_id = ID_INVALID;
    m_autoUniID = 0;

    m_msgNeed = 0;

    m_layoutParams = 0;
    m_weight = 0;
    m_minWidth = m_minHeight = 0;
    m_fixedWidth = m_fixedHeight = false;

    m_rcObj.setLTRB(0, 0, 50, 30);
    m_rcContent.setEmpty();
    m_rcMargin.setEmpty();
    m_rcPadding.setEmpty();

    m_bTempTooltip = false;

    m_enable = true;
    m_visible = true;
    m_hideIfWndInactive = false;
    m_bHideIfMouseInactive = false;
    m_isMouseIn = false;

    m_translucencyWithSkin = false;
    m_opacity = 255;

    m_bCreated = false;
    m_bInitialUpdated = false;

    m_animateType = AT_UNKNOWN;
    m_animateDuration = 0;

    m_bgType = BG_NONE;
    m_bgBpm = BPM_BLEND;
}

CUIObject::~CUIObject() {
    if (m_pSkin) {
        if (m_pSkin->getCaptureMouse() == this) {
            m_pSkin->releaseCaptureMouse(this);
        }

        if (m_strTooltip.size() > 0 && m_id != ID_INVALID) {
            m_pSkin->delToolTip(m_autoUniID);
        }

        m_pSkin->unregisterTimerObject(this);
    }
}

JsValue CUIObject::getJsObject(VMContext *ctx) {
    return ctx->runtime->pushObject(new JsUIObject(this));
}

CColor CUIObject::getBgColor() const {
    if (m_bgType == BG_COLOR) {
        return m_pSkin->getTranslucencyColor(m_clrBg);
    }

    if (m_pContainer) {
        return m_pContainer->getBgColor();
    }

    return m_clrBg;
}

bool CUIObject::isDrawBgImage() const {
    // draw with background color?
    if (m_bgType == BG_IMAGE) {
        return true;
    } else if (m_bgType == BG_NONE && m_pContainer) {
        return m_pContainer->isDrawBgImage();
    }

    return false;
}

void CUIObject::redrawBackground(CRawGraph *canvas, const CRect &rc) {
    assert(canvas);
    if (!canvas) {
        return;
    }

    if (m_bgType == BG_NONE) {
        if (m_pContainer) {
            m_pContainer->redrawBackground(canvas, rc);
        }
        return;
    }

    CColor clrBg = m_pSkin->getTranslucencyColor(m_clrBg);

    CRect rcClip, rcNewClip;
    canvas->getClipBoundBox(rcClip);
    rcNewClip.intersect(rcClip, rc);

    if (m_pSkin->getCurrentTranslucency() == 0) {
        // Optimize for totally transparent background
        CColor clr(RGB(0, 0, 0));
        clr.setAlpha(0);
        canvas->fillRect(rcNewClip, clr);
        return;
    }

    CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
    canvas->setClipBoundBox(rcNewClip);

    if (m_imageBg.isValid()) {
        // draw Background with image
        CDrawImageFunMask funcDraw(canvas, &m_imageBg, nullptr, m_bgBpm);

        if (m_imageBgMask.isValid()) {
            funcDraw.m_imageMask = &m_imageBgMask;
        }

        // Fill background
        if (m_imageBgMask.isValid()) {
            canvas->fillRect(rcNewClip, clrBg);
        } else if (m_pSkin->getCurrentTranslucency() != 255) {
            //             CColor    clr(RGB(0, 0, 0));
            //             clr.setAlpha(0);
            //             canvas->fillRect(rcNewClip, clr);
        }

        m_bgImagePainter.blt(m_rcObj.left, m_rcObj.top, m_rcObj.width(), m_rcObj.height(), funcDraw);

        //        if (m_pSkin->getCurrentTranslucency() != 255)
        //            canvas->multiplyAlpha(m_pSkin->getCurrentTranslucency(), &rcNewClip);
    } else {
        canvas->fillRect(rcNewClip, clrBg);
    }
}

void CUIObject::draw(CRawGraph *canvas) {
    if (m_bgType != BG_NONE) {
        redrawBackground(canvas, m_rcObj);
    }
}

int CUIObject::getOpacity() {
    if (m_translucencyWithSkin) {
        return m_opacity * m_pSkin->m_nCurTranslucencyAlpha / 255;
    } else {
        return m_opacity;
    }
}

void CUIObject::onSize() {
    // set content area
    m_rcContent.left = m_rcObj.left + m_rcPadding.left;
    m_rcContent.top = m_rcObj.top + m_rcPadding.top;
    m_rcContent.right = m_rcObj.right - m_rcPadding.right;
    m_rcContent.bottom = m_rcObj.bottom - m_rcPadding.bottom;

    if (!m_strTooltip.empty() && m_visible) {
        m_pSkin->delToolTip(m_autoUniID);
        m_pSkin->addToolTip(_TL(m_strTooltip.c_str()), m_rcObj, m_autoUniID);
    }
}

uint32_t CUIObject::getMinHeight() const {
    return m_minHeight;
}

uint32_t CUIObject::getMinWidth() const {
    return m_minWidth;
}

void CUIObject::onMeasureSizePos(FORMULA_VAR vars[]) {
    int value;

    if (m_formLeft.calCualteValue(vars, value)) {
        m_rcObj.left = value;
    }

    if (m_formTop.calCualteValue(vars, value)) {
        m_rcObj.top = value;
    }

    if (m_pContainer) {
        m_rcObj.left += m_pContainer->m_rcObj.left;
        m_rcObj.top += m_pContainer->m_rcObj.top;
    }

    if (m_formWidth.calCualteValue(vars, value) && value > 0) {
        m_rcObj.right = m_rcObj.left + value;
    } else {
        m_rcObj.right = m_rcObj.left + 0;
    }

    if (m_formHeight.calCualteValue(vars, value) && value > 0) {
        m_rcObj.bottom = m_rcObj.top + value;
    } else {
        m_rcObj.bottom = m_rcObj.top + 0;
    }

    if (m_layoutParams & (LAYOUT_HEIGHT_WRAP_CONENT | LAYOUT_WIDTH_WRAP_CONENT)) {
        onMeasureSizeByContent();
    }

    // Expanding UIObject size by padding area.
    m_rcObj.left -= m_rcPadding.left;
    m_rcObj.top -= m_rcPadding.top;
    m_rcObj.right += m_rcPadding.right;
    m_rcObj.bottom += m_rcPadding.bottom;
}

void CUIObject::setProperties(vector<string> &properties) {
    for (int i = 0; i < (int)properties.size(); i += 2) {
        setProperty(properties[i].c_str(), properties[i + 1].c_str());
    }
}

bool loadBgImageProperty(CSkinWnd *pSkin, CSXNodeProperty *pProperties,
        CSFImage &imageBg, CSFImage &imageBgMask, CScaleImagePainter &painter,
        BlendPixMode *pBpm) {
    imageBg.loadFromSRM(pSkin, pProperties->getPropertySafe(SZ_PN_IMAGE));
    cstr_t szValue = pProperties->getProperty("ImageMask");
    if (szValue) {
        imageBgMask.loadFromSRM(pSkin, szValue);
    }

    szValue = pProperties->getProperty(SZ_PN_IMAGERECT);
    if (szValue) {
        getRectValue(szValue, imageBg);
        if (imageBgMask.isValid()) {
            getRectValue(szValue, imageBgMask);
        }
    }

    szValue = pProperties->getPropertySafe("HorzExtendPos");
    if (!scan2IntX(szValue, painter.srcHorzExtendStart, painter.srcHorzExtendEnd)) {
        painter.srcHorzExtendStart = painter.srcHorzExtendEnd = imageBg.x() + imageBg.width();
    }

    szValue = pProperties->getPropertySafe("VertExtendPos");
    if (!scan2IntX(szValue, painter.srcVertExtendStart, painter.srcVertExtendEnd)) {
        painter.srcVertExtendStart = painter.srcVertExtendEnd = imageBg.y() + imageBg.height();
    }

    if (pBpm) {
        szValue = pProperties->getProperty("BlendPixMode");
        if (szValue) {
            *pBpm = blendPixModeFromStr(szValue);
        }
    }

    painter.srcX = imageBg.x();
    painter.srcY = imageBg.y();
    painter.srcWidth = imageBg.width();
    painter.srcHeight = imageBg.height();
    painter.bDrawCenterArea = !imageBgMask.isValid();

    return imageBg.isValid();
}

bool CUIObject::setProperty(cstr_t szProperty, CSXNodeProperty *pProperties) {
    if (isPropertyName(szProperty, "BgImage")) {
        if (loadBgImageProperty(m_pSkin, pProperties, m_imageBg, m_imageBgMask, m_bgImagePainter, &m_bgBpm)) {
            m_bgType = BG_IMAGE;
        }
        return true;
    }

    return false;
}

bool CUIObject::setProperty(cstr_t szProperty, cstr_t szValue) {
    assert(m_pSkin);

    if (strcasecmp(szProperty, SZ_PN_LEFT) == 0) {
        m_formLeft.setFormula(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_TOP) == 0) {
        m_formTop.setFormula(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_WIDTH) == 0) {
        m_formWidth.setFormula(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_HEIGHT) == 0) {
        m_formHeight.setFormula(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_RECT) == 0) {
        VecStrings vRect;
        strSplit(szValue, ',', vRect);
        trimStr(vRect, ' ');
        if (vRect.size() != 4) {
            assert(0 && "Rect property should have 4 parameters.");
            ERR_LOG1("Rect property should have 4 parameters: %s", szValue);
            return false;
        }

        if (vRect[0].size()) m_formLeft.setFormula(vRect[0].c_str());
        if (vRect[1].size()) m_formTop.setFormula(vRect[1].c_str());
        if (vRect[2].size()) m_formWidth.setFormula(vRect[2].c_str());
        if (vRect[3].size()) m_formHeight.setFormula(vRect[3].c_str());
    } else if (isPropertyName(szProperty, SZ_PN_MARGIN)) {
        readMarginValue(szValue, m_rcMargin);
    } else if (isPropertyName(szProperty, SZ_PN_PADDING)) {
        readMarginValue(szValue, m_rcPadding);
    } else if (isPropertyName(szProperty, SZ_PN_LAYOUT_PARAMS)) {
        m_layoutParams = getCombinationValue(__idszLayoutParams, szValue);
    } else if (isPropertyName(szProperty, SZ_PN_WEIGHT)) {
        m_weight = atoi(szValue);
    } else if (isPropertyName(szProperty, SZ_PN_MIN_WIDTH)) {
        m_minWidth = atoi(szValue);
    } else if (isPropertyName(szProperty, SZ_PN_MIN_HEIGHT)) {
        m_minHeight = atoi(szValue);
    } else if (isPropertyName(szProperty, SZ_PN_FIXED_WIDTH)) {
        m_fixedWidth = isTRUE(szValue);
    } else if (isPropertyName(szProperty, SZ_PN_FIXED_HEIGHT)) {
        m_fixedHeight = isTRUE(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_ID) == 0) {
        if (m_strTooltip.empty()) {
            m_id = m_pSkin->getSkinFactory()->getIDByNameEx(szValue, m_strTooltip);
            if (!m_strTooltip.empty()) {
                m_bTempTooltip = true;
            }
        } else {
            m_id = m_pSkin->getSkinFactory()->getIDByName(szValue);
        }
    } else if (strcasecmp(szProperty, SZ_PN_NAME) == 0) {
        m_strName = szValue;
    } else if (strcasecmp(szProperty, "Text") == 0) {
        setText(szValue);
    } else if (strcasecmp(szProperty, "Visible") == 0) {
        m_visible = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "HideIfWndInactive") == 0) {
        m_hideIfWndInactive = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "HideIfMouseInactive") == 0) {
        m_bHideIfMouseInactive = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "TranslucencyWithSkin") == 0) {
        m_translucencyWithSkin = isTRUE(szValue);
    } else if (strcasecmp(szProperty, SZ_PN_TOOLTIP) == 0) {
        string str = _TL(szValue);
        if (strcmp(m_strTooltip.c_str(), str.c_str()) != 0) {
            m_strTooltip = str.c_str();
            m_bTempTooltip = false;
        }
    }

    // Background color
    else if (isPropertyName(szProperty, "BgColor")) {
        m_bgType = BG_COLOR;
        getColorValue(m_clrBg, szValue);
        m_clrBgOrg = m_clrBg;
    } else if (isPropertyName(szProperty, "BgImage.Image")) {
        m_imageBg.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "ImageMask")) {
        m_strMaskImage = szValue;
        m_imgMask.loadFromSRM(m_pSkin, m_strMaskImage.c_str());
    } else if (isPropertyName(szProperty, "ImageMaskRect")) {
        if (!getRectValue(szValue, m_imgMask)) {
            ERR_LOG2("Analyse Value: %s = %s FAILED.", szProperty, szValue);
        }
    } else if (isPropertyName(szProperty, "TabFocus")) {
        if (isTRUE(szValue)) {
            m_msgNeed |= UO_MSG_WANT_KEY;
        } else {
            m_msgNeed &= ~UO_MSG_WANT_KEY;
        }
    } else if (isPropertyName(szProperty, "animate")) {
        m_animateType = animateTypeFromString(szValue);
    } else if (isPropertyName(szProperty, "AnimateDuration")) {
        m_animateDuration = atoi(szValue);
    } else if (isPropertyName(szProperty, SZ_PN_OS)) {
        // 由 createChild 按当前 OS 过滤，控件自身忽略。
    } else {
        return false;
    }

    return true;
}

bool CUIObject::setPropertyInt(cstr_t szProperty, int value) {
    char szBuffer[64];

    itoa(value, szBuffer);
    return setProperty(szProperty, szBuffer);
}

void CUIObject::invalidate() {
    assert(m_pSkin);

    if (!isVisible() || !isParentVisible()) {
        return;
    }

    if (m_pContainer) {
        m_pContainer->invalidateUIObject(this);
    } else {
        m_pSkin->invalidateUIObject(this);
    }
}

CSkinFactory *CUIObject::getSkinFactory() const {
    if (m_pSkin) {
        return m_pSkin->getSkinFactory();
    } else {
        return nullptr;
    }
}

bool CUIObject::isVisible() {
    return m_visible && (!m_hideIfWndInactive || m_pSkin->isWndActive())
        && (!m_bHideIfMouseInactive || m_pSkin->isMouseActive());
}

void CUIObject::setVisible(bool bVisible, bool bRedraw) {
    if (m_visible == bVisible) {
        return;
    }

    m_visible = bVisible;
    if (m_strTooltip.size()) {
        if (bVisible) {
            m_pSkin->addToolTip(_TL(m_strTooltip.c_str()), m_rcObj, m_autoUniID);
        } else {
            m_pSkin->delToolTip(m_autoUniID);
        }
    }

    CUIObject *pObjFocus = m_pSkin->getFocusUIObj();
    if (bVisible) {
        if (pObjFocus == nullptr) {
            // try to set focus to one UIObject
            m_pSkin->switchFocusUIObj();
        }
    } else {
        // Hidden this, Will focus UIObject be hide by this call?
        m_pSkin->onUIObjectHidden(this);
    }

    m_pContainer->onSetChildVisible(this, bVisible, bRedraw);

    if (bRedraw) {
        if (bVisible) {
            invalidate();
        } else if (m_pContainer) {
            m_pContainer->invalidate();
        }
    }
}

void CUIObject::setVisibleEx(bool bVisible, bool bAnimation, AnimateDirection animateDirection) {
    if (!bAnimation) {
        setVisible(bVisible);
    } else {
        bool bHide = !bVisible;
        // stop the old animation
        m_pSkin->stopAnimation(this);

        // Recalculate the size position, old Animation may mess it up.
        m_pContainer->recalculateUIObjSizePos(this);

        // start new animation
        m_pSkin->startAnimation(this, newAnimate(getAnimationType(), bHide, this, animateDirection), getAnimationDuration());
    }
}

bool CUIObject::isParentVisible() {
    CSkinContainer *pContainer = m_pContainer;

    while (pContainer) {
        if (!pContainer->isVisible()) {
            return false;
        }

        pContainer = pContainer->getParent();
    }

    return true;
}

void CUIObject::setText(cstr_t szText) {
    m_strTextEnglish = szText;
    m_strText = _TL(m_strTextEnglish.c_str());
}

void CUIObject::setFocus() {
    assert(needMsgKey() && isVisible() && isParentVisible());

    m_pSkin->setFocusUIObj(this);
}

bool CUIObject::isOnFocus() {
    return m_pSkin->getFocusUIObj() == this;
}

void CUIObject::setCursor(Cursor &cursor) {
    m_pSkin->setCursor(&cursor);
}

bool CUIObject::isPtIn(CPoint pt) {
    if (!m_rcObj.ptInRect(pt)) {
        return false;
    }

    if (m_imgMask.isValid()) {
        pt.x -= m_rcObj.left;
        pt.y -= m_rcObj.top;
        if (m_imgMask.isPixelTransparent(pt)) {
            // pt 所在的点是透明的.
            return false;
        }
    }

    return true;
}

#ifdef _SKIN_EDITOR_
void CUIObject::enumProperties(CUIObjProperties &listProperties) {
    listProperties.addPropStr(SZ_PN_NAME, m_strName.c_str(), !m_strName.empty());

    listProperties.addPropID(SZ_PN_ID, m_pSkin->getSkinFactory()->getStringOfID(m_id).c_str(), m_id != ID_INVALID);

    listProperties.addPropVar(SZ_PN_LEFT, m_formLeft);
    listProperties.addPropVar(SZ_PN_TOP, m_formTop);
    listProperties.addPropVar(SZ_PN_WIDTH, m_formWidth);
    listProperties.addPropVar(SZ_PN_HEIGHT, m_formHeight);

    listProperties.addPropBoolStr("Visible", m_visible, !m_visible);
    listProperties.addPropBoolStr("HideIfWndInactive", m_hideIfWndInactive, m_hideIfWndInactive);
    listProperties.addPropBoolStr("TranslucencyWithSkin", m_translucencyWithSkin, m_translucencyWithSkin);

    listProperties.addPropStr(SZ_PN_TOOLTIP, m_strTooltip.c_str(), !m_strTooltip.empty() && !m_bTempTooltip);

    listProperties.addPropImage("ImageMask", "ImageMaskRect", m_strMaskImage.c_str(), m_imgMask, !m_strMaskImage.empty());
}
#endif // _SKIN_EDITOR_

int CUIObject::fromXML(SXNode *pXmlNode) {
    for (SXNode::Property &prop : pXmlNode->listProperties) {
        if (!setProperty(prop.name.c_str(), prop.strValue.c_str())) {
#ifdef DEBUG
            if (!isPropertyName(prop.name.c_str(), SZ_PN_EXTENDS) &&
                !isPropertyName(prop.name.c_str(), SZ_PN_OS)) {
                DBG_LOG3("%s - Unknown property: %s=%s",
                    (string(getClassName()) + pXmlNode->getPropertySafe(SZ_PN_ID)).c_str(),
                    prop.name.c_str(), prop.strValue.c_str());
            }
#endif
        }
    }

    // Handle property node.
    for (SXNode *pNode: pXmlNode->listChildren) {
        // Is it a property node?
        if (isPropertyName(pNode->name.c_str(), SZ_PN_PROPERTY)) {
            if (!isSkinXmlNodeForCurrentOs(pNode)) {
                continue;
            }
            cstr_t szPropName = pNode->getPropertySafe(SZ_PN_NAME);
            cstr_t szExtends = pNode->getProperty(SZ_PN_EXTENDS);
            SXNode *pExtends = nullptr;
            if (szExtends != nullptr) {
                pExtends = m_pSkin->getSkinFactory()->getExtendsStyle(szExtends);
            }
            CSXNodeProperty properties(pNode, pExtends);
            if (!isEmptyString(szPropName) && !setProperty(szPropName, &properties)) {
                DBG_LOG2("Unknow Property: %s, name: %s", pNode->name.c_str(), szPropName);
            }
        }
    }

    return ERR_OK;
}

#ifdef _SKIN_EDITOR_
void CUIObject::toXML(CXMLWriter &xmlStream) {
    CUIObjProperties properites;

    enumProperties(properites);

    xmlStream.writeStartElement(getClassName());

    properites.toXMLCategoryAttrib(xmlStream);

    // ?????
    if (hasXMLChild()) {
        onToXMLChild(xmlStream);
    }

    xmlStream.writeEndElement();
}
#endif // _SKIN_EDITOR_

AnimateType CUIObject::getAnimationType() const {
    if (m_animateType == AT_UNKNOWN) {
        return m_pSkin->getAnimationType();
    }
    return m_animateType;
}

int CUIObject::getAnimationDuration() const {
    if (m_animateDuration > 0) {
        return m_animateDuration;
    }
    return m_pSkin->getAnimationDuration();
}

uint32_t CUIObject::getInt(cstr_t szKeyName, int nDefault) {
    return g_profile.getInt(m_pSkin->getSkinFactory()->getSkinName(),
        getProfileKeyName(szKeyName).c_str(), nDefault);
}

string CUIObject::getString(cstr_t szKeyName, cstr_t szDefault) {
    string strValue;

    strValue = g_profile.getString(m_pSkin->getSkinFactory()->getSkinName(),
        getProfileKeyName(szKeyName).c_str(),
        szDefault);

    return strValue;
}

void CUIObject::writeInt(cstr_t szKeyName, int value) {
    g_profile.writeInt(m_pSkin->getSkinFactory()->getSkinName(),
        getProfileKeyName(szKeyName).c_str(),
        value);
}

void CUIObject::writeString(cstr_t szKeyName, cstr_t szValue) {
    g_profile.writeString(m_pSkin->getSkinFactory()->getSkinName(),
        getProfileKeyName(szKeyName).c_str(),
        szValue);
}

string CUIObject::getProfileKeyName(cstr_t szSubKeyName) {
    string strKeyName;

    strKeyName = m_pSkin->getSkinWndName();
    strKeyName += m_pSkin->getSkinFactory()->getStringOfID(m_id);
    strKeyName += szSubKeyName;

    return strKeyName;
}

void CUIObject::onLanguageChanged() {
    if (m_strTextEnglish.size()) {
        m_strText = _TL(m_strTextEnglish.c_str());
    }

    if (m_strTooltip.size()) {
        m_strTooltip = m_pSkin->getSkinFactory()->getTooltip(m_id);
    }
}

int CUIObject::getIDByName(cstr_t szID) {
    assert(m_pSkin);
    return m_pSkin->getSkinFactory()->getIDByName(szID);
}
