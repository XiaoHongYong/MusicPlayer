#include "MPlayerApp.h"
#include "PreferPageAdvanced.h"
#include "DownloadMgr.h"
#include "../MediaTags/LrcParser.h"

#ifdef _WIN32
#include "win32/MLTrayIcon.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/MLTrayIcon.h"
#endif

#ifdef _MAC_OS
#include "mac/MLTrayIcon.h"
#endif

#define DURATION_SEARCH     500

#define SZ_YES            _TLT("Yes")
#define SZ_NO            _TLT("NO")
#define SZ_ENABLED        _TLT("Enabled")
#define SZ_DISABLED        _TLT("Disabled")
#define SZ_EDIT            _TLT("Edit...")
#define SZ_BROWSE        _TLT("Browse...")
#define SZ_RESET        _TLT("Reset selected settings")

#define MENUID_START        3000

string connectToLocalStr(cstr_t szStr1, cstr_t szStr2) {
    string str = _TL(szStr1);

    str += ": ";
    str += _TL(szStr2);

    return str;
}

class CPfItemRestoreLyrChgSavePrompt : public CPreferItem {
public:
    CPfItemRestoreLyrChgSavePrompt(Window *pWnd) : CPreferItem(connectToLocalStr(_TLM("UI"),
        _TLM("Restore saving prompt setting for lyrics changed")).c_str()) {
        m_pWnd = pWnd;
    }

    virtual string getValue() {
        return "";
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(_TLT("Restore"));
    }

    virtual void setOption(int nIndex) {
        g_profile.writeInt("SaveLyrPrompt", IDCANCEL);
        m_pWnd->messageOut(_TLT("Setting was restored successfully."), MB_OK | MB_ICONINFORMATION);
    }

    virtual bool isDefault() {
        return true;
    }

    virtual void reset() {
    }

    Window                      *m_pWnd;

};

class CPfItemInt : public CPreferItem {
public:
    CPfItemInt(cstr_t szName, EventType etNotify, cstr_t szSectName, cstr_t szValueName, int nDefault) : CPreferItem(szName) {
        eventType = etNotify;
        strSection = szSectName;
        strValueName = szValueName;
        nDefaultValue = nDefault;
    }

    virtual string getValue() {
        return std::to_string(getIntValue());
    }

    virtual int getIntValue() {
        return g_profile.getInt(strSection.c_str(), strValueName.c_str(), nDefaultValue);
    }

    virtual void setIntValue(int value) {
        CMPlayerSettings::setSettings(eventType, strSection.c_str(),
            strValueName.c_str(), value, eventType != ET_INVALID);
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(SZ_EDIT);
    }

    virtual void setOption(int nIndex) {
        assert(nIndex == 0);
    }

    virtual bool isDefault() {
        return getIntValue() == nDefaultValue;
    }

    virtual void reset() {
        setIntValue(nDefaultValue);
    }

    EventType                   eventType;
    string                      strSection;
    string                      strValueName;
    int                         nDefaultValue;

};


class CPfItemBool : public CPfItemInt {
public:
    CPfItemBool(cstr_t szName, EventType etNotify, cstr_t szSectName, cstr_t szValueName, bool bDefault)
    : CPfItemInt(szName, etNotify, szSectName, szValueName, bDefault) {
    }

    virtual string getValue() {
        if (getIntValue()) {
            return SZ_YES;
        } else {
            return SZ_NO;
        }
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(SZ_YES);
        vString.push_back(SZ_NO);

        if (getIntValue()) {
            nRadio = 0;
        } else {
            nRadio = 1;
        }
    }

    virtual void setOption(int nIndex) {
        setIntValue(nIndex == 0);
    }

};

class CPfItemBoolEnableAutoDlLyr : public CPfItemBool {
public:
    CPfItemBoolEnableAutoDlLyr()
    : CPfItemBool(connectToLocalStr(_TLM("Lyrics Download"),
        _TLM("Auto download lyrics")).c_str(), ET_NULL, SZ_SECT_LYR_DL,
        "EnableAuoDownload", true) {
    }

    virtual void setIntValue(int value) {
        g_LyricsDownloader.m_bAutoDownload = tobool(value);
        CPfItemBool::setIntValue(value);
    }

};

class CPfItemTitleFilter : public CPreferItem {
public:
    CPfItemTitleFilter(Window *pWnd) : CPreferItem(connectToLocalStr(_TLM("Lyrics Download"),
        _TLM("Filter radio station names from artist and title")).c_str()) {
        m_pWnd = pWnd;
    }

