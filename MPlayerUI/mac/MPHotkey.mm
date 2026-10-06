#import <Carbon/Carbon.h>
#import "MediaKeyController.h"
#import "../MPlayerApp.h"
#import "../MLCmd.h"
#import "MPHotkey.h"


OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData) {
    EventHotKeyID eventHotkeyID;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(eventHotkeyID), NULL, &eventHotkeyID);

    DBG_LOG1("hotKeyHandler: %d\n", eventHotkeyID.id);

    CMPHotkey *hotKey = (CMPHotkey *) userData;
    hotKey->onHotKey(eventHotkeyID.id, 0, 0);
    return noErr;
}

int g_hcPlayback[] = { ID_PLAY, ID_PLAYPAUSE, ID_STOP, ID_PREVIOUS, ID_NEXT, ID_FORWARD, ID_BACKWARD, ID_J_PREV_LINE, ID_J_NEXT_LINE, 0 };
int g_hcGeneral[] = { ID_HELP, ID_QUIT, ID_CLOSE, ID_MENU, ID_PREFERENCES, ID_DISPLAY_OPT, 0 };
int g_hcUI[] = { ID_FONT_SIZE_INC, ID_FONT_SIZE_DEC, ID_CLR_PREV_HUE, ID_CLR_NEXT_HUE, ID_TOGGLE_MP, ID_MINIMIZE, ID_TOPMOST, ID_CLICK_THROUGH, 0 };
int g_hcLyrics[] = { ID_OPEN_LRC, ID_BACKWARD_LYRICS, ID_FORWARD_LYRICS, ID_RELOAD_LYR, 0 };
int g_hcLyrEditor[] = { ID_LYR_EDITOR, ID_NEW_LRC, ID_SAVE_LRC, ID_SAVE_LRC_AS, ID_SAVE_LYR_IN_SONG_FILE, ID_INSERTTAG_DOWN, ID_INSERTTAG, ID_DEL_TAG, ID_AUTO_FILL_LYR_INFO, ID_JUMP_TO_CUR_LINE, ID_FORWARD_CUR_LINE, ID_BACKWARD_CUR_LINE, ID_FORWARD_REMAIN_LINES, ID_BACKWARD_REMAIN_LINES, ID_REMOVE_ALL_TAG, ID_REMOVE_BLANK_LINE, ID_TRIM_WHITESPACE, ID_REMOVE_UNSYNC_LINES, ID_CAPITALIZE_LEADING_LETTER, ID_LYRICS_TO_LOWERCASE, 0 };

MPHotKeySection        g_vHotkeySections[] = {
    { _TLM("Playback"), g_hcPlayback },
    { _TLM("General"), g_hcGeneral },
    { _TLM("UI"), g_hcUI },
    { _TLM("Lyrics"), g_hcLyrics },
    { _TLM("Lyrics Editor"), g_hcLyrEditor },
    { nullptr, nullptr }
};

#ifndef VK_MEDIA_NEXT_TRACK
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_MEDIA_STOP       0xB2
#define VK_MEDIA_PLAY_PAUSE 0xB3
#endif


