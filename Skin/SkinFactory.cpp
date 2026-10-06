#include "SkinTypes.h"
#include "Skin.h"
#include "SkinFactory.h"
#include "SkinWndResizeObj.h"
#include "SkinFadeArea.h"
#include "SkinDecorativeContainer.h"
#include "SkinToggleCtrlContainer.h"
#include "SkinMenuBar.h"
#include "SkinFrameCtrl.h"
#include "SkinEditCtrl.h"
#include "SkinLinearContainer.h"
#include "SkinResizableLinearContainer.h"
#include "SkinMenuItemsContainer.hpp"
#include "SkinFilterCtrl.h"
#include "SkinRateCtrl.h"
#include "SkinPicText.h"
#include "SkinTreeCtrl.h"


#define _SZ_SKINWND         "skinwnd"
#define _SZ_MENUS           "Menus"
#define _SZ_DYNMICCTRLS     "DynControls"
#define _SZ_DYNMICCMDS      "DynCmds"
#define _SZ_CONTROLDEF      "controldef"
#define _SZ_SKIN_FILE_NAME  "main.xml"
#define _SZ_ADJUST_HUE      "adjustHue"

#define MENU_ID_CUSTOM_BASE 46000


SkinWndStartupInfo::SkinWndStartupInfo(cstr_t szClassName, cstr_t szCaptionText,
    cstr_t szSkinWnd, Window *pWndParent, bool bMainWnd) {
    this->strClassName = szClassName;
    this->strCaptionText = szCaptionText;
    this->strSkinWnd = szSkinWnd;
    this->pWndParent = pWndParent;
    this->bMainWnd = bMainWnd;
    this->pWndParent = pWndParent;
    this->pSkinWnd = nullptr;
}

#undef DEFINE_CMD_ID
#define DEFINE_CMD_ID(uid) { #uid, uid, nullptr },

#undef DEFINE_CMD_ID_TIP
#define DEFINE_CMD_ID_TIP(uid, tooltip)   { #uid, uid, tooltip },

UIObjectIDDefinition g_uidBaseDefinition[] = {
    SKIN_BASE_IDS

    DEFINE_CMD_ID(ID_OK)
    DEFINE_CMD_ID(ID_CANCEL)
    DEFINE_CMD_ID_TIP(ID_CLOSE, _TLM("Close"))

    { nullptr, 0, nullptr },
};

/*
   <DynControls>
     <VScrollBar width="14" PushBtHeight="14" ThumbHeight="28" TrackHeight="28" ScrollBarWidth="14" Image="vscrollbar.bmp"/>
     <ListCtrl BkColor="#4F586E" SelBkColor="#D9D9D9" NowPlayBkColor="#5478CA" TextColor="#FFFFFF" SelTextColor="#4F586E" NowPlayTextColor="#FFFFFF" FontHeight="13"/>
     <Toolbar name="normal" image="toolbar-small.bmp" enablehover="true" height="18" units_x="18" blank_x="216" blank_cx="18" seperator_x="216" seperator_cx="10">
     </Toolbar>
   </DynControls>
*/