    virtual string getValue() {
        return "";
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(_TLT("Edit..."));
    }

    virtual void setOption(int nIndex) {
        string strFilterFile = g_player.getTitleFilterFile();
        if (!isFileExist(strFilterFile.c_str())) {
            writeFile(strFilterFile.c_str(), "; Enter the Radio station name filters below, one name per line.\r\n; Please save this file, and restart MusicPlayer.\r\n");
        }

        string strEditor;
        getNotepadEditor(strEditor);
        execute(m_pWnd,
            strEditor.c_str(),
            strFilterFile.c_str());
    }

    virtual bool isDefault() {
        return true;
    }

    virtual void reset() {
    }

    Window                      *m_pWnd;

};

class CPfItemLyrSearchFolder : public CPreferItem {
public:
    CPfItemLyrSearchFolder(Window *pWndParent)
    : CPreferItem(connectToLocalStr(_TLM("Lyrics"), _TLM("Lyrics search folder")).c_str()) {
        m_pWndParent = pWndParent;
    }

    virtual string getValue() {
        string strValue;

        for (int i = 0; i < g_LyricSearch.getSearchFolerCount(); i++) {
            strValue += g_LyricSearch.getFolder(i);
            strValue += "; ";
        }

        return strValue;
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(_TLT("Add &Folder..."));

        if (g_LyricSearch.getSearchFolerCount() > 0) {
            vString.push_back("");
        }

        for (int i = 0; i < g_LyricSearch.getSearchFolerCount(); i++) {
            vString.push_back(string(_TLT("Remove folder:")) + " " + g_LyricSearch.getFolder(i));
        }
    }

    virtual void setOption(int nIndex) {
        if (nIndex == 0) {
            // add new folder:
            // 取得上次选择过的文件夹的位置
            string path = g_profile.getString("LastSelectFolder", "");
            CFolderDialog dlg(path.c_str());

            if (dlg.doBrowse(m_pWndParent)) {
                // 保存上次选择过的文件夹的位置
                path = dlg.getFolder();
                g_profile.writeString("LastSelectFolder", path.c_str());

                if (g_LyricSearch.setFolder(path.c_str())) {
                    // update lyric
                    MPlayerApp::getInstance()->dispatchResearchLyrics();
                    g_LyricSearch.saveLyricFolderCfg();
                }
            }
        } else {
            // remove search folder
            nIndex--;
            if (nIndex >= g_LyricSearch.getSearchFolerCount()) {
                assert(0);
                return;
            }
            g_LyricSearch.removeFolder(nIndex);
            g_LyricSearch.saveLyricFolderCfg();

            MPlayerApp::getInstance()->dispatchResearchLyrics();
        }
    }

    virtual bool isDefault() {
        return g_LyricSearch.getSearchFolerCount() == 0;
    }

    virtual void reset() {
        for (int i = 0; i < g_LyricSearch.getSearchFolerCount(); i++) {
            g_LyricSearch.removeFolder(0);
        }
        g_LyricSearch.saveLyricFolderCfg();
    }

    Window                      *m_pWndParent;

};

CharEncodingType getDefaultLyricsEncodingSettings() {
    string defEncoding = g_profile.getString("LyrDefEncoding", "");
    CharEncodingType encodingId = getCharEncodingID(defEncoding.c_str());
    return encodingId;
}

void setDefaultLyricsEncodingSettings(CharEncodingType encoding) {
    setDefaultLyricsEncoding(encoding);

    g_profile.writeString("LyrDefEncoding", getCharEncodingByID(encoding).szEncoding);
}

class CPfItemLyrDefaultEncoding : public CPreferItem {
public:
    CPfItemLyrDefaultEncoding()
    : CPreferItem(connectToLocalStr(_TLM("Lyrics"), _TLM("Default encoding of lyrics")).c_str()) {
    }

    virtual string getValue() {
        return _TL(getCharEncodingByID(getDefaultLyricsEncodingSettings()).szDesc);
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        int count = getCharEncodingCount();
        for (int i = 0; i < count; i++) {
            vString.push_back(_TL(getCharEncodingByID((CharEncodingType)i).szDesc));
        }

        nRadio = getDefaultLyricsEncodingSettings();
    }

    virtual void setOption(int nIndex) {
        setDefaultLyricsEncodingSettings((CharEncodingType)nIndex);
    }

