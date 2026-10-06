#pragma once

#include "../Window/WindowLib.h"
#include "SkinTypes.h"
#include "Formula.h"
#include "SkinAnimation.h"
#include "SFImage.h"
#include "interpreter/VirtualMachineTypes.hpp"


class CFormula;
class CSFImage;
class CSkinContainer;
class CUIObject;
class CSkinAnimation;
class SXNode;
class CSkinWnd;
class CSkinFactory;

typedef vector<CUIObject *> VecUIObjects;
typedef list<CUIObject *> ListUIObjects;

enum LayoutParams {
    // For both
    LAYOUT_WIDTH_WRAP_CONENT    = 1 << 0,
    LAYOUT_HEIGHT_WRAP_CONENT   = 1 << 1,

    // Only for vertical linear container
    LAYOUT_WIDTH_MATCH_PARENT   = 1 << 2,
    LAYOUT_ALIN_HORZ_CENTER     = 1 << 3,
    LAYOUT_ALIN_RIGHT           = 1 << 4,

    // Only for horizontal linear container
    LAYOUT_HEIGHT_MATCH_PARENT  = 1 << 5,
    LAYOUT_ALIN_VERT_CENTER     = 1 << 6,
    LAYOUT_ALIN_BOTTOM          = 1 << 7,
};

enum BLT_MODE {
    BLT_COPY,                        // 直接拷贝
    BLT_STRETCH,                     // 进行伸缩复制
    BLT_TILE                         // 平铺复制，自动判断是X方向还是Y方向
};

BLT_MODE bltModeFromStr(cstr_t szBltMode);

BlendPixMode blendPixModeFromStr(cstr_t szBpm);

enum AlginText {
    AT_LEFT                     = DT_LEFT,
    AT_CENTER                   = DT_CENTER,
    AT_RIGHT                    = DT_RIGHT,
    AT_TOP                      = DT_TOP,
    AT_VCENTER                  = DT_VCENTER,
    AT_BOTTOM                   = DT_BOTTOM,
};

uint32_t alignTextFromStr(cstr_t szAlignText);
void alignTextToStr(uint32_t dwAlignTextFlags, string &str);

class CUIObjProperty {
public:
    CUIObjProperty() { bToXmlAttrib = true; valueType = VT_INT; }
    enum VALUE_TYPE {
        VT_INT,                          // 整型数值
        VT_ID,                           // ID of controls
        VT_STR,                          // 字符串值
        VT_FILE,                         // options 是文件扩展名列表
        VT_IMG_FILE,
        VT_IMAGE,                        // Image file(strValue) and rect(options[0, 1])
        VT_VAR_INT,
        VT_RECT,                         // %d, %d, %d, %d 型的值
        VT_BOOL_STR,                     // bool 值
        VT_COMB_STR,                     // options 是可以选择的字符串列表
        VT_COLOR,                        // color
        VT_FONT_NAME,                    // font
    };
    string                      name;
    string                      desc;               // 描述
    string                      strValue;
    VALUE_TYPE                  valueType;
    bool                        bToXmlAttrib;

    vector<string>              options;

    // 子属性
    vector<CUIObjProperty>      childProps;

    void setValue(int value);

    void toUIObjProperties(vector<string> &properties);

};