void insertNodeAbove(SXNode *pNodeParent, cstr_t szNodeAbove, SXNode *pNodeChild) {
    SXNode::iterator it, itEnd;

    itEnd = pNodeParent->listChildren.end();
    for (it = pNodeParent->listChildren.begin(); it != itEnd; ++it) {
        SXNode *pNode = *it;

        if (strcasecmp(pNode->name.c_str(), szNodeAbove) == 0) {
            pNodeParent->listChildren.insert(it, pNodeChild);
            return;
        }
    }

    pNodeParent->listChildren.push_back(pNodeChild);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Class CSkinFileXML
CSkinFileXML::CSkinFileXML() {
}

CSkinFileXML::~CSkinFileXML() {
}

int CSkinFileXML::load(cstr_t szFile) {
    m_strFile = szFile;

    if (!m_xml.parseFile(szFile)) {
        return ERR_PARSE_XML;
    }

    if (strcasecmp(m_xml.m_pRoot->name.c_str(), "skin") == 0) {
        return ERR_OK;
    } else {
        return ERR_BAD_FILE_FORMAT;
    }
}

int CSkinFileXML::save(cstr_t szFile) {
    CXMLWriter xml;

#ifdef _SKIN_EDITOR_
    m_xml.m_pRoot->toXML(xml);
#endif // _SKIN_EDITOR_

    if (xml.saveAsFile(szFile)) {
        return ERR_OK;
    } else {
        return ERR_WRITE_FILE;
    }
}

void CSkinFileXML::close() {
    m_xml.free();
    m_strFile.resize(0);
}

int CSkinFileXML::getSkinWndNode(cstr_t szSkinWndName, SXNode::iterator *itWnd) {
    assert(isOpened());
    if (!isOpened()) {
        return ERR_NOT_FOUND;
    }

    SXNode *pNodeRoot;
    pNodeRoot = m_xml.m_pRoot;
    SXNode::iterator it, itEnd;

    itEnd = pNodeRoot->listChildren.end();
    for (it = pNodeRoot->listChildren.begin(); it != itEnd; ++it) {
        SXNode *pNode = *it;
        if (strcasecmp(pNode->name.c_str(), _SZ_SKINWND) == 0) {
            cstr_t szName = pNode->getProperty(SZ_PN_NAME);
            if (szName && strcasecmp(szName, szSkinWndName) == 0) {
                *itWnd = it;
                return ERR_OK;
            }
        }
    }

    return ERR_NOT_FOUND;
}

SXNode *CSkinFileXML::getSkinWndNode(cstr_t szSkinWndName) {
    SXNode::iterator itWnd;
    int nRet;

    nRet = getSkinWndNode(szSkinWndName, &itWnd);
    if (nRet == ERR_OK) {
        return *itWnd;
    }

    return nullptr;
}

int CSkinFileXML::newSkinWndNode(cstr_t szSkinWndName) {
    assert(isOpened());
    if (!isOpened()) {
        return ERR_NOT_FOUND;
    }

    SXNode::iterator itWnd;
    int nRet;
    SXNode *pWndNode;

    nRet = getSkinWndNode(szSkinWndName, &itWnd);
    if (nRet == ERR_OK) {
        return ERR_EXIST;
    }

    pWndNode = new SXNode;
    pWndNode->name = "skinwnd";
    pWndNode->setProperty(SZ_PN_NAME, szSkinWndName);
    m_xml.m_pRoot->listChildren.push_back(pWndNode);

    return ERR_OK;
}

int CSkinFileXML::setSkinWndNode(cstr_t szSkinWndName, SXNode *pWndNode) {
    SXNode::iterator itWnd;
    int nRet;

    nRet = getSkinWndNode(szSkinWndName, &itWnd);
    if (nRet == ERR_OK) {
        SXNode *pNode = *itWnd;
        delete pNode;
        *itWnd = pWndNode;
        return ERR_OK;
    }

    assert(0 && "Not fond skin wnd node");

    return nRet;
}

int CSkinFileXML::removeSkinWnd(cstr_t szSkinWndName) {
    SXNode::iterator itWnd;
    int nRet;

    nRet = getSkinWndNode(szSkinWndName, &itWnd);
    if (nRet == ERR_OK) {
        m_xml.m_pRoot->listChildren.erase(itWnd);
    }

    return nRet;
}

int CSkinFileXML::changeSkinWndName(cstr_t szNameOld, cstr_t szNameNew) {
    SXNode::iterator itWnd;
    int nRet;

    if (strcasecmp(szNameNew, szNameOld) == 0) {
        return ERR_OK;
    }

    if (!canUseSkinWndName(szNameNew)) {
        return ERR_EXIST;
    }

    nRet = getSkinWndNode(szNameOld, &itWnd);
    if (nRet == ERR_OK) {
        SXNode *pNode;
        pNode = *itWnd;
        pNode->setProperty(SZ_PN_NAME, szNameNew);
    }

    return nRet;
}

bool CSkinFileXML::canUseSkinWndName(cstr_t szName) {
    SXNode::iterator itWnd;
    int nRet;

    nRet = getSkinWndNode(szName, &itWnd);
    if (nRet == ERR_NOT_FOUND) {
        return true;
    }

    return false;
}

SXNode *CSkinFileXML::getMenusNode() {
    return m_xml.m_pRoot->getChild(_SZ_MENUS);
}

int CSkinFileXML::setMenusNode(SXNode *pNode) {
    return setNode(_SZ_MENUS, pNode);
}

SXNode *CSkinFileXML::getDynamicCmdsNode() {
    return m_xml.m_pRoot->getChild(_SZ_DYNMICCMDS);
}

int CSkinFileXML::setDynamicCmdsNode(SXNode *pNode) {
    return setNode(_SZ_DYNMICCMDS, pNode);
}

cstr_t CSkinFileXML::getSkinProperty(cstr_t szProperty) {
    return m_xml.m_pRoot->getProperty(szProperty);
}

int CSkinFileXML::getMainSkinWnd(string &strSkinWndName, cstr_t szSkinWndNameDef) {
    cstr_t szMainWnds, szDefMainWnd;

    szDefMainWnd = m_xml.m_pRoot->getPropertySafe("defmainwnd");
    szMainWnds = m_xml.m_pRoot->getPropertySafe("mainwnds");

    strSkinWndName.resize(0);

    //
    // Is szSkinWndName exists in "mainwnds"?
    //
    if (szSkinWndNameDef == nullptr || isEmptyString(szSkinWndNameDef)) {
        if (!isEmptyString(szDefMainWnd)) {
            strSkinWndName = szDefMainWnd;
            return ERR_OK;
        }
    } else if (szMainWnds) {
        vector<string> vWnds;
        strSplit(szMainWnds, ',', vWnds);
        for (int i = 0; i < (int)vWnds.size(); i++) {
            trimStr(vWnds[i]);
            if (strcasecmp(szSkinWndNameDef, vWnds[i].c_str()) == 0) {
                strSkinWndName = vWnds[i];
                return ERR_OK;
            }
        }
    }

    //
    // Use defined "defmainwnd"
    //
    if (!isEmptyString(szDefMainWnd)) {
        strSkinWndName = szDefMainWnd;
        return ERR_OK;
    }

    vector<string> vWnds;
    if (szMainWnds) {
        strSplit(szMainWnds, ',', vWnds);
    }
    if (vWnds.size()) {
        trimStr(vWnds[0]);
        strSkinWndName = vWnds[0];
        return ERR_OK;
    } else {
        ERR_LOG1("No mainwnds is defined: %s", m_strFile.c_str());
        return ERR_NOT_FOUND;
    }
}

void CSkinFileXML::enumSkinWnd(vector<string> &vSkinWnd) {
    SXNode *pNodeRoot;
    pNodeRoot = m_xml.m_pRoot;

    for (SXNode::iterator it = pNodeRoot->listChildren.begin();
    it != pNodeRoot->listChildren.end(); ++it)
        {
        SXNode *pNode = *it;

        if (strcasecmp(pNode->name.c_str(), _SZ_SKINWND) == 0) {
            cstr_t szName = pNode->getProperty(SZ_PN_NAME);
            if (szName) {
                vSkinWnd.push_back(szName);
            }
        }
    }
}

int CSkinFileXML::setNode(cstr_t szName, SXNode *pNode) {
    SXNode *pNodeOld;
    SXNode *pNodeRoot;

    pNodeRoot = m_xml.m_pRoot;
    for (SXNode::iterator it = pNodeRoot->listChildren.begin();
    it != pNodeRoot->listChildren.end(); ++it)
        {
        pNodeOld = *it;
        if (strcasecmp(pNodeOld->name.c_str(), szName) == 0) {
            delete pNodeOld;
            *it = pNode;
            return ERR_OK;
        }
    }

    insertNodeAbove(pNodeRoot, szName, pNode);

    return ERR_OK;
}

//////////////////////////////////////////////////////////////////////////
//
CSkinStyles::CSkinStyles() {
}

CSkinStyles::~CSkinStyles() {
    m_mapStyles.clear();
}

int CSkinStyles::fromXML(SXNode *pNodeSkin) {
    m_mapStyles.clear();

    for (SXNode *pNode : pNodeSkin->listChildren) {
        if (isPropertyName(pNode->name.c_str(), "styles")) {
            for (auto style : pNode->listChildren) {
                addStyle(style);
            }
        } else if (isPropertyName(pNode->name.c_str(), "style")) {
            addStyle(pNode);
        }
    }

    return ERR_OK;
}

void CSkinStyles::free() {
    m_mapStyles.clear();
}

SXNode *CSkinStyles::getClassNode(cstr_t className) const {
    auto it = m_mapStyles.find(className);
    if (it == m_mapStyles.end()) {
        return nullptr;
    }

    return (*it).second;
}

void CSkinStyles::addStyle(SXNode *nodeStyle) {
    for (auto node : nodeStyle->listChildren) {
        string styleName;
        SkinStyleOsKind osKind = parseSkinStyleName(node->name.c_str(), styleName);
        if (osKind == SkinStyleOsKind::Other) {
            continue;
        }
        if (styleName.empty()) {
            continue;
        }

        auto it = m_mapStyles.find(styleName);
        if (it != m_mapStyles.end()) {
            // 已有平台专用样式时，不能再用无后缀的通用样式盖掉。
            // 依赖节点保留原名（如 Caption.mac），不要改写成基名。
            string existingBase;
            if (parseSkinStyleName(it->second->name.c_str(), existingBase) == SkinStyleOsKind::Current
                && osKind != SkinStyleOsKind::Current) {
                DBG_LOG1("Skip generic style, keep OS style: %s", styleName.c_str());
                continue;
            }
            DBG_LOG1("Overwrite style name: %s", styleName.c_str());
        }

        // 先按旧表解析 Extends，避免 Caption.mac 覆盖 Caption 后变成自己继承自己。
        auto extends = node->getProperty(SZ_PN_EXTENDS);
        SXNode *nodeFrom = nullptr;
        if (extends) {
            auto itExt = m_mapStyles.find(extends);
            if (itExt != m_mapStyles.end()) {
                nodeFrom = itExt->second;
            }
        }

        // map key 用基名；node->name 保留带后缀的原名，供覆盖判断。
        m_mapStyles[styleName] = node;

        if (nodeFrom) {
            for (auto &prop : nodeFrom->listProperties) {
                if (node->getProperty(prop.name.c_str()) == nullptr) {
                    node->listProperties.push_back(prop);
                }
            }

            auto orgExtends = nodeFrom->getProperty(SZ_PN_EXTENDS);
            if (orgExtends != nullptr) {
                node->setProperty(SZ_PN_EXTENDS, orgExtends);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// CDynamicCmds class

int CDynamicCmds::fromXML(SXNode *pNodeDynCmds) {
    m_listDynCmd.clear();

    assert(m_pSkinFactory);
    assert(strcasecmp(pNodeDynCmds->name.c_str(), _SZ_DYNMICCMDS) == 0);
    for (SXNode::iterator it = pNodeDynCmds->listChildren.begin();
    it != pNodeDynCmds->listChildren.end(); ++it)
        {
        SXNode *pNode = *it;

        if (strcasecmp(pNode->name.c_str(), "cmd") == 0) {
            DynamicCmd cmd;
            cmd.nID = m_pSkinFactory->getIDByName(pNode->getPropertySafe(SZ_PN_ID));
            cmd.strFunction = pNode->getPropertySafe("func");
            cmd.strParam = pNode->getPropertySafe("param");
            m_listDynCmd.push_back(cmd);
        }
    }

    return ERR_OK;
}

#ifdef _SKIN_EDITOR_
SXNode *CDynamicCmds::toXML() {
    assert(m_pSkinFactory);

    SXNode *pNode;
    SXNode *pNodeRoot;

    pNodeRoot = new SXNode;
    pNodeRoot->name = _SZ_DYNMICCMDS;

    LIST_DYNCMD::iterator it, itEnd;
    itEnd = m_listDynCmd.end();
    for (it = m_listDynCmd.begin(); it != itEnd; ++it) {
        DynamicCmd &dc = *it;

        pNode = new SXNode;
        pNode->name = "cmd";
        pNode->addProperty(SZ_PN_ID, m_pSkinFactory->getStringOfID(dc.nID).c_str());
        pNode->addProperty("func", dc.strFunction.c_str());
        pNode->addProperty("param", dc.strParam.c_str());
        pNodeRoot->listChildren.push_back(pNode);
    }

    return pNodeRoot;
}
#endif // _SKIN_EDITOR_

void CDynamicCmds::free() {
    m_listDynCmd.clear();
}

bool CDynamicCmds::getCmd(int nCmdID, DynamicCmd &cmd) {
    LIST_DYNCMD::iterator it, itEnd;

    itEnd = m_listDynCmd.end();
    for (it = m_listDynCmd.begin(); it != itEnd; ++it) {
        DynamicCmd &dc = *it;
        if (dc.nID == nCmdID) {
            cmd = dc;
            return true;
        }
    }

    return false;
}

void CDynamicCmds::enumAllCmds(VecStrings &vstrCmds) {
    assert(m_pSkinFactory);

    LIST_DYNCMD::iterator it, itEnd;

    itEnd = m_listDynCmd.end();
    for (it = m_listDynCmd.begin(); it != itEnd; ++it) {
        DynamicCmd &dc = *it;
        vstrCmds.push_back(m_pSkinFactory->getStringOfID(dc.nID));
    }
}



CSkinFactory::CSkinFactory(CSkinApp *pApp, UIObjectIDDefinition uidDefinition[]) {
    m_pApp = pApp;

    m_strSkinFileName = _SZ_SKIN_FILE_NAME;
    m_userCmdIDNext = ID_ID_USER_BASE;
    m_dynamicCmds.init(this);

    assert(uidDefinition);
    for (int i = 0; uidDefinition[i].szId != 0; i++) {
        assert(m_setUIDDefinition.find(uidDefinition + i) == m_setUIDDefinition.end());
        m_setUIDDefinition.insert(uidDefinition + i);
    }

    for (int i = 0; g_uidBaseDefinition[i].szId != 0; i++) {
        assert(m_setUIDDefinition.find(g_uidBaseDefinition + i) == m_setUIDDefinition.end());
        m_setUIDDefinition.insert(g_uidBaseDefinition + i);
    }
}

CSkinFactory::~CSkinFactory() {

}

int CSkinFactory::init() {
    AddUIObjNewer(CSkinButton);
    AddUIObjNewer(CSkinImageButton);
    AddUIObjNewer(CSkinImage);
    AddUIObjNewer(CSkinNStatusImage);
    AddUIObjNewer(CSkinActiveImage);
    AddUIObjNewer(CSkinXScaleImage);
    m_mapUIObjNewer["XScaleImage"] = new CUIObjectNewer<CSkinXScaleImage>();
    AddUIObjNewer(CSkinYScaleImage);
    m_mapUIObjNewer["YScaleImage"] = new CUIObjectNewer<CSkinYScaleImage>();
    AddUIObjNewer(CSkinVScrollBar);
    AddUIObjNewer(CSkinHScrollBar);
    m_mapUIObjNewer["VScrollBarOSStyle"] = new CUIObjectNewer<CSkinVScrollBar>();
    m_mapUIObjNewer["HScrollBarOSStyle"] = new CUIObjectNewer<CSkinHScrollBar>();
    AddUIObjNewer(CSkinCaption);
    AddUIObjNewer(CSkinNStatusButton);
    AddUIObjNewer(CSkinContainer);
    AddUIObjNewer(CSkinDecorativeContainer);
    AddUIObjNewer(CSkinMenuBar);
    AddUIObjNewer(CSkinWndResizeObj);
    AddUIObjNewer(CSkinEditCtrl);
    AddUIObjNewer(CSkinSquare);
    AddUIObjNewer(CSkinFadeArea);
    AddUIObjNewer(CSkinToggleCtrlContainer);
    AddUIObjNewer(CSkinClientArea);
    AddUIObjNewer(CSkinAnimationUIObj);
    AddUIObjNewer(CSkinFrameCtrl);
    AddUIObjNewer(CSkinComboBox);
    AddUIObjNewer(CSkinListCtrl);
    AddUIObjNewer(CSkinListView);
    AddUIObjNewer(CSkinSeekCtrl);
    AddUIObjNewer(CSkinStaticText);
    AddUIObjNewer(CSkinTabButton);
    AddUIObjNewer(CSkinTextButton);
    AddUIObjNewer(CSkinToolbar);
    AddUIObjNewer(CSkinTxtLink);
    AddUIObjNewer(CSkinLinearContainer);
    AddUIObjNewer(CSkinResizableLinearContainer);
    AddUIObjNewer(SkinMenuItemsContainer);
    AddUIObjNewer(CSkinRateCtrl);
    AddUIObjNewer(CSkinPicText);
    AddUIObjNewer(CSkinFilterCtrl);
    AddUIObjNewer(CSkinTreeCtrl);

    return ERR_OK;
}

void CSkinFactory::quit() {
    for (MapUIObjNewer::iterator it = m_mapUIObjNewer.begin(); it != m_mapUIObjNewer.end(); ++it) {
        delete (*it).second;
    }

    m_mapUIObjNewer.clear();

    close(true);
}

int CSkinFactory::getIDByNameEx(cstr_t szId, string *pstrToolTip) {
    assert(szId);
    if (isEmptyString(szId)) {
        return ID_INVALID;
    }

    if (pstrToolTip) {
        pstrToolTip->resize(0);
    }

    UIObjectIDDefinition uidDef;
    uidDef.szId = szId;

    SetUIObjectIDDefinition::iterator it = m_setUIDDefinition.find(&uidDef);
    if (it != m_setUIDDefinition.end()) {
        UIObjectIDDefinition *p = *it;
        if (pstrToolTip && p->szToolTip) {
            *pstrToolTip = p->szToolTip;
        }
        return p->nId;
    }

    //
    // Find in user defined UIDs
    //
    {
        auto it = m_mapUserUIDs.find(szId);
        if (it != m_mapUserUIDs.end()) {
            return (*it).second;
        }
    }

    auto id = m_userCmdIDNext++;
    m_mapUserUIDs[szId] = id;

    return id;
}

string CSkinFactory::getTooltip(int nUID) {
    for (auto p : m_setUIDDefinition) {
        if (p->nId == nUID) {
            if (p->szToolTip) {
                return _TL(p->szToolTip);
            } else {
                break;
            }
        }
    }

    return string();
}

#ifdef DEBUG
void CSkinFactory::dumpUID() {
    typedef map<int, string> MapInt2String;

    MapInt2String mapUID;

    for (auto p : m_setUIDDefinition) {
        mapUID[p->nId] = string(p->szId);
    }

    for (MapInt2String::iterator it = mapUID.begin(); it != mapUID.end(); ++it) {
        DBG_LOG2("UID: %04X, %s", (*it).first, (*it).second.c_str());
    }

    for (auto &item : m_mapUserUIDs) {
        DBG_LOG2("User UID: %04X, %s", item.second, item.first.c_str());
    }
}
#endif

int CSkinFactory::allocUID() {
    m_userCmdIDNext++;
    assert(m_userCmdIDNext <= uint16_t(-1));
    if (m_userCmdIDNext >= uint16_t(-1)) {
        m_userCmdIDNext = ID_ID_USER_BASE + 1000;
    }

    return m_userCmdIDNext;
}

string CSkinFactory::getStringOfID(int id) {
    for (auto p : m_setUIDDefinition) {
        if (p->nId == id) {
            return p->szId;
        }
    }

    for (auto &item : m_mapUserUIDs) {
        if (item.second == id) {
            return item.first;
        }
    }

    return string();
}

int CSkinFactory::openSkin(cstr_t szSkinName, cstr_t szSkinDir, cstr_t szExtraDir) {
    string strSkinDir;

    assert(szSkinDir && szExtraDir);

    close();

    // If szSkinDir is nullptr, set to default position: %SkinRoot%\\szSkinName.
    if (szSkinDir == nullptr || isEmptyString(szSkinDir)) {
        strSkinDir = getSkinRootDir();
        strSkinDir += szSkinName;
        szSkinDir = strSkinDir.c_str();
    }

    m_strSkinName = szSkinName;

    m_resourceMgr.clearRessourceDir();
    m_resourceMgr.addRessourceDir(szSkinDir);
    if (szExtraDir && !isEmptyString(szExtraDir)) {
        m_resourceMgr.addRessourceDir(szExtraDir);
    }
    m_resourceMgr.addRessourceDir(dirStringJoin(getSkinRootDir(), "assets").c_str());

    string skinFile;
    if (!m_resourceMgr.getResourcePathName(m_strSkinFileName.c_str(), skinFile)) {
        ERR_LOG2("Can't find skin file in dir: %s, %s", szSkinDir, szExtraDir);
        return ERR_OPEN_FILE;
    }

    return openSkinFile(skinFile.c_str());
}

int CSkinFactory::changeSkin(cstr_t szSkinName, cstr_t szSkinDir, cstr_t szExtraDir, bool bLoadSkinColorTheme, bool bCreateSkinWnd) {
    int nRet = openSkin(szSkinName, szSkinDir, szExtraDir);
    if (nRet != ERR_OK) {
        return nRet;
    }

    if (bLoadSkinColorTheme) {
        // load the default theme settings of skin
        g_profile.writeInt(szSkinName, "Hue", 0);

        applyDefaultThemeOfSkin();
    }

    float hue = (float)g_profile.getInt(szSkinName, "Hue", 0);
    if (hue < 0) {
        hue = 0;
    } else if (hue > 360) {
        hue = 360;
    }
    m_resourceMgr.adjustHue((float)hue, 0.5, 0.5);

    if (!bCreateSkinWnd) {
        return ERR_OK;
    }

    // change the skins of existed windows.
    string strSkinWndDef;
    string strSkinWndName;
    string strOpenedSkinWnds;
    SetICaseStr setOpenedSkinWnds;

    strOpenedSkinWnds = g_profile.getString(getSkinFileName(), "LatestOpenedSkinWnds", "");
    strSkinWndDef = g_profile.getString(getSkinFileName(), "DefaultSkinWnd", "");

    VecStrings strs;
    StringView(strOpenedSkinWnds).split(',', strs);
    for (auto &s : strs) {
        setOpenedSkinWnds.insert(s);
    }

    nRet = m_skinFile.getMainSkinWnd(strSkinWndName, strSkinWndDef.c_str());
    if (nRet != ERR_OK) {
        ERR_LOG1("Can't get main window name: %s", m_skinFile.getFileName());
        return nRet;
    }

    {
        // remove main window from other windows.
        SetICaseStr::iterator it = setOpenedSkinWnds.find(strSkinWndName.c_str());
        if (it != setOpenedSkinWnds.end()) {
            setOpenedSkinWnds.erase(it);
        }
    }

    // create or open Main Window
    CSkinWnd *pMainWnd = nullptr;
    if (m_listSkinWnds.empty()) {
        pMainWnd = newSkinWnd("MusicPlayer", true);

        SkinWndStartupInfo skinWndStartupInfo("MusicPlayer", "MusicPlayer", strSkinWndName.c_str(), nullptr, true);
        int nRet = pMainWnd->create(skinWndStartupInfo, this, false);
        assert(nRet == ERR_OK);
        if (nRet == ERR_OK) {
            pMainWnd->setMainAppWnd(true);
        }
    } else {
        pMainWnd = m_listSkinWnds.front();
        nRet = pMainWnd->openSkin(strSkinWndName.c_str());
        if (nRet != ERR_OK) {
            return nRet;
        }
        pMainWnd->show();
    }

    ListSkinWnds listToBeProcessed;

    // destroy other skin windows.
    if (m_listSkinWnds.size() > 1) {
        listToBeProcessed.insert(listToBeProcessed.begin(), ++(m_listSkinWnds.begin()), m_listSkinWnds.end());

        for (auto it = listToBeProcessed.begin(); it != listToBeProcessed.end(); ++it) {
            CSkinWnd *pSkinWnd = *it;
            nRet = pSkinWnd->openDefaultSkin();
            if (nRet != ERR_OK) {
                pSkinWnd->destroy();
            } else {
                setOpenedSkinWnds.erase(pSkinWnd->getSkinWndName());
            }
        }
    }

    for (SetICaseStr::iterator itSet = setOpenedSkinWnds.begin();
    itSet != setOpenedSkinWnds.end(); ++itSet)
        {
        if (!isAppResSkinWnd((*itSet).c_str())) {
            SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, (*itSet).c_str(), (*itSet).c_str(), nullptr);
            nRet = createSkinWnd(skinWndStartupInfo);
            if (nRet != ERR_OK) {
                DBG_LOG1("Failed to create skin window: %s", Error2Str(nRet).c_str());
            }
        }
    }

    return ERR_OK;
}

void CSkinFactory::getWndDragAutoCloseTo(vector<Window *> &vWnd) {
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pSkinWnd = *it;
        vWnd.push_back(pSkinWnd);
    }
}

void CSkinFactory::getWndDragTrackMove(vector<Window *> &vWnd) {
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pSkinWnd = *it;
        vWnd.push_back(pSkinWnd);
    }
}

int CSkinFactory::openSkinFile(cstr_t szSkinFile) {
    int nRet;
    SXNode *pNode;

    nRet = m_skinFile.load(szSkinFile);
    if (nRet != ERR_OK) {
        ERR_LOG1("Failed to parse skin file(xml): %s", szSkinFile);
        return nRet;
    }

    // Extra resource folder can be muliple folder, seperated with ','.
    cstr_t szExtraResouceFolder = m_skinFile.getSkinProperty("ExtraResouceFolder");
    if (szExtraResouceFolder && !isEmptyString(szExtraResouceFolder)) {
        VecStrings vExtraResDirs;

        strSplit(szExtraResouceFolder, ',', vExtraResDirs);

        for (auto &path : vExtraResDirs) {
            trimStr(path);
            if (path.empty()) {
                continue;
            }
#ifdef WIN32
            strrep(path, "/", PATH_SEP_STR);
#else
            strrep(path, "\\", PATH_SEP_STR);
#endif
            string strExtraDirAbs;
            strExtraDirAbs = getSkinRootDir();
            if (path[0] == PATH_SEP_CHAR) {
                strExtraDirAbs += path.c_str() + 1;
            } else {
                strExtraDirAbs += path;
            }
            m_resourceMgr.addRessourceDir(strExtraDirAbs.c_str(), 1);
        }
    }

    expandIncludeNode(m_skinFile.getRootNode());

    m_skinStyle.fromXML(m_skinFile.getRootNode());
    m_icons.load(&m_resourceMgr);

    pNode = m_skinFile.getDynamicCmdsNode();
    if (pNode) {
        m_dynamicCmds.fromXML(pNode);
    }

    return ERR_OK;
}

void CSkinFactory::expandIncludeNode(SXNode *parent) {
    for (SXNode *&p : parent->listChildren) {
        if (isPropertyName(p->name.c_str(), "include")) {
            cstr_t szName = p->getProperty(SZ_PN_NAME);
            if (!szName || isEmptyString(szName)) {
                ERR_LOG0("Invalid include node");
                continue;
            }

            CSimpleXML xml;
            string includeFile;
            if (!m_resourceMgr.getResourcePathName(szName, includeFile)) {
                ERR_LOG1("Can't get resource file: %s", szName);
                continue;
            }

            if (!xml.parseFile(includeFile.c_str())) {
                ERR_LOG1("Failed to parse xml file: %s", includeFile.c_str());
                continue;
            }
            expandIncludeNode(xml.m_pRoot);

            delete p;
            p = xml.m_pRoot;
            xml.m_pRoot = nullptr;
        } else {
            expandIncludeNode(p);
        }
    }
}

int CSkinFactory::saveSkinFile(cstr_t szFile) {
    /*
CXMLWriter        xmlStream;

xmlStream.pushCategory("skin");
toXML(xmlStream);
xmlStream.popCategory();

if (xmlStream.saveAsFile(szFile))
    return ERR_OK;

*/
    return ERR_FAILED;
}

void CSkinFactory::close(bool bQuit) {
    m_icons.clear();
    m_skinStyle.free();
    m_dynamicCmds.free();
    m_skinFile.close();

    // save opened window name, and close it.
    string strOpenedSkinWnds;
    ListSkinWnds listSkinWnds = m_listSkinWnds;
    for (auto it = listSkinWnds.begin(); it != listSkinWnds.end(); ++it) {
        CSkinWnd *pSkin = *it;

        // If the skin window has been destroyed, ignore it.
        if (find(m_listSkinWnds.begin(), m_listSkinWnds.end(), *it) == m_listSkinWnds.end()) {
            continue;
        }

        if (pSkin->isMainAppWnd()) {
            g_profile.writeString(getSkinFileName(), "DefaultSkinWnd", pSkin->getSkinWndName());
        } else {
            if (!strOpenedSkinWnds.empty()) {
                strOpenedSkinWnds += ",";
            }
            strOpenedSkinWnds += pSkin->getSkinWndName();
        }

        if (bQuit) {
            pSkin->destroy();
        } else {
            pSkin->closeSkin();
        }
    }
    if (listSkinWnds.size() > 0) {
        g_profile.writeString(getSkinFileName(), "LatestOpenedSkinWnds", strOpenedSkinWnds.c_str());
    }

    // assert(m_listSkinWnds.size() <= 1);

    m_resourceMgr.onClose();

    // Recover the original UIDs.
    m_userCmdIDNext = ID_ID_USER_BASE;
    m_mapUserUIDs.clear();
}

CSkinWnd *CSkinFactory::newSkinWnd(cstr_t szSkinWndName, bool bMainWnd) {
    return new CSkinWnd;
}

CSkinWnd *CSkinFactory::findSkinWnd(cstr_t szSkinWndName) {
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pSkinWnd = *it;
        if (strcasecmp(szSkinWndName, pSkinWnd->getSkinWndName()) == 0) {
            return pSkinWnd;
        }
    }

    return nullptr;
}

bool CSkinFactory::isValidSkinWnd(CSkinWnd *pWnd) {
    for (auto it = m_listSkinWnds.begin(); it != m_listSkinWnds.end(); ++it) {
        if (*it == pWnd) {
            return true;
        }
    }

    return false;
}

int CSkinFactory::activeOrCreateSkinWnd(SkinWndStartupInfo &skinWndStartupInfo) {
    CSkinWnd *pSkinWnd = findSkinWnd(skinWndStartupInfo.strSkinWnd.c_str());
    if (pSkinWnd) {
        if (!pSkinWnd->isVisible()) {
            pSkinWnd->show();
        }
        if (pSkinWnd->isIconic()) {
            pSkinWnd->restore();
        }
        pSkinWnd->setForeground();
        skinWndStartupInfo.pSkinWnd = pSkinWnd;
        return ERR_OK;
    }

    return createSkinWnd(skinWndStartupInfo);
}

int CSkinFactory::openOrCloseSkinWnd(SkinWndStartupInfo &skinWndStartupInfo) {
    CSkinWnd *pSkinWnd = findSkinWnd(skinWndStartupInfo.strSkinWnd.c_str());
    if (pSkinWnd) {
        pSkinWnd->destroy();
        return ERR_OK;
    }

    return createSkinWnd(skinWndStartupInfo);
}

int CSkinFactory::createSkinWnd(SkinWndStartupInfo &skinWndStartupInfo) {
    // If the skinwnd doesn't exist, return ERR_NOT_FOUND
    if (getSkinWndNode(skinWndStartupInfo.strSkinWnd.c_str()) == nullptr) {
        if (!isAppResSkinWnd(skinWndStartupInfo.strSkinWnd.c_str())) {
            return ERR_NOT_FOUND;
        }
    }

    // If the pSkinWnd is newed already, use it.
    if (skinWndStartupInfo.pSkinWnd == nullptr) {
        skinWndStartupInfo.pSkinWnd = newSkinWnd(skinWndStartupInfo.strSkinWnd.c_str(), skinWndStartupInfo.bMainWnd);
    }
    assert(skinWndStartupInfo.pSkinWnd);
    skinWndStartupInfo.pSkinWnd->setFreeOnDestory(true);
    int nRet = skinWndStartupInfo.pSkinWnd->create(skinWndStartupInfo, this);
    if (nRet != ERR_OK) {
        delete skinWndStartupInfo.pSkinWnd;
    }

    return nRet;
}

void CSkinFactory::onSkinWndCreate(CSkinWnd *pSkinWnd) {
#ifdef DEBUG
    for (auto it = m_listSkinWnds.begin(); it != m_listSkinWnds.end(); ++it) {
        CSkinWnd *pSkin = *it;
        assert(pSkin != pSkinWnd);
    }
#endif

    m_listSkinWnds.push_back(pSkinWnd);
}

void CSkinFactory::onSkinWndDestory(CSkinWnd *pSkinWnd) {
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pSkin = *it;
        if (pSkin == pSkinWnd) {
            m_listSkinWnds.erase(it);
            return;
        }
    }
}