    virtual bool isDefault() {
        return getDefaultLyricsEncodingSettings() == ED_SYSDEF;
    }

    virtual void reset() {
        setDefaultLyricsEncodingSettings(ED_SYSDEF);
    }

};

class CPfItemLyrExternalEditor : public CPreferItem {
public:
    CPfItemLyrExternalEditor(Window *pWndParent)
    : CPreferItem(connectToLocalStr(_TLM("Lyrics"), _TLM("External Lyrics Editor")).c_str()) {
        m_pWndParent = pWndParent;
    }

    virtual string getValue() {
        string strEditor;
        getNotepadEditor(strEditor);
        return CMLProfile::getDir(SZ_SECT_UI, "LyricsEditor", strEditor.c_str()).c_str();
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(_TL(SZ_BROWSE));
    }

    virtual void setOption(int nIndex) {
        assert(nIndex == 0);
        string strEditor;

        getNotepadEditor(strEditor);

        cstr_t szExt = "";
#ifdef _WIN32
        szExt = "Executable file(*.exe)\0*.exe\0\0";
#endif
#ifdef _MAC_OS
        szExt = "Executable file(*.app)\0*.app\0\0";
#endif
        CFileOpenDlg dlg(_TLT("Browse external lyrics editor program"), strEditor.c_str(), szExt, 1);

        if (dlg.doModal(m_pWndParent) == IDOK) {
            CMLProfile::writeDir(SZ_SECT_UI, "LyricsEditor", dlg.getOpenFile());
        }
    }

    virtual bool isDefault() {
        string strEditor;
        getNotepadEditor(strEditor);
        return strcmp(strEditor.c_str(), getValue().c_str()) == 0;
    }

    virtual void reset() {
        string strEditor;

        getNotepadEditor(strEditor);
        CMLProfile::writeDir(SZ_SECT_UI, "LyricsEditor", strEditor.c_str());
    }

    Window                      *m_pWndParent;

};

#ifdef _MAC_OS
cstr_t SYSTEM_TRAY_TITLE = _TLM("System menu bar control");
#else
cstr_t SYSTEM_TRAY_TITLE = _TLM("System tray icon player control");
#endif

class CPfItemTrayIconPlayerCtrl : public CPfItemBool {
public:
    CPfItemTrayIconPlayerCtrl(cstr_t szName, int nPlayerCtrlIndex, bool defVal)
    : CPfItemBool((string(_TL(SYSTEM_TRAY_TITLE)) + ": " + _TL(szName)).c_str(),
        ET_NULL, SZ_SECT_UI, "", defVal) {
        strValueName = stringPrintf("TrayIcon%d", nPlayerCtrlIndex);

        this->nPlayerCtrlIndex = nPlayerCtrlIndex;
    }

    int getIntValue() override {
        return g_sysTrayIconCmd[nPlayerCtrlIndex].isEnabled;
    }

    void setIntValue(int value) override {
        g_profile.writeInt(strValueName.c_str(), value);

        g_sysTrayIconCmd[nPlayerCtrlIndex].isEnabled = tobool(value);

        MPlayerApp::getMainWnd()->updatePlayerSysTrayIcon();
    }

    int                         nPlayerCtrlIndex;

};


#ifdef _WIN32
class CPfItemWndOpacity : public CPfItemInt {
public:
    CPfItemWndOpacity() : CPfItemInt(connectToLocalStr(_TLM("UI"), _TLM("Window opaque percent (10 - 100)")).c_str(),
        ET_NULL, SZ_SECT_UI, "WindowOpaquePercent", 100) { }

    int getIntValue() {
        int nOpaque = CPfItemInt::getIntValue();
        if (nOpaque < 10) {
            nOpaque = 10;
        } else if (nOpaque > 100) {
            nOpaque = 100;
        }
        return nOpaque / 10 * 10;
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        for (int i = 10; i <= 100; i += 10) {
            vString.push_back(itos(i));
        }

        nRadio = getIntValue() / 10 - 1;
    }

    virtual void setOption(int nIndex) {
        setIntValue((nIndex + 1) * 10);
    }

    virtual void setIntValue(int value) {
        CPfItemInt::setIntValue(value);
        MPlayerApp::getMPSkinFactory()->allUpdateTransparent();
    }

};

cstr_t g_szSysTrayOpt[] = { _TLM("&Taskbar only"), _TLM("S&ystem tray only"), _TLM("T&askbar and system tray"), "&None" };