class CUIObjProperties : public list<CUIObjProperty> {
public:
    void addPropStr(cstr_t szName, cstr_t szValue, bool bToXmlAttrib = true);
    void addPropID(cstr_t szName, cstr_t szValue, bool bToXmlAttrib = true);
    void addPropInt(cstr_t szName, int value, bool bToXmlAttrib = true);
    void addPropVar(cstr_t szName, CFormula &formular, bool bToXmlAttrib = true);
    void addPropBoolStr(cstr_t szName, bool bValue, bool bToXmlAttrib = true);
    void addPropImage(cstr_t szName, cstr_t szRectName, cstr_t szImage, CSFImage &image, bool bToXmlAttrib = true);
    void addPropImageEx(cstr_t szName, cstr_t szRectName, cstr_t szImage, CSFImage &image, vector<string> &vExtraProperies, bool bToXmlAttrib = true);
    void addPropImageFile(cstr_t szName, cstr_t szImageFile, bool bToXmlAttrib = true);
    void addPropFile(cstr_t szName, cstr_t szFile, cstr_t szExtFilter, bool bToXmlAttrib = true);
    void addPropColor(cstr_t szName, CColor &clrValue, bool bToXmlAttrib = true);
    void addPropFontName(cstr_t szName, cstr_t szFont, bool bToXmlAttrib = true);
    void addPropBltMode(cstr_t szName, BLT_MODE bltMode, bool bToXmlAttrib = true);
    void addPropBlendPixMode(cstr_t szName, BlendPixMode bpm, bool bToXmlAttrib = true);

    void delProp(cstr_t szName);

    void toXMLCategoryAttrib(CXMLWriter &xmlStream);

    void toUIObjProperties(vector<string> &properties);

    bool isPropExist(cstr_t szName);

};

enum {
    UO_MSG_WANT_MOUSEMOVE       = 1,
    UO_MSG_WANT_LBUTTON         = 1 << 1,
    UO_MSG_WANT_RBUTTON         = 1 << 2,
    UO_MSG_WANT_KEY             = 1 << 3,
    UO_MSG_WANT_ENTER_KEY       = 1 << 4,
    UO_MSG_WANT_ALL_KEYS        = 1 << 5,
    UO_MSG_WANT_MOUSEWHEEL      = 1 << 6,
    UO_MSG_WANT_MENU_KEY        = 1 << 7,
    UO_MSG_WANT_COMMAND         = 1 << 8,
    UO_MSG_WANT_MAGNIFY         = 1 << 10,
    UO_MSG_WANT_ALL             = 0xFFFFFFFF
};

bool isTRUE(cstr_t szValue);

#define BOOLTOSTR(bValue)   (bValue ? "true" : "false")

#define GET_ID_BY_NAME(_id) _id = getIDByName(#_id)
#define GET_ID_BY_NAME2(_id1, _id2) { _id1 = getIDByName(#_id1); _id2 = getIDByName(#_id2); }
#define GET_ID_BY_NAME3(_id1, _id2, _id3)   { _id1 = getIDByName(#_id1); _id2 = getIDByName(#_id2); _id3 = getIDByName(#_id3); }
#define GET_ID_BY_NAME4(_id1, _id2, _id3, _id4) { _id1 = getIDByName(#_id1); _id2 = getIDByName(#_id2); _id3 = getIDByName(#_id3); _id4 = getIDByName(#_id4); }

#define SZ_TRUE             "true"
#define SZ_FALSE            "false"

// PN = Property Name
#define SZ_PN_NAME          "Name"
#define SZ_PN_TEXT          "Text"
#define SZ_PN_ID            "ID"
#define SZ_PN_LEFT          "Left"
#define SZ_PN_TOP           "Top"
#define SZ_PN_WIDTH         "width"
#define SZ_PN_HEIGHT        "height"
#define SZ_PN_TOOLTIP       "ToolTip"
#define SZ_PN_EXTENDS       "Extends"
#define SZ_PN_OS            "os"

#define SZ_PN_PROPERTY      "Property"

#define SZ_PN_IMAGE         "Image"
#define SZ_PN_IMAGE_SIZE    "ImageSize"
#define SZ_PN_IMAGE_POS     "ImagePos"
#define SZ_PN_IMAGERECT     "ImageRect"

#define SZ_PN_RECT          "Rect"
#define SZ_PN_MARGIN        "Margin"
#define SZ_PN_PADDING       "Padding"
#define SZ_PN_LAYOUT_PARAMS "LayoutParams"
#define SZ_PN_WEIGHT        "Weight"
#define SZ_PN_MIN_WIDTH     "MinWidth"
#define SZ_PN_MIN_HEIGHT    "MinHeight"
#define SZ_PN_FIXED_WIDTH   "fixedWidth"
#define SZ_PN_FIXED_HEIGHT  "fixedHeight"