CMPHotkey::CmdAccKey            g_vDefAccKey[] = {
    { ID_PLAYPAUSE, true, VK_MEDIA_PLAY_PAUSE, 0, 0 },
    { ID_STOP, true, VK_MEDIA_STOP, 0, 0 },
    { ID_NEXT, true, VK_MEDIA_NEXT_TRACK, 0, 0 },
    { ID_PREVIOUS, true, VK_MEDIA_PREV_TRACK, 0, 0 },

    { ID_BACKWARD_LYRICS, false, VK_UP, MK_CONTROL | MK_SHIFT, 0 },
    { ID_FORWARD_LYRICS, false, VK_DOWN, MK_CONTROL | MK_SHIFT, 0 },
    { ID_TOPMOST, true, VK_T, MK_CONTROL | MK_SHIFT, 0 },
    { ID_TOGGLE_MP, true, VK_M, MK_CONTROL | MK_ALT, 0 },

    { ID_HELP, false, VK_F1, 0, 0 },

    { ID_OPEN_LRC, false, VK_D, MK_CONTROL, 0 },
    { ID_RELOAD_LYR, false, VK_F5, 0, 0 },

    { ID_LYR_EDITOR, false, VK_E, MK_CONTROL, 0 },
    { ID_NEW_LRC, false, VK_N, MK_CONTROL, 0 },
    { ID_OPEN_LRC, false, VK_O, MK_CONTROL, 0 },
    { ID_SAVE_LRC, false, VK_S, MK_CONTROL, 0 },
    { ID_JUMP_TO_CUR_LINE, false, VK_J, MK_CONTROL, 0 },
    { ID_FORWARD_REMAIN_LINES, false, VK_F11, 0, 0 },
    { ID_BACKWARD_REMAIN_LINES, false, VK_F12, 0, 0 },
    { ID_INSERTTAG_DOWN, false, VK_F7, 0, 0 },
    { ID_INSERTTAG, false, VK_F7, MK_CONTROL, 0 },
    { ID_FORWARD_CUR_LINE, false, VK_ADD, MK_CONTROL, 0 },
    { ID_BACKWARD_CUR_LINE, false, VK_SUBTRACT, MK_CONTROL, 0 },

    { ID_FORWARD, false, VK_NEXT, MK_CONTROL, 0 },
    { ID_BACKWARD, false, VK_PRIOR, MK_CONTROL, 0 },

    { ID_MINIMIZE, false, VK_M, MK_CONTROL, 0 },
    { ID_TOPMOST, false, VK_T, MK_CONTROL, 0 },

    { ID_PREFERENCES, false, VK_P, MK_CONTROL, 0 },

};

#define SZ_HOTKEY_FILE_HEADER_V1 "HotkeysV1.0"
#define SZ_HOTKEY_FILE_HEADER   "HotkeysV1.1"
#define SZ_HOTKEY_FILENAME      "hotkeys.cfg"

inline uint32_t makeHotkeyID(uint32_t nModifier, uint32_t button) {
    return nModifier | button;
}