class CPfItemSystemTrayIcon : public CPfItemInt {
public:
    CPfItemSystemTrayIcon()
    : CPfItemInt(_TLM("show $Product$ in System tray and taskbar"),
        ET_UI_SETTINGS_CHANGED, SZ_SECT_UI, "ShowIconOn", SHOW_ICON_ON_TASKBAR) {
    }

    virtual string getValue() {
        int n = getIntValue();
        if (n < 0 || n >= CountOf(g_szSysTrayOpt)) {
            n = SHOW_ICON_ON_TASKBAR;
        }
        return removePrefixOfAcckey(_TL(g_szSysTrayOpt[n]));
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        for (int i = 0; i < CountOf(g_szSysTrayOpt); i++) {
            vString.push_back(_TL(g_szSysTrayOpt[i]));
        }

        nRadio = getIntValue();
    }

    virtual void setOption(int nIndex) {
        setIntValue(nIndex);
    }

};

#endif // #ifdef _WIN32

class CPfItemLyrLineSpacing : public CPfItemInt {
public:
    CPfItemLyrLineSpacing() : CPfItemInt(connectToLocalStr(_TLM("Lyrics Display"), _TLM("Lyrics line spacing")).c_str(),
        ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "LineSpacing", 2) { }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        for (int i = 0; i <= 10; i++) {
            vString.push_back(itos(i));
        }

        nRadio = getIntValue();
    }

    virtual void setOption(int nIndex) {
        setIntValue(nIndex);
    }

};

static string getCmdIDDescription(int nCmd) {
    string strTooltip = MPlayerApp::getMPSkinFactory()->getTooltip(nCmd);
    if (strTooltip.empty()) {
        return "";
    }

    MPHotKeySection *pSect = g_vHotkeySections;
    for (; pSect->szName != nullptr; pSect++) {
        for (int k = 0; pSect->vHotkeys[k] != 0; k++) {
            if (pSect->vHotkeys[k] == nCmd) {
                return string(_TL(pSect->szName)) + ": " + _TL(strTooltip.c_str());
            }
        }
    }

    return strTooltip;
}

class DlgShortcutKey : public CSkinWnd {
public:
    DlgShortcutKey(uint32_t cmd, bool isGlobal) : cmd(cmd), isGlobal(isGlobal) {
    }

    void onCreate() override {
        CSkinWnd::onCreate();

        GET_ID_BY_NAME(CID_E_NAME);

        MPlayerApp::getHotkey().enableGlobalHotkey(false);
    }

    void onCommand(uint32_t nId) override {
        if (nId != ID_OK) {
            return CSkinWnd::onCommand(nId);
        }

        auto &hotKeys = MPlayerApp::getHotkey();

        // Is this hotkey already being used?
        int usedCmdIndex = hotKeys.getByKey(key, modifiers);
        if (usedCmdIndex != -1) {
            // Same one is self?
            if (hotKeys.get(usedCmdIndex)->cmd == cmd) {
                return CSkinWnd::onCommand(nId);
            }

            auto strCmd = getCmdIDDescription(hotKeys.get(usedCmdIndex)->cmd);

            auto strMsg = stringPrintf(_TLT("This hotkey is currently used by action: %s."), _TL(strCmd.c_str()));
            strMsg += "\r\n";
            strMsg += _TLT("Do you want to replace it?");
            if (messageOut(strMsg.c_str(), MB_ICONINFORMATION | MB_YESNO) != IDYES) {
                return;
            }

            hotKeys.remove(usedCmdIndex);
        }

        hotKeys.add(cmd, isGlobal, key, modifiers);

        if (isGlobal) {
            hotKeys.enableGlobalHotkey(true);
        }

        CSkinWnd::onCommand(nId);
    }

    bool onKeyDown(uint32_t nChar, uint32_t nFlags) override {
        auto obj = getFocusUIObj();
        if (obj && obj->getID() == CID_E_NAME) {
            key = nChar;
            modifiers = nFlags;
            auto text = formatHotkeyText(key, modifiers);
            obj->setText(text.c_str());
            obj->invalidate();
            return true;
        }

        return CSkinWnd::onKeyDown(nChar, nFlags);
    }

    // Do nothing for text input mode.
    void startTextInput() override { }

    int                         CID_E_NAME = -1;
    bool                        isGlobal = false;
    uint32_t                    cmd = 0, key = 0, modifiers = 0;

};