void CSkinFactory::onMainSkinWndDestory(CSkinWnd *pSkinWnd) {
    int nMainAppWnd = 0;
    for (ListSkinWnds::iterator it = m_listSkinWnds.begin();
    it != m_listSkinWnds.end(); ++it)
        {
        CSkinWnd *p = *it;
        if (p->isMainAppWnd()) {
            nMainAppWnd++;
        }
    }
    if (nMainAppWnd <= 1) {
        m_pApp->postQuitMessage();
    }
}

// methods for skin window
SXNode *CSkinFactory::getSkinWndNode(cstr_t szSkinWndName) {
    return m_skinFile.getSkinWndNode(szSkinWndName);
}

int CSkinFactory::loadAppResSkinWndXml(cstr_t szSkinWndName, CSimpleXML &xml) {
    string file;
    if (!m_resourceMgr.getResourcePathName(szSkinWndName, file)) {
        return ERR_NOT_FOUND;
    }

    if (!xml.parseFile(file.c_str())) {
        return ERR_PARSE_XML;
    }

    expandIncludeNode(xml.m_pRoot);

    return ERR_OK;
}

bool CSkinFactory::isAppResSkinWnd(cstr_t szSkinWndName) {
    string file;
    if (m_resourceMgr.getResourcePathName(szSkinWndName, file)) {
        return true;
    }

    return false;
}