cstr_t keyCodeToText(uint32_t key) {
    static IdToString KEY_NAMES[] = {
        { VK_A, "A" },
        { VK_S, "S" },
        { VK_D, "D" },
        { VK_F, "F" },
        { VK_H, "H" },
        { VK_G, "G" },
        { VK_Z, "Z" },
        { VK_X, "X" },
        { VK_C, "C" },
        { VK_V, "V" },
        { VK_B, "B" },
        { VK_Q, "Q" },
        { VK_W, "W" },
        { VK_E, "E" },
        { VK_R, "R" },
        { VK_Y, "Y" },
        { VK_T, "T" },
        { VK_1, "1" },
        { VK_2, "2" },
        { VK_3, "3" },
        { VK_4, "4" },
        { VK_6, "6" },
        { VK_5, "5" },
        { VK_EQUAL, "=" },
        { VK_9, "9" },
        { VK_7, "7" },
        { VK_MINUS, "-" },
        { VK_8, "8" },
        { VK_0, "0" },
        { VK_RIGHT_BRACKET, "]" },
        { VK_O, "O" },
        { VK_U, "U" },
        { VK_LEFT_BRACKET, "[" },
        { VK_I, "I" },
        { VK_P, "P" },
        { VK_L, "L" },
        { VK_J, "J" },
        { VK_QUOTE, "\"" },
        { VK_K, "K" },
        { VK_SEMI_COLON, ";" },
        { VK_BACK_SLASH, "\\" },
        { VK_COMMA, "," },
        { VK_SLASH, "/" },
        { VK_N, "N" },
        { VK_M, "M" },
        { VK_PERIOD, "." },
        { VK_GRAVE, "`" },
        { VK_KEYPAD_DECIMAL, "Keypad ." },
        { VK_KEYPAD_MULTIPLY, "Keypad *" },
        { VK_KEYPAD_PLUS, "Keypad +" },
        { VK_KEYPAD_CLEAR, "Keypad Esc" },
        { VK_KEYPAD_DIVIDE, "Keypad /" },
        { VK_KEYPAD_ENTER, "Keypad Enter" },
        { VK_KEYPAD_MINUS, "Keypad -" },
        { VK_KEYPAD_EQUAL, "Keypad =" },
        { VK_KEYPAD_0, "Keypad 0" },
        { VK_KEYPAD_1, "Keypad 1" },
        { VK_KEYPAD_2, "Keypad 2" },
        { VK_KEYPAD_3, "Keypad 3" },
        { VK_KEYPAD_4, "Keypad 4" },
        { VK_KEYPAD_5, "Keypad 5" },
        { VK_KEYPAD_6, "Keypad 6" },
        { VK_KEYPAD_7, "Keypad 7" },
        { VK_KEYPAD_8, "Keypad 8" },
        { VK_KEYPAD_9, "Keypad 9" },

        { VK_RETURN, "Enter" },
        { VK_TAB, "Tab" },
        { VK_SPACE, "space" },
        { VK_DELETE, "Del" },
        { VK_ESCAPE, "Esc" },
        { VK_COMMAND, "Command" },
        { VK_SHIFT, "Shift" },
        { VK_CAPS_LOCK, "Caps" },
        { VK_OPTION, "Option" },
        { VK_CONTROL, "Ctrl" },
        { VK_RIGHT_COMMAND, "Right Command" },
        { VK_RIGHT_SHIFT, "Right Shift" },
        { VK_RIGHT_OPTION, "Right Option" },
        { VK_RIGHT_CONTROL, "Right Ctrl" },
        { VK_FUNCTION, "Fn" },
        { VK_F17, "F17" },
        { VK_VOLUME_UP, "Volume Up" },
        { VK_VOLUME_DOWN, "Volume Down" },
        { VK_MUTE, "Mute" },
        { VK_F18, "F18" },
        { VK_F19, "F19" },
        { VK_F20, "F20" },
        { VK_F5, "F5" },
        { VK_F6, "F6" },
        { VK_F7, "F7" },
        { VK_F3, "F3" },
        { VK_F8, "F8" },
        { VK_F9, "F9" },
        { VK_F11, "F11" },
        { VK_F13, "F13" },
        { VK_F16, "F16" },
        { VK_F14, "F14" },
        { VK_F10, "F10" },
        { VK_F12, "F12" },
        { VK_F15, "F15" },
        { VK_HELP, "Help" },
        { VK_HOME, "Home" },
        { VK_PAGE_UP, "Page Up" },
        { VK_FORWARD_DELETE, "Delete" },
        { VK_F4, "F4" },
        { VK_END, "End" },
        { VK_F2, "F2" },
        { VK_PAGE_DOWN, "Page Down" },
        { VK_F1, "F1" },
        { VK_LEFT, "Left Arrow" },
        { VK_RIGHT, "Right Arrow" },
        { VK_DOWN, "Down Arrow" },
        { VK_UP, "Up Arrow" },
        { 0, nullptr },
    };

    return idToString(KEY_NAMES, key, "Unknown");
}

string formatHotkeyText(uint32_t key, uint32_t modifiers) {
    string text;

    if (isFlagSet(modifiers, MK_COMMAND)) {
        text += _TLT("Command+");
    }
    if (isFlagSet(modifiers, MK_FUNCTION)) {
        text += _TLT("Fn+");
    }
    if (isFlagSet(modifiers, MK_CONTROL)) {
        text += _TLT("Ctrl+");
    }
    if (isFlagSet(modifiers, MK_SHIFT)) {
        text += _TLT("Shift+");
    }
    if (isFlagSet(modifiers, MK_ALT)) {
        text += _TLT("Alt+");
    }

    text += keyCodeToText(key);
    return text;
}

struct _HotKeyPrivate {
    MediaKeyController          *mediaKeyController = nullptr;
};

CMPHotkey::CMPHotkey() {
    m_bGlobalHotkeyEnabled = false;
    m_data = new _HotKeyPrivate();
}

CMPHotkey::~CMPHotkey() {
    if (m_data->mediaKeyController) {
        [m_data->mediaKeyController release];
    }

    delete m_data;
}