void showShortcutKeyDialog(CSkinWnd *parent, uint32_t cmd, bool isGlobal) {

    SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, _SZ_SKINWND_CLASS_NAME,
        "DlgShortCutKey.xml", parent);

    auto *window = new DlgShortcutKey(cmd, isGlobal);
    skinWndStartupInfo.pSkinWnd = window;

    CSkinApp::getInstance()->getSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
}

class CPfItemBoolEnableGlobalHotkey : public CPfItemBool {
public:
    CPfItemBoolEnableGlobalHotkey()
    : CPfItemBool(removePrefixOfAcckey(connectToLocalStr(_TLM("Shortcut"), _TLM("&Enable Global Hotkeys")).c_str()).c_str(),
        ET_NULL, SZ_APP_NAME, "enableGlobalHotkey", false) {
    }

    virtual void setIntValue(int value) {
        MPlayerApp::getHotkey().enableGlobalHotkey(tobool(value));
        CPfItemBool::setIntValue(value);
    }

};

class CPfItemShortcut : public CPreferItem {
public:
    struct Hotkey {
        bool                        bGlobal;
        string                      strHotkey;
    };
    typedef vector<Hotkey>    VecHotkeys;

    CPfItemShortcut(CPagePfAdvanced *pWndParent, cstr_t szName, int nCmd)
    : CPreferItem(szName) {
        m_nCmd = nCmd;
        m_pWndParent = pWndParent;
    }

    virtual string getValue() {
        string strValue;
        VecHotkeys vHotkeys;

        enumAll(vHotkeys);

        for (size_t i = 0; i < vHotkeys.size(); i++) {
            if (!strValue.empty()) {
                strValue.append("; ");
            }
            if (vHotkeys[i].bGlobal) {
                strValue += _TLT("Global:");
                strValue += " ";
            }
            strValue += vHotkeys[i].strHotkey;
        }

        return strValue;
    }

    void enumAll(VecHotkeys &vHotkeys) {
        int nIndex = -1;
        CMPHotkey::CmdAccKey *pCmdKey;
        while (1) {
            nIndex = MPlayerApp::getHotkey().getByCmd(m_nCmd, nIndex);
            if (nIndex == -1) {
                break;
            }

            pCmdKey = MPlayerApp::getHotkey().get(nIndex);
            if (pCmdKey) {
                Hotkey hotkey;
                hotkey.strHotkey = formatHotkeyText(pCmdKey->button, pCmdKey->fsModifiers);
                hotkey.bGlobal = pCmdKey->bGlobal;
                vHotkeys.push_back(hotkey);
            }
        }
    }

    virtual void getOptions(VecStrings &vString, int &nRadio) {
        vString.push_back(_TLT("Add shortcut key..."));
        vString.push_back(_TLT("Add global hotkey..."));

        VecHotkeys vHotkeys;

        enumAll(vHotkeys);

        if (vHotkeys.size()) {
            vString.push_back("");
        }

        for (size_t i = 0; i < vHotkeys.size(); i++) {
            vString.push_back(
                string(_TL(vHotkeys[i].bGlobal ? "remove Global Hotkey:" : "remove Shortcut Key:"))
                + " " + vHotkeys[i].strHotkey);
        }
    }

    virtual void setOption(int nIndex) {
        if (nIndex == 0 || nIndex == 1) {
            // add new shortcut key:
            bool isGloal = (nIndex == 1);
            showShortcutKeyDialog(m_pWndParent->getSkinWnd(), m_nCmd, isGloal);
        } else {
            // remove shortcut key
            int nShortcutIndex = -1;
            for (int i = 2; i <= nIndex; i++) {
                nShortcutIndex = MPlayerApp::getHotkey().getByCmd(m_nCmd, nShortcutIndex);
                if (nShortcutIndex == -1) {
                    break;
                }

                if (i == nIndex) {
                    MPlayerApp::getHotkey().remove(nShortcutIndex);
                }
            }
        }
    }

    virtual bool isDefault() {
        return MPlayerApp::getHotkey().isDefaultKey(m_nCmd);
    }

    virtual void reset() {
        return MPlayerApp::getHotkey().restoreDefaultKey(m_nCmd);
    }

    CPagePfAdvanced             *m_pWndParent;
    int                         m_nCmd;

};

//////////////////////////////////////////////////////////////////////////

UIOBJECT_CLASS_NAME_IMP(CPagePfAdvanced, "PreferPage.Advanced")