#define SZ_PN_LINK          "Link"

//
// declare getClassName, isKindOf, className of classes derived from CUIObject
//
#define UIOBJECT_CLASS_NAME_DECLARE(BaseClass)    \
public:\
    virtual cstr_t getClassName() override { return ms_szClassName; }\
    virtual bool isKindOf(cstr_t szClassName) override\
    {\
            if (BaseClass::isKindOf(szClassName))\
                return true;\
            return strcasecmp(szClassName, ms_szClassName) == 0;\
    }\
    static cstr_t className() { return ms_szClassName; }\
protected:\
    static cstr_t        ms_szClassName;\
public:


#define UIOBJECT_CLASS_NAME_IMP(_ClassName, szClassName)    \
    cstr_t _ClassName::ms_szClassName = szClassName;

class CSXNodeProperty {
public:
    CSXNodeProperty(SXNode *pNode1, SXNode *pNode2);

    // If not exists, nullptr will be returned
    cstr_t getProperty(cstr_t szPropName);

    // If not exists, "" will be returned.
    cstr_t getPropertySafe(cstr_t szPropName);

    int getPropertyInt(cstr_t szPropName);
    int getPropertyInt(cstr_t szPropName, int nValueIfInexist);

protected:
    SXNode                      *m_pNode1, *m_pNode2;

};

// 从 <Property Name="BgImage|WindowImage" Image=... HorzExtendPos=... VertExtendPos=... /> 加载 9-slice 图。
// pBpm 非空时解析 BlendPixMode；为空则保持调用方已设定的混合模式。
// 窗口级请用 WindowImage（CSkinWnd 固定 copy）；控件级仍用 BgImage。
bool loadBgImageProperty(CSkinWnd *pSkin, CSXNodeProperty *pProperties,
    CSFImage &imageBg, CSFImage &imageBgMask, CScaleImagePainter &painter,
    BlendPixMode *pBpm = nullptr);

class CUIObject {
public:
    CUIObject();
    virtual ~CUIObject();

    enum BackgroundType {
        BG_NONE,                         // Use the background of parent.
        BG_COLOR,
        BG_IMAGE,
    };

public:
    void setParent(CSkinWnd *pSkinWnd, CSkinContainer *pContainer)
        { m_pSkin = pSkinWnd; m_pContainer = pContainer; }

    virtual JsValue getJsObject(VMContext *ctx);

    // modify CUIObject's property
    virtual bool setProperty(cstr_t szProperty, CSXNodeProperty *pProperties);
    virtual bool setProperty(cstr_t szProperty, cstr_t szValue);
    virtual bool setPropertyInt(cstr_t szProperty, int value);
    virtual void setProperties(vector<string> &properties);

    uint32_t getLayoutParams() const { return m_layoutParams; }
    uint32_t getWeight() const { return m_weight; }
    uint32_t getMinHeight() const;
    uint32_t getMinWidth() const;
    bool isFixedWidth() const { return m_fixedWidth; }
    bool isFixedHeight() const { return m_fixedHeight; }

    void onMeasureSizePos(FORMULA_VAR vars[]);
    virtual void onMeasureSizeByContent() { }

    const CRect &getMargin() const { return m_rcMargin; }
    const CRect &getPadding() const { return m_rcPadding; }

    // when CSkinWnd's size is changed, call every CUIObject's RecalclulatePos to change pos
    virtual void invalidate();

    CSkinContainer *getParent() const { return m_pContainer; }
    CSkinWnd *getSkinWnd() const { return m_pSkin; }
    CSkinFactory *getSkinFactory() const;