void CSkinFactory::registerUIObjNewer(cstr_t szClassName, IUIObjNewer *pObjNewer) {
    assert(m_mapUIObjNewer.find(szClassName) == m_mapUIObjNewer.end());
    m_mapUIObjNewer[szClassName] = pObjNewer;
}

// create new uiobject by class name
CUIObject * CSkinFactory::createUIObject(CSkinWnd *pSkin, cstr_t szClassName, CSkinContainer *pContainer) {
    SXNode *pNode = m_skinStyle.getClassNode(szClassName);
    if (!pNode) {
        return newUIObject(pSkin, pContainer, szClassName);
    }

    cstr_t szExtends = pNode->getProperty(SZ_PN_EXTENDS);
    if (szExtends) {
        szClassName = szExtends;
    }

    CUIObject *pObj = newUIObject(pSkin, pContainer, szClassName);

    // set styled properties of Extends from
    if (pObj) {
        pObj->fromXML(pNode);
    }

    return pObj;
}

CUIObject *CSkinFactory::newUIObject(CSkinWnd *pSkin, CSkinContainer *pContainer, cstr_t szClassName) {
    MapUIObjNewer::iterator it = m_mapUIObjNewer.find(szClassName);
    if (it == m_mapUIObjNewer.end()) {
        return nullptr;
    }

    CUIObject *pObj = (*it).second->New();
    assert(pObj);
    pObj->setParent(pSkin, pContainer);

    return pObj;
}