CPagePfAdvanced::CPagePfAdvanced() : CPagePfBase(PAGE_ADVANCED, "ID_ROOT_ADVANCED") {
    m_nMenuIdEnd = m_nMenuIdStart = 0;
    m_bIgnoreSettingListNotify = false;
    CID_LIST_SETTINGS = 0;
    CID_E_ADV_SEARCH = 0;
    m_nTimerIdSearch = 0;
}

CPagePfAdvanced::~CPagePfAdvanced() {
    for (size_t i = 0; i < m_vPreferItems.size(); i++) {
        delete m_vPreferItems[i];
    }
    m_vPreferItems.clear();
}

void CPagePfAdvanced::onInitialUpdate() {
    CPagePfBase::onInitialUpdate();

    GET_ID_BY_NAME(CID_LIST_SETTINGS);
    GET_ID_BY_NAME(CID_E_ADV_SEARCH);

    m_pListItems = (CSkinListCtrl *)getUIObjectById(CID_LIST_SETTINGS, CSkinListCtrl::className());
    assert(m_pListItems);
    if (!m_pListItems) {
        return;
    }

    m_pSkin->registerUIObjNotifyHandler(CID_LIST_SETTINGS, this);

    m_pListItems->addColumn(_TLT("Option"), 450);
    m_pListItems->addColumn(_TLT("Value"), 150);

#ifdef _WIN32
    m_vPreferItems.push_back(new CPfItemWndOpacity());
#endif

    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("UI"), _TLM("Auto adjust window width with lyrics")).c_str(), ET_NULL, SZ_SECT_UI, "AutoAdjustWndWidth", false));
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("UI"), _TLM("Auto adjust window height with lyrics")).c_str(), ET_NULL, SZ_SECT_UI, "AutoAdjustWndHeight", true));
    m_vPreferItems.push_back(new CPfItemRestoreLyrChgSavePrompt(m_pSkin));
    // m_vPreferItems.push_back(new CPfItemBool("Hide the link of rating lyrics", ET_NULL, SZ_SECT_UI, "HideRateLink", true));
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("Lyrics Display"), _TLM("Allow adjusting the vertical position of lyrics")).c_str(),
        ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY,
        "enableAdjustVertAlign", false));
    m_vPreferItems.push_back(new CPfItemLyrLineSpacing());

    //
    // Lyrics
    //
    // m_vPreferItems.push_back(new CPfItemBool("", ));
    m_vPreferItems.push_back(new CPfItemBoolEnableAutoDlLyr());
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("Lyrics Download"), _TLM("Only download synchronized lyrics with .lrc extension")).c_str(), ET_NULL, SZ_SECT_LYR_DL, "OnlyDlSyncLyr", false));
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("Lyrics Download"), _TLM("Try to download synchronized lyrics (.lrc) even if local unsynchronized (.txt) lyrics exist")).c_str(), ET_NULL, SZ_SECT_LYR_DL, "DownLrcEvenIfHasTxt", true));
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("Lyrics Download"), _TLM("Pop up lyrics search dialog when lyrics are found")).c_str(), ET_NULL, SZ_SECT_LYR_DL, "DownLrcUserSelect", false));
    m_vPreferItems.push_back(new CPfItemBool(connectToLocalStr(_TLM("Lyrics Download"), _TLM("Save downloaded lyrics in lettered subfolders")).c_str(), ET_NULL, SZ_SECT_LYR_DL, "DownSaveByABC", false));
    m_vPreferItems.push_back(new CPfItemTitleFilter(m_pSkin));
    m_vPreferItems.push_back(new CPfItemLyrSearchFolder(m_pSkin));
    m_vPreferItems.push_back(new CPfItemLyrExternalEditor(m_pSkin));
    m_vPreferItems.push_back(new CPfItemLyrDefaultEncoding());

    //
    // Tray Icon controls
    //
    for (int i = 0; i < MAX_PLAYER_TRAY_ICON_CMD; i++) {
        m_vPreferItems.push_back(new CPfItemTrayIconPlayerCtrl(g_sysTrayIconCmd[i].cmdText, i, g_sysTrayIconCmd[i].isEnabled));
    }

#ifdef _WIN32
    m_vPreferItems.push_back(new CPfItemSystemTrayIcon());