    // UIObject messages
    virtual void onInitialUpdate() { }
    virtual void onCreate() { }
    virtual void onDestroy() { }
    virtual bool onKeyUp(uint32_t nChar, uint32_t nFlags) { return false; }
    virtual bool onKeyDown(uint32_t nChar, uint32_t nFlags) { return false; }
    virtual bool onMenuKey(uint32_t nChar, uint32_t nFlags) { return false; }
    virtual void onChar(uint32_t nChar) { }
    virtual bool onRButtonUp(uint32_t nFlags, CPoint point) { return false; }
    virtual bool onRButtonDown(uint32_t nFlags, CPoint point) { return false; }
    virtual bool onLButtonUp(uint32_t nFlags, CPoint point) { return false; }
    virtual bool onLButtonDown(uint32_t nFlags, CPoint point) { return false; }
    virtual bool onLButtonDblClk(uint32_t nFlags, CPoint point) { return false; }
    virtual bool onMouseDrag(CPoint point) { return false; }
    virtual bool onMouseMove(CPoint point) { return false; }
    virtual void onMouseEnter(CPoint point) { }
    virtual void onMouseLeave(CPoint point) { }
    virtual void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) { }
    virtual void onMagnify(float magnification) { }

    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) { }

    // 最终输入的文字
    virtual void onInputText(cstr_t text) { }
    // MarketText 是临时的文字，当输入其他字符时会被替代
    virtual void onInputMarkedText(cstr_t text) { }

    virtual bool onCommand(uint32_t nId) { return false; }

    virtual void onAnimate(CSkinAnimation *pAnimation) { }

    virtual void draw(CRawGraph *canvas);

    int getOpacity();

    virtual void onKillFocus() { }
    virtual void onSetFocus() { }

    virtual void onTimer(int nId) { }
    virtual void onSize();

    virtual void onAdjustHue(float hue, float saturation, float luminance) { }
    virtual void onSkinFontChanged() { }

    inline bool needMsgMouseMove()    { return (m_msgNeed & UO_MSG_WANT_MOUSEMOVE) != 0; }
    inline bool needMsgMouseWheel()    { return (m_msgNeed & UO_MSG_WANT_MOUSEWHEEL) != 0; }
    inline bool needMsgLButton()    { return (m_msgNeed & UO_MSG_WANT_LBUTTON) != 0; }
    inline bool needMsgRButton()    { return (m_msgNeed & UO_MSG_WANT_RBUTTON) != 0; }
    inline bool needMsgKey()    { return (m_msgNeed & (UO_MSG_WANT_KEY | UO_MSG_WANT_ALL_KEYS)) != 0; }
    inline bool needMsgAllKeys()    { return (m_msgNeed & UO_MSG_WANT_ALL_KEYS) != 0; }
    inline bool needMsgEnterKey()    { return (m_msgNeed & UO_MSG_WANT_ENTER_KEY) != 0; }
    inline bool needMsgMenuKey()    { return (m_msgNeed & UO_MSG_WANT_MENU_KEY) != 0; }
    inline bool needMsgCommand()    { return (m_msgNeed & UO_MSG_WANT_COMMAND) != 0; }
    inline bool needMsgMagnify()    { return (m_msgNeed & UO_MSG_WANT_MAGNIFY) != 0; }

    bool isVisible();
    virtual void setVisible(bool bVisible, bool bRedraw = false);
    virtual void setVisibleEx(bool bVisible, bool bAnimation, AnimateDirection animateDirection);
    bool isParentVisible();

    inline bool isEnable() { return m_enable; }
    inline void setEnable(bool bEnable) { m_enable = bEnable; }

    inline bool isUseParentBg() { return m_bgType == BG_NONE; }
    // inline void SetUseParentBg(bool bUseSkinBg) { m_bgType = BG_IMAGE; }

    CColor getBgColor() const;
    bool isDrawBgImage() const;

    void redrawBackground(CRawGraph *canvas, const CRect &rc);

    virtual void setText(cstr_t szText);
    virtual string &getText() { return m_strText; }

    void setFocus();
    bool isOnFocus();

    void setCursor(Cursor &cursor);

    inline int getID() const { return m_id; }

    virtual bool isPtIn(CPoint pt);

#ifdef _SKIN_EDITOR_
    virtual void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    virtual int fromXML(SXNode *pXmlNode);
#ifdef _SKIN_EDITOR_
    virtual void toXML(CXMLWriter &xmlStream);