CUIObject *CSkinFactory::createDynamicCtrl(CSkinContainer *pContainer, cstr_t szClassName, int nIDAssign, cstr_t szLeft, cstr_t szTop, cstr_t szWidth, cstr_t szHeight) {
    CUIObject *pObj = nullptr;

    // create the control
    pObj = createUIObject(pContainer->getSkinWnd(), szClassName, pContainer);
    assert(pObj);
    if (pObj) {
        if (nIDAssign == ID_INVALID) {
            nIDAssign = pContainer->getSkinWnd()->allocUIObjID();
        }
        pObj->m_id = nIDAssign;

        //
        // 先使用参数来设置高度和宽度
        if (szWidth && !isEmptyString(szWidth)) {
            pObj->setProperty(SZ_PN_WIDTH, szWidth);
        }
        if (szHeight && !isEmptyString(szHeight)) {
            pObj->setProperty(SZ_PN_HEIGHT, szHeight);
        }
        if (szLeft && !isEmptyString(szLeft)) {
            pObj->setProperty(SZ_PN_LEFT, szLeft);
        }
        if (szLeft && !isEmptyString(szTop)) {
            pObj->setProperty(SZ_PN_TOP, szTop);
        }

        // 添加到数组中
        pContainer->addUIObject(pObj);
    }

    return pObj;
}