#endif

    m_vPreferItems.push_back(new CPfItemBoolEnableGlobalHotkey());

    //
    // Shortcut keys
    //
    MPHotKeySection *pSect = g_vHotkeySections;
    for (; pSect->szName != nullptr; pSect++) {
        for (int k = 0; pSect->vHotkeys[k] != 0; k++) {
            string strTooltip = MPlayerApp::getMPSkinFactory()->getTooltip(pSect->vHotkeys[k]);
            if (strTooltip.empty()) {
                continue;
            }

            m_vPreferItems.push_back(new CPfItemShortcut(
                this,
                (string(_TLT("Shortcut")) + ": "
                + _TL(pSect->szName) + ": " + _TL(strTooltip.c_str())).c_str(),
                pSect->vHotkeys[k]));
        }
    }

    listAllItems();

    CSkinEditCtrl *pEdit = (CSkinEditCtrl *)getUIObjectById(CID_E_ADV_SEARCH, CSkinEditCtrl::className());
    assert(pEdit);
    if (pEdit) {
        pEdit->setEditNotification(this);
    }
}

void CPagePfAdvanced::onDestroy() {
    m_pSkin->unregisterUIObjNotifyHandler(this);

    CPagePfBase::onDestroy();
}

void CPagePfAdvanced::onUIObjNotify(IUIObjNotify *pNotify) {
    if (pNotify->nID == CID_LIST_SETTINGS && pNotify->isKindOf(CSkinListCtrl::className())) {
        if (m_bIgnoreSettingListNotify) {
            return;
        }

        CSkinListCtrlEventNotify *pListCtrlNotify = (CSkinListCtrlEventNotify*)pNotify;
        if (pListCtrlNotify->cmd == CSkinListCtrlEventNotify::C_SEL_CHANGED) {
            enableUIObject(getIDByName("CID_CUSTOMIZE"), m_pListItems->getNextSelectedItem() != -1);
        } else if (pListCtrlNotify->cmd == CSkinListCtrlEventNotify::C_DBL_CLICK
            || pListCtrlNotify->cmd == CSkinListCtrlEventNotify::C_RBTN_CLICK) {
            CPoint pt = getCursorPos();
            showCustomizeMenu(pt.x, pt.y);
        }
    }
}

// Edit control notification
void CPagePfAdvanced::onEditorTextChanged() {
    if (m_nTimerIdSearch != 0) {
        m_pSkin->unregisterTimerObject(this, m_nTimerIdSearch);
    }
    m_nTimerIdSearch = m_pSkin->registerTimerObject(this, DURATION_SEARCH);
}

void CPagePfAdvanced::onTimer(int nId) {
    if (nId != m_nTimerIdSearch) {
        return;
    }

    m_pSkin->unregisterTimerObject(this, m_nTimerIdSearch);
    m_nTimerIdSearch = 0;

    string strKey;

    strKey = getUIObjectText(CID_E_ADV_SEARCH);
    if (strcasecmp(strKey.c_str(), m_strLastSearch.c_str()) == 0) {
        return;
    }

    m_strLastSearch = strKey;

    VecStrings vKeyWords;
    strSplit(strKey.c_str(), ' ', vKeyWords);
    trimStr(vKeyWords, ' ');
    if (vKeyWords.empty() || (vKeyWords.size() == 1 && vKeyWords[0].empty())) {
        listAllItems();
        return;
    }

    m_pListItems->deleteAllItems();
    m_vFilteredItems.clear();

    // List results.
    for (int i = 0; i < (int)m_vPreferItems.size(); i++) {
        CPreferItem *item = m_vPreferItems[i];
        bool bMatch = true;

        for (int k = 0; k < (int)vKeyWords.size(); k++) {
            if (!stristr(_TL(item->m_strName.c_str()), vKeyWords[k].c_str())) {
                bMatch = false;
                break;
            }
        }

        if (bMatch) {
            m_vFilteredItems.push_back(item);
            int n = m_pListItems->insertItem(i, _TL(item->m_strName.c_str()), item->isDefault() ? IMG_UNMODIFIED : IMG_MODIFIED, 0, false);
            m_pListItems->setItemText(n, 1, item->getValue().c_str(), false);
        }
    }

    m_pListItems->invalidate();
}