#endif // _SKIN_EDITOR_

    AnimateType getAnimationType() const;
    int getAnimationDuration() const;

    virtual cstr_t getClassName() { return ms_szClassName; }
    virtual bool isKindOf(cstr_t szClassName) { return strcasecmp(szClassName, ms_szClassName) == 0; }
    static cstr_t className() { return ms_szClassName; }

    // profile api
    uint32_t getInt(cstr_t szKeyName, int nDefault);
    bool getBool(cstr_t szKeyName, bool bDefault) { return getInt(szKeyName, bDefault) != 0; }
    string getString(cstr_t szKeyName, cstr_t szDefault);
    void writeInt(cstr_t szKeyName, int value);
    void writeString(cstr_t szKeyName, cstr_t szValue);
    string getProfileKeyName(cstr_t szSubKeyName);

    virtual void onLanguageChanged();

    int getIDByName(cstr_t szID);

public:
    //
    // CSkinContainer API
    //
    virtual bool isContainer() const { return false; }
    virtual CSkinContainer *getContainerIf() { return nullptr; }

    void tryToCallInitialUpdate()
        { if (!m_bInitialUpdated) { m_bInitialUpdated = true; onInitialUpdate(); } }

protected:
#ifdef _SKIN_EDITOR_
    // For Skin Editor to port child.
    virtual bool hasXMLChild() { return false; }
    virtual void onToXMLChild(CXMLWriter &xmlStream) {}
#endif // _SKIN_EDITOR_

public:
    CFormula                    m_formTop;
    CFormula                    m_formLeft;
    CFormula                    m_formWidth;
    CFormula                    m_formHeight;

    uint32_t                    m_layoutParams;
    uint16_t                    m_weight;
    uint16_t                    m_minWidth, m_minHeight; // The minimum width and height
    bool                        m_fixedWidth, m_fixedHeight;

    CRect                       m_rcObj;            // The whole area of the UIObject, including its border.
    CRect                       m_rcContent;        // The content area of the UIObject, for example: for TextButton, it's the text area.

    // For differences between margin and padding: http://stackoverflow.com/questions/4619899/difference-between-a-views-padding-and-margin
    CRect                       m_rcMargin;         // The margin of UIObject (left, top, right, bottom)
    CRect                       m_rcPadding;        // The padding of UIObject (left, top, right, bottom)

    // auto allocated uni-ID by CSkinWnd.
    int                         m_autoUniID;

    int                         m_id;
    string                      m_strName;

    int                         m_msgNeed;          // message mask is used to specify what messages ui objects want to process
    string                      m_strText, m_strTextEnglish;
    bool                        m_enable;           // Is control being enabled?
    bool                        m_visible;
    bool                        m_hideIfWndInactive;
    bool                        m_bHideIfMouseInactive;
    bool                        m_isMouseIn;        // 鼠标是否在此组件内？

    JsValue                     m_onMouseEnterListener;
    JsValue                     m_onMouseLeaveListener;

    /**
     * Background related properties.
     **/
    BackgroundType              m_bgType;

    // Background color
    CColor                      m_clrBg, m_clrBgOrg;

    // Background images
    CScaleImagePainter          m_bgImagePainter;
    CSFImage                    m_imageBg, m_imageBgMask;
    BlendPixMode                m_bgBpm;

    bool                        m_translucencyWithSkin; // set the alpha of UIObject to the skin, if this flag is set.
    int16_t                     m_opacity;

    string                      m_strMaskImage;
    CSFImage                    m_imgMask;

protected:
    static cstr_t               ms_szClassName;

    string                      m_strTooltip, m_strTooltipEnglish;
    bool                        m_bTempTooltip;

    AnimateType                 m_animateType;
    int                         m_animateDuration;

    CSkinWnd                    *m_pSkin;
    CSkinContainer              *m_pContainer;

protected:
    bool                        m_bCreated;
    bool                        m_bInitialUpdated;

    friend class CSkinWnd;
    friend class CSkinContainer;

};