void CMPHotkey::init() {
    m_bGlobalHotkeyEnabled = g_profile.getBool(SZ_APP_NAME, "enableGlobalHotkey", false);

    EventHandlerUPP hotKeyFunction = NewEventHandlerUPP(hotKeyHandler);
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyReleased;

    InstallApplicationEventHandler(hotKeyFunction, 1, &eventType, this, NULL);

    if (loadSettings() != ERR_OK) {
        // load default
        m_vAccKey.clear();
        for (int i = 0; i < CountOf(g_vDefAccKey); i++) {
            m_vAccKey.push_back(g_vDefAccKey[i]);
            m_vAccKey.back().idHotKey = makeHotkeyID(g_vDefAccKey[i].fsModifiers, g_vDefAccKey[i].button);
        }
    }

    if (m_bGlobalHotkeyEnabled) {
        registerAllGlobalHotKeys();
    }
}

void CMPHotkey::setEventWnd(Window *pWnd) {
    if (m_bGlobalHotkeyEnabled) {
        registerAllGlobalHotKeys();
    }
}

void CMPHotkey::quit() {
    // free hotkeys...
    unregisterAllGlobalHotKeys();
    m_vAccKey.clear();
}

void CMPHotkey::enableGlobalHotkey(bool bEnable) {
    if (m_bGlobalHotkeyEnabled == bEnable) {
        return;
    }

    g_profile.writeInt(SZ_APP_NAME, "enableGlobalHotkey", bEnable);

    m_bGlobalHotkeyEnabled = bEnable;

    if (m_bGlobalHotkeyEnabled) {
        registerAllGlobalHotKeys();
    } else {
        unregisterAllGlobalHotKeys();
    }
}

void CMPHotkey::restoreDefaults() {
    unregisterAllGlobalHotKeys();
    m_vAccKey.clear();

    for (int i = 0; i < CountOf(g_vDefAccKey); i++) {
        m_vAccKey.push_back(g_vDefAccKey[i]);
        m_vAccKey.back().idHotKey = makeHotkeyID(g_vDefAccKey[i].fsModifiers, g_vDefAccKey[i].button);
    }

    if (m_bGlobalHotkeyEnabled) {
        registerAllGlobalHotKeys();
    }

    saveSettings();
}

void CMPHotkey::onHotKey(uint32_t nId, uint32_t fuModifiers, uint32_t uVirtKey) {
    for (int i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (cmdKey.bGlobal && nId == cmdKey.idHotKey) {
            // execute cmd
            if (MPlayerApp::getMPSkinFactory()->getMainWnd()) {
                MPlayerApp::getMPSkinFactory()->getMainWnd()->postShortcutKeyCmd(cmdKey.cmd);
            }
            return;
        }
    }
}

bool CMPHotkey::onKeyDown(CMPSkinWnd *pWnd, uint32_t nChar, uint32_t nFlags) {
    assert(pWnd);

    // Remove MK_FUNCTION flag for mac
    nFlags &= ~MK_FUNCTION;
    auto nIDKeyPressed = makeHotkeyID(nFlags, nChar);

    for (int i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (!cmdKey.bGlobal && nIDKeyPressed == cmdKey.idHotKey) {
            // execute cmd
            pWnd->postShortcutKeyCmd(cmdKey.cmd);
            return true;
        }
    }

    return false;
}

void CMPHotkey::add(int cmd, bool bGlobal, int nKey, int fuModifiers) {
    CmdAccKey cmdKey;
    cmdKey.cmd = cmd;
    cmdKey.bGlobal = bGlobal;
    cmdKey.button = nKey;
    cmdKey.fsModifiers = fuModifiers;
    cmdKey.idHotKey = makeHotkeyID(cmdKey.fsModifiers, cmdKey.button);

    m_vAccKey.push_back(cmdKey);

    saveSettings();
}

bool CMPHotkey::set(int nIndex, int cmd, bool bGlobal, int nKey, int fuModifiers) {
    if (nIndex < 0 || nIndex >= (int)m_vAccKey.size()) {
        return false;
    }

    CmdAccKey &cmdKey = m_vAccKey[nIndex];
    if (cmdKey.button != 0 && cmdKey.bGlobal) {

    }
    cmdKey.cmd = cmd;
    cmdKey.bGlobal = bGlobal;
    cmdKey.button = nKey;
    cmdKey.fsModifiers = fuModifiers;
    cmdKey.idHotKey = makeHotkeyID(cmdKey.fsModifiers, cmdKey.button);

    saveSettings();

    return true;
}