void CSkinFactory::setSkinsRootDir(cstr_t szSkinsRootDir) {
    m_strSkinRootDir = szSkinsRootDir;
    if (!m_strSkinRootDir.empty()) {
        dirStringAddSep(m_strSkinRootDir);
        g_profile.writeString("SkinRootDir", m_strSkinRootDir.c_str());
    }
}

cstr_t CSkinFactory::getSkinRootDir() {
    if (m_strSkinRootDir.empty()) {
        m_strSkinRootDir = g_profile.getString("SkinRootDir", "");
        if (!m_strSkinRootDir.empty()) {
            dirStringAddSep(m_strSkinRootDir);
            if (isDirExist(m_strSkinRootDir.c_str())) {
                return m_strSkinRootDir.c_str();
            }
        }

        m_strSkinRootDir = getAppResourceDir();
        m_strSkinRootDir += "skins";


        // 可能在编译输出目录 build/debug
        if (!isFileExist(m_strSkinRootDir.c_str())) {
            m_strSkinRootDir = getAppResourceDir();
            m_strSkinRootDir += "../../Skins-Design/skins";
            strrep(m_strSkinRootDir, '/', PATH_SEP_CHAR);
        }

        m_strSkinRootDir += PATH_SEP_STR;
    }

    return m_strSkinRootDir.c_str();
}

void CSkinFactory::setSkinFileName(cstr_t szSkinFileName) {
    m_strSkinFileName = szSkinFileName;
}

