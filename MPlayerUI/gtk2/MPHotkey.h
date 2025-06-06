﻿#pragma once

#ifndef MPlayerUI_gtk2_MPHotkey_h
#define MPlayerUI_gtk2_MPHotkey_h



struct MPHotKeySection {
    cstr_t                      szName;
    int                         *vHotkeys;
};

extern MPHotKeySection g_vHotkeySections[];

class CMPHotkey {
public:
    struct CmdAccKey {
        int                         cmd;
        bool                        bGlobal;
        uint16_t                    button;
        uint16_t                    fsModifiers;
        uint16_t                    idHotKey;
    };
    typedef vector<CmdAccKey>       VecCmAccKeys;
    typedef VecCmAccKeys::iterator  iterator;

public:
    CMPHotkey();
    virtual ~CMPHotkey();

    void init();

    void setEventWnd(Window *pWnd);

    void quit();

    void enableGlobalHotkey(bool bEnable);

    void restoreDefaults();

    void onHotKey(int nId, uint32_t fuModifiers, uint32_t uVirtKey);

    // If this key was processed, return true.
    bool onKeyDown(CMPSkinWnd *pWnd, uint32_t nChar, uint32_t nFlags);

    void add(int cmd, bool bGlobal, int nKey, int fuModifiers);
    bool set(int nIndex, int cmd, bool bGlobal, int nKey, int fuModifiers);
    bool remove(int nIndex);
    CmdAccKey *get(int nIndex);
    int getByKey(int nKey, int nModifiers);
    int getByCmd(int cmd, int nStartFind = -1);

    bool isDefaultKey(int cmd);
    void restoreDefaultKey(int cmd);

    bool getHotkeyText(int cmd, string &strKey);

    iterator begin() { return m_vAccKey.begin(); }
    iterator end() { return m_vAccKey.end(); }

protected:
    int loadSettings();
    int saveSettings();

    void registerAllHotKeys();
    void unregisterAllHotKeys();

protected:
    bool                        m_bEnabled;
    VecCmAccKeys                m_vAccKey;

};

#endif // !defined(MPlayerUI_gtk2_MPHotkey_h)