bool CMPHotkey::remove(int nIndex) {
    if (nIndex < 0 || nIndex >= (int)m_vAccKey.size()) {
        return false;
    }

    // CmdAccKey &cmdKey = m_vAccKey[nIndex];
    m_vAccKey.erase(m_vAccKey.begin() + nIndex);

    saveSettings();

    return true;
}

CMPHotkey::CmdAccKey *CMPHotkey::get(int nIndex) {
    if (nIndex < 0 || nIndex >= (int)m_vAccKey.size()) {
        return nullptr;
    }

    return &(m_vAccKey[nIndex]);
}

int CMPHotkey::getByKey(int nKey, int nModifiers) {
    int nIDKey = makeHotkeyID(nModifiers, nKey);

    for (int i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (nIDKey == cmdKey.idHotKey) {
            return i;
        }
    }

    return -1;
}

int CMPHotkey::getByCmd(int cmd, int nStartFind) {
    if (nStartFind < -1 || nStartFind >= (int)m_vAccKey.size()) {
        return -1;
    }

    for (int i = nStartFind + 1; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (cmd == cmdKey.cmd) {
            return i;
        }
    }

    return -1;
}

bool CMPHotkey::isDefaultKey(int cmd) {
    std::set<int> setKeys;
    int i;

    for (i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (cmdKey.cmd == cmd) {
            setKeys.insert(makeHotkeyID(cmdKey.fsModifiers, cmdKey.button));
        }
    }

    for (i = 0; i < CountOf(g_vDefAccKey); i++) {
        if (g_vDefAccKey[i].cmd == cmd) {
            int nId = makeHotkeyID(g_vDefAccKey[i].fsModifiers, g_vDefAccKey[i].button);
            auto it = setKeys.find(nId);
            if (it != setKeys.end()) {
                setKeys.erase(it);
            } else {
                return false;
            }
        }
    }

    return setKeys.empty();
}

void CMPHotkey::restoreDefaultKey(int cmd) {
    int i;

    for (i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (cmdKey.cmd == cmd) {
            remove(i);
            i--;
        }
    }

    for (i = 0; i < CountOf(g_vDefAccKey); i++) {
        if (g_vDefAccKey[i].cmd == cmd) {
            add(cmd, g_vDefAccKey[i].bGlobal, g_vDefAccKey[i].button, g_vDefAccKey[i].fsModifiers);
        }
    }
}

bool CMPHotkey::getHotkeyText(int cmd, string &strKey) {
    for (int i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];

        if (cmdKey.cmd == cmd) {
            strKey = formatHotkeyText(cmdKey.button, cmdKey.fsModifiers);
            return true;
        }
    }

    if (cmd == ID_FONT_SIZE_INC) {
        strKey = string(_TLT("Ctrl+")) + _TLT("Mouse Wheel Up");
    } else if (cmd == ID_FONT_SIZE_DEC) {
        strKey = string(_TLT("Ctrl+")) + _TLT("Mouse Wheel Down");
    } else {
        return false;
    }

    return true;
}


int CMPHotkey::loadSettings() {
    // every line is a hotkey settings.
    // cmd, keycode, modifiers
    m_vAccKey.clear();

    string strFile;
    strFile = getAppDataDir();
    strFile += SZ_HOTKEY_FILENAME;

    CTextFile file;
    if (file.open(strFile.c_str(), false, ED_SYSDEF) != ERR_OK) {
        ERR_LOG1("Failed to open hotkey config file: %s", strFile.c_str());
        return ERR_OPEN_FILE;
    }

    bool bFileVer1 = false;

    // File header correct?
    string line;
    if (!file.readLine(line)) {
        return ERR_BAD_FILE_FORMAT;
    }
    if (strcmp(line.c_str(), SZ_HOTKEY_FILE_HEADER) != 0) {
        if (strcmp(line.c_str(), SZ_HOTKEY_FILE_HEADER_V1) != 0) {
            return ERR_BAD_FILE_FORMAT;
        }
        bFileVer1 = true;
    }

    while (file.readLine(line)) {
        CmdAccKey cmdKey;
        CCommaSeparatedValues csv;
        VecStrings vValues;

        csv.split(line.c_str(), vValues);
        trimStr(vValues);

        if (vValues.size() < 3) {
            continue;
        }

        size_t index = 0;

        // Cmd
        cmdKey.cmd = MPlayerApp::getMPSkinFactory()->getIDByName(vValues[index++].c_str());
        if (cmdKey.cmd == ID_INVALID) {
            continue;
        }

        // Global flag
        if (!bFileVer1) {
            cmdKey.bGlobal = tobool(stringToInt(vValues[index++].c_str()));
        }

        // keycode
        if (index < vValues.size()) {
            cmdKey.button = (uint32_t)stringToInt(vValues[index++].c_str());
        }

        // modifier
        if (index < vValues.size()) {
            cmdKey.fsModifiers = (uint32_t)stringToInt(vValues[index++].c_str());
        }

        cmdKey.idHotKey = makeHotkeyID(cmdKey.fsModifiers, cmdKey.button);
        m_vAccKey.push_back(cmdKey);
    }

    return ERR_OK;
}