// 枚举 skin
// COMMENT:
//        查找Skin目录下的第一个Skin
// RETURN:
//        The handle of find skin
//        INVALID_HANDLE_VALUE - 没有找到
bool CSkinFactory::findFirstSkin(string &strSkin) {
    FileFind find;

    if (!find.openDir(getSkinRootDir())) {
        return false;
    }

    while (find.findNext()) {
        if (find.isCurDir()) {
            string fn = getSkinRootDir();
            fn += find.getCurName();
            fn += PATH_SEP_STR;
            fn += m_strSkinFileName;

            if (isFileExist(fn.c_str())) {
                strSkin = find.getCurName();
                return true;
            }
        }
    }

    return false;
}

bool CSkinFactory::enumAllSkins(vector<string> &vSkins) {
    FileFind find;

    if (!find.openDir(getSkinRootDir())) {
        return false;
    }

    while (find.findNext()) {
        if (find.isCurDir()) {
            string fn = getSkinRootDir();
            fn += find.getCurName();
            fn += PATH_SEP_STR;
            fn += m_strSkinFileName;

            if (isFileExist(fn.c_str())) {
                vSkins.push_back(find.getCurName());
            }
        }
    }

    // sort(vSkins.begin(), vSkins.end());

    return true;
}

CSkinWnd *CSkinFactory::getMainWnd() {
    for (auto it = m_listSkinWnds.begin(); it != m_listSkinWnds.end(); ++it) {
        CSkinWnd *p = *it;
        if (p->isMainAppWnd()) {
            return p;
        }
    }

    return nullptr;
}

SXNode *CSkinFactory::getExtendsStyle(cstr_t szExtendsSkinWnd) const {
    return m_skinStyle.getClassNode(szExtendsSkinWnd);
}

static SXNode *getMenuOfMenus(SXNode *pNodeMenus, cstr_t szMenuName) {
    SXNode::iterator it, itEnd;
    itEnd = pNodeMenus->listChildren.end();
    for (it = pNodeMenus->listChildren.begin(); it != itEnd; ++it) {
        SXNode *pNode = *it;

        // OK, found the contorl
        if (strcasecmp(pNode->name.c_str(), "Menu") == 0) {
            if (strcasecmp(pNode->getPropertySafe("Name"), szMenuName) == 0) {
                return pNode;
            }
        }
    }

    return nullptr;
}

SkinMenuPtr CSkinFactory::loadMenu(CSkinWnd *pWnd, cstr_t szMenu) {
    SkinMenuPtr menu;

    // load menu from xml
    SXNode *pNodeMenus = m_skinFile.getMenusNode();
    if (pNodeMenus) {
        SXNode *pNode = getMenuOfMenus(pNodeMenus, szMenu);
        if (pNode) {
            cstr_t szFromMenu = pNode->getProperty("From");
            if (szFromMenu) {
                menu = loadPresetMenu(pWnd, szFromMenu);
            }

            if (!menu) {
                menu = std::make_shared<CSkinMenu>();
            }

            menu->fromXML(pNode, menu->getOrgAppendPos());
        }
    }

    if (!menu) {
        // is this a preset menu?
        menu = loadPresetMenu(pWnd, szMenu);
    }

    return menu;
}

void CSkinFactory::showPopupMenu(CSkinWnd *pWnd, cstr_t menuName) {
    auto menu = loadMenu(pWnd, menuName);
    if (menu) {
        CPoint pt = getCursorPos();
        menu->trackPopupMenu(pt.x, pt.y, pWnd, nullptr);
    }
}

SkinMenuPtr CSkinFactory::loadPresetMenu(CSkinWnd *pWnd, cstr_t szMenu) {
    if (m_menus.IsNull()) {
        string fn = CSkinApp::getInstance()->getSkinFactory()->getResourceMgr()->getResourcePathName("menu.json");
        string content;
        if (!readFile(fn.c_str(), content)) {
            ERR_LOG1("Failed to load menu file: %s", fn.c_str());
            return nullptr;
        }

        m_menus.Parse(content.c_str(), content.size());
        assert(m_menus.IsObject());
    }

    if (m_menus.HasMember(szMenu)) {
        return newSkinMenu(pWnd, m_menus[szMenu]);
    } else {
        ERR_LOG1("Can NOT find menu %s.", szMenu);
        return nullptr;
    }
}

int CSkinFactory::onDynamicCmd(int nCmdID, CSkinWnd *pSkinWnd) {
    int nRet = ERR_NOT_FOUND;

    CDynamicCmds::DynamicCmd cmd;
    if (m_dynamicCmds.getCmd(nCmdID, cmd)) {
        nRet = exeDynamicCmd(cmd, pSkinWnd);
    } else {
        nRet = ERR_NOT_FOUND;
    }

    return nRet;
}

