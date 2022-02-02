
#pragma once

#include "MLClientSession.h"

#include "Helper.h"
#include "MPSkinFactory.h"
#include "MPEventsDispatcher.h"
#include "Player.h"
#include "MLCmd.h"
#include "MPSkinMainWnd.h"
#include "LyricsLocalSearch.h"
#include "MLProfile.h"
#include "../LyricsLib/MLLib.h"

#ifdef _WIN32
#include "win32/MPHotkey.h"
#include "win32/DlgChooseFont.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/MPHotkey.h"
#include "gtk2/DlgChooseFont.h"
#endif

#ifdef _MAC_OS
#include "mac/MPHotkey.h"
#include "mac/DlgChooseFont.h"
#endif

#include "resource.h"

#ifdef _MPLAYER
#define SZ_APP_NAME            "ZikiPlayer"
#define SZ_COMPANY_NAME        ""
#else
#define SZ_APP_NAME            "MiniLyrics"
#define SZ_COMPANY_NAME        "CrintSoft"
#endif

#define SZ_MACRO_PRODUCT_NAME                "$Product$"
#define SZ_MACRO_COMPANY_NAME                "$Company$"


#ifdef _MPLAYER
#define SZ_PROFILE_NAME        "ZikiPlayer.ini"
#define SZ_MUTEX_RUNNING    "ZikiPlayerRunning"
#define SZ_MAINWND_CLASSNAME    "ZikiPlayer"
#ifndef SZ_MINILYRICS_SERVICE_CLASS_NAME
#define SZ_MINILYRICS_SERVICE_CLASS_NAME    SZ_MAINWND_CLASSNAME
#endif
#else
#define SZ_PROFILE_NAME        "MiniLyric.ini"
#define SZ_MUTEX_RUNNING    "MiniLyricsRunning"
#define SZ_MAINWND_CLASSNAME    "MiniLyrics"
#endif

class CDownloadMgr;
class CMLClientSession;
class CPreferenceDlg;
class CDownloadTask;
class CMPlayerApp;
class CMPEventsDispatcher;
class CMLData;


// lyrics data
extern CMLData                    g_LyricData;

// lyrics download manager
extern CDownloadMgr                g_LyricsDownloader;

// Local lyrics search
extern CLyricsLocalSearch        g_LyricSearch;

enum STR_NAME
{
    SN_HTTP_DOMAIN,
    SN_HTTP_REGISTER,
    SN_HTTP_DLSKIN,
    SN_HTTP_DLPLUGIN,
    SN_HTTP_FAQ_INET,
    SN_HTTP_BBS,
    SN_HTTP_ML_VER,
    // SN_HTTP_AD,
    SN_HTTP_SIGNUP,
    SN_HTTP_LOGIN,
    SN_HTTP_RATE_LRC,
    SN_HTTP_HELP,
    SN_HTTP_HELP_EMBEDDED_LYR,
    SN_HTTP_HELP_UPLOAD_LYR,
    SN_HTTP_HELP_EDIT_LYR,
    SN_HTTP_HELP_SEARCH_LYR_SUGGESTIONS,
    SN_HTTP_HELP_STATIC_LYR,
    SN_HTTP_HELP_G15_LCD,
    SN_HTTP_FEEDBACK,
    SN_EMAIL,
    SN_SUPPORT_MAIL,
    SN_WEBHOME,
};

string getAppNameLong();

cstr_t getStrName(STR_NAME nameId);

void dispatchPlayPosEvent(int nPlayPos);

class CMPlayerAppBase : public CSkinApp
{
public:
    CMPlayerAppBase();
    virtual ~CMPlayerAppBase();

    virtual bool init();
    virtual void quit();

    virtual void onEvent(const IEvent *pEvent);

    virtual void postQuitMessage();

protected:
    virtual CEventsDispatcher *newEventPatcher();
    virtual CSkinFactory *newSkinFactory();

public:
    static CMPlayerApp *getInstance();

    static CMPSkinFactory *getMPSkinFactory();
    static CEventsDispatcherBase *getEventsDispatcher();

    static CMPSkinMainWnd *getMainWnd();
    static CMPHotkey &getHotkey();

    void dispatchInfoText(cstr_t szInfo, cstr_t szInfoName = nullptr);

    // When user click the info text, the szCmd can be executed.
    void dispatchLongErrorText(cstr_t szError, cstr_t szCmd = nullptr);
    void dispatchLongErrorText(cstr_t szError, int cmd);

    void dispatchResearchLyrics();
    void dispatchLyricsChangedSyncEvent();

    void onMediaChanged(bool bAutoDownloadIfNotExist);

    void onLanguageChanged();

    void onOnlineSearchEnd();

    void onDownloadLyricsFailed(CDownloadTask *pTask);

    void newLyrics();

    int openLyrics(cstr_t szAssociateKeyword, cstr_t szLrcSource);

    bool onLyricsChangingSavePrompt();

    int messageOut(cstr_t lpText, uint32_t uType = MB_ICONINFORMATION | MB_OK, cstr_t lpCaption = nullptr);

    int changeSkinByUserCmd(cstr_t szSkin);

    void getCurLyrDisplaySettingName(bool bFloatingLyr, string &strSectionName, EventType &etDispSettings);
    cstr_t getCurLyrDisplaySettingName(bool bFloatingLyr);

protected:
    void setDefaultSettings();

protected:
    CMPHotkey                m_hotKey;

};

#ifdef _WIN32
#include "win32/MPlayerApp.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/MPlayerApp.h"
#endif

#ifdef _MAC_OS
#include "mac/MPlayerApp.h"
#endif