int CMPHotkey::saveSettings() {
    string strFile;
    CTextFile file;

    strFile = getAppDataDir();
    strFile += SZ_HOTKEY_FILENAME;

    if (file.open(strFile.c_str(), true, ED_SYSDEF) != ERR_OK) {
        ERR_LOG1("Failed to open hotkey config file: %s", strFile.c_str());
        return ERR_OPEN_FILE;
    }

    file.writeLine(SZ_HOTKEY_FILE_HEADER);

    CCommaSeparatedValues csv;

    for (int i = 0; i < (int)m_vAccKey.size(); i++) {
        CmdAccKey &cmdKey = m_vAccKey[i];
        string strId = MPlayerApp::getMPSkinFactory()->getStringOfID(cmdKey.cmd);

        if (strId.empty()) {
            continue;
        }

        csv.addValue(strId.c_str());
        csv.addValue(cmdKey.bGlobal);
        csv.addValue(cmdKey.button);
        csv.addValue(cmdKey.fsModifiers);
        file.writeLine(csv.c_str(), csv.size());

        csv.clear();
    }

    file.close();

    return ERR_OK;
}

// Cocoa and Carbon uses different modifiers consts.
uint32_t cocoaModifiersToCarbon(uint32_t modifers) {
    // Event.h
    uint32_t flags = 0;
    if (modifers & MK_SHIFT) { flags |= shiftKey; }
    if (modifers & MK_CONTROL) { flags |= controlKey; }
    if (modifers & MK_ALT) { flags |= optionKey; }
    if (modifers & MK_COMMAND) { flags |= cmdKey; }
    // if (modifers & MK_FUNCTION) { flags |= shiftKey; }
    return flags;
}

void CMPHotkey::registerAllGlobalHotKeys() {
    if (!m_data->mediaKeyController) {
        m_data->mediaKeyController = [[MediaKeyController alloc] init];
        [m_data->mediaKeyController retain];
    }

    for (CmdAccKey &cmdKey : m_vAccKey) {
        if (cmdKey.bGlobal) {
            cmdKey.idHotKey = makeHotkeyID(cmdKey.fsModifiers, cmdKey.button);
            cmdKey.hotKeyRef = NULL;

            UInt32 keyCode = cmdKey.button;
            EventHotKeyID keyID;
            keyID.signature = 'DHPL';
            keyID.id = cmdKey.idHotKey;

            RegisterEventHotKey(keyCode, cocoaModifiersToCarbon(cmdKey.fsModifiers), keyID, GetApplicationEventTarget(), 0, &cmdKey.hotKeyRef);
            //assert(err == noErr);
        }
    }
}

void CMPHotkey::unregisterAllGlobalHotKeys() {
    if (m_data->mediaKeyController) {
        [m_data->mediaKeyController release];
        m_data->mediaKeyController = nullptr;
    }

    for (CmdAccKey &cmdKey : m_vAccKey) {
        if (cmdKey.hotKeyRef != nullptr) {
            UnregisterEventHotKey(cmdKey.hotKeyRef);
            cmdKey.hotKeyRef = nullptr;
        }
    }
}