int CSkinFactory::exeDynamicCmd(const CDynamicCmds::DynamicCmd &cmd, CSkinWnd *pSkinWnd) {
    if (cmd.strFunction == "change_skin_wnd") {
        return pSkinWnd->openSkin(cmd.strParam.c_str());
    } else if (cmd.strFunction == "create_skin_wnd") {
        SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, cmd.strParam.c_str(), cmd.strParam.c_str(), nullptr);
        return activeOrCreateSkinWnd(skinWndStartupInfo);
    } else if (cmd.strFunction == "openclose_skin_wnd") {
        CSkinWnd *pSkinWnd = findSkinWnd(cmd.strParam.c_str());
        if (pSkinWnd) {
            if (pSkinWnd->isIconic()) {
                pSkinWnd->activateWindow();
            } else {
                pSkinWnd->postDestroy();
            }
            return ERR_OK;
        }

        SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, cmd.strParam.c_str(), cmd.strParam.c_str(), nullptr);
        return createSkinWnd(skinWndStartupInfo);
    } else if (cmd.strFunction == "is_skin_wnd_visible") {
        CSkinWnd *pSkinWnd = findSkinWnd(cmd.strParam.c_str());
        if (pSkinWnd) {
            return ERR_OK;
        } else {
            return ERR_FALSE;
        }
    } else if (cmd.strFunction == "load_ctrl_visible_state") {
        VecStrings vStrParam;
        CUIObject *pObj;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        if (vStrParam.size() > 2 || vStrParam.size() == 0) {
            return ERR_FALSE;
        }
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);

            pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[0].c_str()));
            if (pObj) {
                // Proceed only if visible state is set.
                string strValue = pObj->getString("VisibleState", "");
                if (!strValue.empty()) {
                    pObj->setVisible(isTRUE(strValue.c_str()));
                }
            }
        }

        pSkinWnd->invalidateRect();

        return ERR_OK;
    } else if (cmd.strFunction == "save_ctrl_visible_state") {
        VecStrings vStrParam;
        CUIObject *pObj;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);

            pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[i].c_str()));
            if (pObj) {
                pObj->writeInt("VisibleState", pObj->isVisible());
            }
        }

        return ERR_OK;
    } else if (cmd.strFunction == "toggle_ctrl_visible") {
        VecStrings vStrParam;
        CUIObject *pObj;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);
        }

        pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[0].c_str()));
        if (pObj) {
            bool bVisible;
            if (vStrParam.size() > 1) {
                bVisible = isTRUE(vStrParam[1].c_str());
            } else {
                bVisible = !pObj->isVisible();
            }
            pObj->setVisible(bVisible);
            pSkinWnd->invalidateRect();
            return ERR_OK;
        } else {
            return ERR_FALSE;
        }
    } else if (cmd.strFunction == "show_hide_ctrl") {
        VecStrings vStrParam;
        CUIObject *pObj;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        if (vStrParam.size() != 2) {
            return ERR_FALSE;
        }
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);
        }

        // show first
        pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[0].c_str()));
        if (pObj) {
            pObj->setVisible(true);
        }

        // hide second
        pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[1].c_str()));
        if (pObj) {
            pObj->setVisible(false);
        }
        pSkinWnd->invalidateRect();

        return ERR_OK;
    } else if (cmd.strFunction == "is_ctrl_visible") {
        CUIObject *pObj = pSkinWnd->getUIObjectById(getIDByName(cmd.strParam.c_str()));
        if (pObj && pObj->isVisible()) {
            return ERR_OK;
        }

        return ERR_FALSE;
    } else if (cmd.strFunction == "set_property") {
        // p1: property name, p2: value
        VecStrings vStrParam;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        if (vStrParam.size() != 2) {
            return ERR_FALSE;
        }
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);
        }

        pSkinWnd->setProperty(vStrParam[0].c_str(), vStrParam[1].c_str());

        pSkinWnd->invalidateRect();

        return ERR_OK;
    } else if (cmd.strFunction == "set_ctrl_property") {
        VecStrings vStrParam;
        CUIObject *pObj;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        if (vStrParam.size() != 3) {
            return ERR_FALSE;
        }
        trimStr(vStrParam);

        pObj = pSkinWnd->getUIObjectById(getIDByName(vStrParam[0].c_str()));
        if (pObj) {
            pObj->setProperty(vStrParam[1].c_str(), vStrParam[2].c_str());
        }

        return ERR_OK;
    } else if (cmd.strFunction == "start_animation") {
        pSkinWnd->startAnimation(getIDByName(cmd.strParam.c_str()));

        return ERR_OK;
    } else if (cmd.strFunction == "multi_cmd") {
        VecStrings vStrParam;
        int nRet = ERR_NOT_FOUND;

        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        for (int i = 0; i < (int)vStrParam.size(); i++) {
            trimStr(vStrParam[i]);
            nRet = onDynamicCmd(getIDByName(vStrParam[i].c_str()), pSkinWnd);
            if (nRet != ERR_OK) {
                ERR_LOG2("execute subcommand failed: %s, %s", vStrParam[i].c_str(), (cstr_t)Error2Str(nRet));
            }
        }
        return nRet;
    } else if (cmd.strFunction == "get_profile_bool") {
        VecStrings vStrParam;
        strSplit(cmd.strParam.c_str(), ',', vStrParam);
        trimStr(vStrParam);

        if (vStrParam.size() == 0) {
            return ERR_FALSE;
        }

        bool bDefault = true;
        if (vStrParam.size() > 1) {
            bDefault = isTRUE(vStrParam[1].c_str());
        }
        if (g_profile.getBool(getSkinName(), vStrParam[0].c_str(), bDefault)) {
            return ERR_OK;
        }

        return ERR_FALSE;
    }
    //    else if (cmd.strFunction == "open_skin_wnd")
    //        return activeOrCreateSkinWnd(false, _SZ_SKINWND_CLASS_NAME, cmd.strParam.c_str(), cmd.strParam.c_str());

    return ERR_NOT_FOUND;
}

void CSkinFactory::adjustHue(float hue, float saturation, float luminance) {
    // reload the resource which is set to adjust hue
    m_resourceMgr.adjustHue(hue, saturation, luminance);

    // Notify each child ctrl of this message.
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pSkin = *it;
        pSkin->onAdjustHue(hue, saturation, luminance);
        pSkin->invalidateRect();
    }
}

void CSkinFactory::getAdjustedHueResult(CColor &clr) {
    float H, S, L;
    COLORREF c;

    m_resourceMgr.getAdjustHueParam(H, S, L);
    if (H != 0) {
        c = clr.get();
        adjustColorHue(c, H);
        clr.set(c);
    }
}

void CSkinFactory::getAdjustedHueResult(COLORREF &clr) {
    float H, S, L;

    m_resourceMgr.getAdjustHueParam(H, S, L);
    if (H != 0) {
        adjustColorHue(clr, H);
    }
}

void CSkinFactory::onLanguageChanged() {
    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pWnd = *it;
        pWnd->onLanguageChanged();
    }
}

void CSkinFactory::applyDefaultThemeOfSkin() {
    string themeFile;
    if (!m_resourceMgr.getResourcePathName("theme.xml", themeFile)) {
        return;
    }

    CSimpleXML xml;
    if (!xml.parseFile(themeFile.c_str())) {
        ERR_LOG1("Failed to parse theme file: %s", themeFile.c_str());
        return;
    }

    if (strcmp(xml.m_pRoot->name.c_str(), "Theme") != 0) {
        return;
    }

    for (SXNode *nodeSection : xml.m_pRoot->listChildren) {
        for (SXNode *nodeValue : nodeSection->listChildren) {
            g_profile.writeString(nodeSection->name.c_str(),
                                  nodeValue->name.c_str(), nodeValue->strContent.c_str());
        }
    }
}

//
// Bring all the skin windows to the top
//
void CSkinFactory::onSkinWndActivate(CSkinWnd *pWndActive) {
#ifdef _WIN32
    ListSkinWnds::iterator it, itEnd;

    HWND hWndPrev, hWndNext, hWndActive;
    HDWP hWindowPosInfo;
    vector<HWND> vToBring;

    if (pWndActive) {
        hWndActive = pWndActive->getWndHandle();
    } else {
        hWndActive = GetForegroundWindow();
    }

    if (isTopmostWindow(hWndActive)) {
        vToBring.push_back(HWND_TOP);
    } else {
        vToBring.push_back(hWndActive);
    }

    hWndPrev = hWndNext = hWndActive;
    hWndNext = GetWindow(hWndNext, GW_HWNDNEXT);
    while (hWndNext) {
        itEnd = m_listSkinWnds.end();
        for (it = m_listSkinWnds.begin(); it != itEnd; ++it) {
            if ((*it)->getWndHandle() == hWndNext && hWndNext != hWndActive) {
                if (!isTopmostWindow(hWndNext) && !::IsIconic(hWndNext)) {
                    vToBring.push_back(hWndNext);
                    hWndPrev = hWndNext;
                }
                break;
            }
        }
        hWndNext = GetWindow(hWndNext, GW_HWNDNEXT);
    }
    hWindowPosInfo = BeginDeferWindowPos(vToBring.size());
    for (int i = 1; i < (int)vToBring.size(); i++) {
        DeferWindowPos(hWindowPosInfo, vToBring[i], vToBring[i - 1], 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    }
    EndDeferWindowPos(hWindowPosInfo);

    // DBG_LOG1("onSkinWndActivate: %s", pWndActive->getSkinWndName());
#endif
}

void CSkinFactory::enterInDrawUpdate() {
    for (auto it = m_listSkinWnds.begin(); it != m_listSkinWnds.end(); ++it) {
        CSkinWnd *pWnd = *it;
        pWnd->enterInDrawUpdate();
    }
}

void CSkinFactory::leaveInDrawUpdate() {
    for (auto it = m_listSkinWnds.begin(); it != m_listSkinWnds.end(); ++it) {
        CSkinWnd *pWnd = *it;
        pWnd->leaveInDrawUpdate();
    }
}
