#pragma once

#ifndef MPlayerUI_MPEventsDispatcher_h
#define MPlayerUI_MPEventsDispatcher_h


#include "../MPlayerEngine/IPlayerCore.hpp"
#include "../Skin/Skin.h"
#include "../MPlayer/Player.h"
#include "MPlayerApp.h"


// section name
#define SZ_SECT_LYR_DISPLAY "LyrDispaly"
#define SZ_SECT_FLOATING_LYR "FloatingLyr"
#define SZ_SECT_PLAYER      "Player"
#define SZ_SECT_LYR_DL      "Download"

#define SZ_SECT_UI          "MusicPlayer"

enum MLEventType {
    ET_INVALID                  = 0,
    ET_NULL                     = 0,

    ET_PLAYER_STATUS_CHANGED,
    ET_PLAYER_SEEK,
    ET_PLAYER_CUR_MEDIA_CHANGED,
    ET_PLAYER_CUR_MEDIA_INFO_CHANGED,
    ET_PLAYER_MEDIA_INFO_CHANGED,
    ET_PLAYER_CUR_PLAYLIST_CHANGED,
    ET_PLAYER_SETTING_CHANGED,
    ET_PLAYER_EQ_SETTING_CHANGED,
    ET_PLAYER_PLAY_HALT_ERROR,
    ET_PLAYER_POS_UPDATE,
    ET_PLAY_HISTORY_RECORDED,

    ET_LYRICS_RESEARCH,
    ET_LYRICS_DISPLAY_SETTINGS,
    ET_LYRICS_FLOATING_SETTINGS,
    ET_LYRICS_DRAW_UPDATE,
    ET_LYRICS_CHANGED,
    ET_LYRICS_ON_SAVE_EDIT,
    ET_LYRICS_EDITOR_RELOAD_TAG,

    ET_LYRICS_SEARCH_END,
    ET_DOWNLOAD_END,

    ET_UI_SETTINGS_CHANGED,
    ET_UI_INFO_TEXT,
    ET_UI_LONG_ERROR_TEXT,

    ET_VIS_DRAW_UPDATE,

    COUNT_OF_EVENT_TYPE,
};

struct CEventPlayerStatusChanged : public IEvent {
    PlayerState                 status;
};

struct CEventPlayerSettingChanged : public IEvent {
    IMPEvent::SettingType   settingType;
    int                         value;
};

struct CEventPlaylistChanged : public IEvent {
    IMPEvent::PlaylistChangeAction action;
    int                         nIndex, nIndexOld;
};

struct CEventPlayHistoryRecorded : public IEvent {
    int                         songId = 0;
};

struct CEventPlayerEQChanged : public IEvent {
    EQualizer                   eqlalizer;
};

class CEventLyricsEditorCmd : public IEvent {
public:
    uint32_t                    cmd;
};

class CEventDownloadEnd : public IEvent {
public:
    enum DOWNLOAD_TYPE {
        DT_DL_LRC_FAILED,
        DT_DL_CHECK_NEW_VERSION_OK,
    };

    DOWNLOAD_TYPE               downloadType;
    class CDownloadTask         *pTask;

};

class CEventVisDrawUpdate : public IEvent {
public:
    // VisParam                    *pVisParam;
};

class ISkinCmdHandler {
public:
    ISkinCmdHandler() { m_pSkinWnd = nullptr; }
    virtual ~ISkinCmdHandler() { }

    virtual void init(CSkinWnd *pSkinWnd) { m_pSkinWnd = pSkinWnd; }

    // if the command id is processed, return true.
    virtual bool onCommand(uint32_t nId) = 0;
    virtual bool onUIObjNotify(IUIObjNotify *pNotify) = 0;

    // IUICheckStatus method
    virtual bool getChecked(uint32_t nID, bool &bChecked) { return false; }
    virtual bool getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) { return false; }

protected:
    CSkinWnd                    *m_pSkinWnd;

};

class CMPlayerSettings {
public:
    // szValue: p1="v1" p2="v2"
    //    or        v1
    static void setLyricsDisplaySettings(cstr_t szSettingName, cstr_t szValue, bool bNotify = true) {
        setSettings(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, szSettingName, szValue, bNotify);
    }

    // szValue: p1="v1" p2="v2"
    //    or        v1
    static void setLyricsDisplaySettings(cstr_t szSettingName, int value, bool bNotify = true) {
        setSettings(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, szSettingName, value, bNotify);
    }

    //
    // Player UI
    //
    static void getPlayerUISettings(cstr_t szSettingName, string &strValue) {
        getSettings(SZ_SECT_UI, szSettingName, strValue);
    }

    // szValue: p1="v1" p2="v2"
    //    or        v1
    static void setPlayerUISettings(cstr_t szSettingName, cstr_t szValue, bool bNotify = true) {
        setSettings(ET_UI_SETTINGS_CHANGED, SZ_SECT_UI, szSettingName, szValue, bNotify);
    }

    // szValue: p1="v1" p2="v2"
    //    or        v1
    static void setPlayerUISettings(cstr_t szSettingName, int value, bool bNotify = true) {
        setSettings(ET_UI_SETTINGS_CHANGED, SZ_SECT_UI, szSettingName, value, bNotify);
    }

    static void getSettings(cstr_t szSectionName, cstr_t szSettingName, string &strValue) {
        strValue = g_profile.getString(szSectionName, szSettingName, "");
    }

    static int getSettings(cstr_t szSectionName, cstr_t szSettingName, int nValueDef) {
        return g_profile.getInt(szSectionName, szSettingName, nValueDef);
    }

    // szValue: p1="v1" p2="v2"
    //    or        v1
    static void setSettings(EventType eventType, cstr_t szSectionName, cstr_t szSettingName, cstr_t szValue, bool bNotify = true);

    static void setSettings(EventType eventType, cstr_t szSectionName, cstr_t szSettingName, int value, bool bNotify = true);

};

#endif // !defined(MPlayerUI_MPEventsDispatcher_h)