bool CPagePfAdvanced::onCommand(uint32_t nId) {
    if (nId == getIDByName("CID_CUSTOMIZE")) {
        CRect rc;

        getUIObjectRect(nId, rc);
        m_pSkin->clientToScreen(rc);

        showCustomizeMenu(rc.left, rc.top);
    } else if ((int)nId >= m_nMenuIdStart && (int)nId <= m_nMenuIdEnd) {
        if (nId == m_nMenuIdEnd) {
            // reset selected settings
            int nSel = -1;
            while (1) {
                nSel = m_pListItems->getNextSelectedItem(nSel);
                if (nSel >= 0 && nSel < (int)m_vFilteredItems.size()) {
                    CPreferItem *item = m_vFilteredItems[nSel];
                    item->reset();
                    m_pListItems->setItemText(nSel, 1, item->getValue().c_str(), false);
                    m_pListItems->setItemImageIndex(nSel, item->isDefault() ? IMG_UNMODIFIED : IMG_MODIFIED, false);
                } else {
                    break;
                }
            }
        } else {
            // set option.
            CPreferItem *item = getCurItem();
            if (!item) {
                return true;
            }

            item->setOption(nId - m_nMenuIdStart);

            int nSel = m_pListItems->getNextSelectedItem();
            m_pListItems->setItemText(nSel, 1, item->getValue().c_str(), false);

            m_pListItems->setItemImageIndex(nSel, item->isDefault() ? IMG_UNMODIFIED : IMG_MODIFIED, false);
        }

        m_pListItems->invalidate();

        return true;
    } else {
        return CPagePfBase::onCommand(nId);
    }

    return true;
}

void CPagePfAdvanced::updateValues() {
    for (int i = 0; i < (int)m_vFilteredItems.size(); i++) {
        CPreferItem *item = m_vFilteredItems[i];
        m_pListItems->setItemText(i, 1, item->getValue().c_str(), false);
        m_pListItems->setItemImageIndex(i, item->isDefault() ? IMG_UNMODIFIED : IMG_MODIFIED, false);
    }

    m_pListItems->invalidate();
}

CPreferItem *CPagePfAdvanced::getCurItem() {
    int nSel = 0;

    nSel = m_pListItems->getNextSelectedItem();
    if (nSel < 0 || nSel >= (int)m_vFilteredItems.size()) {
        return nullptr;
    }

    return m_vFilteredItems[nSel];
}

void CPagePfAdvanced::showCustomizeMenu(int x, int y) {
    CMenu menu;
    VecStrings options;
    int nRadio = -1;
    bool bEnabeReset = false;

    if (m_pListItems->getSelectedCount() == 0) {
        return;
    }

    m_nMenuIdStart = MENUID_START;
    m_nMenuIdEnd = MENUID_START;

    menu.createPopupMenu();

    if (m_pListItems->getSelectedCount() == 1) {
        CPreferItem *item = getCurItem();
        if (!item) {
            return;
        }

        item->getOptions(options, nRadio);
        for (size_t i = 0; i < options.size(); i++) {
            if (options[i].empty()) {
                menu.appendSeperator();
            } else {
                menu.appendItem(m_nMenuIdEnd++, options[i].c_str());
            }
        }
        menu.appendSeperator();
        if (!item->isDefault()) {
            bEnabeReset = true;
        }
    } else {
        int nSel = -1;

        while (1) {
            nSel = m_pListItems->getNextSelectedItem(nSel);
            if (nSel >= 0 && nSel < (int)m_vFilteredItems.size()) {
                CPreferItem *item = m_vFilteredItems[nSel];
                if (!item->isDefault()) {
                    bEnabeReset = true;
                }
            } else {
                break;
            }
        }
    }

    menu.appendItem(m_nMenuIdEnd, SZ_RESET);
    if (!bEnabeReset) {
        menu.enableItem(m_nMenuIdEnd, bEnabeReset);
    }

    if (nRadio != -1) {
        menu.checkRadioItem(m_nMenuIdStart + nRadio, m_nMenuIdStart, m_nMenuIdEnd - 1);
    }

    menu.trackPopupMenu(x, y, m_pSkin);
}

void CPagePfAdvanced::listAllItems() {
    m_pListItems->deleteAllItems();

    m_vFilteredItems = m_vPreferItems;

    for (int i = 0; i < (int)m_vPreferItems.size(); i++) {
        CPreferItem *item = m_vPreferItems[i];
        int n = m_pListItems->insertItem(i, _TL(item->m_strName.c_str()), item->isDefault() ? IMG_UNMODIFIED : IMG_MODIFIED, 0, false);
        m_pListItems->setItemText(n, 1, item->getValue().c_str(), false);
    }

    m_pListItems->invalidate();
}
