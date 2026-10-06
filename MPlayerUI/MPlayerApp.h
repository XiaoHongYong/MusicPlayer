#pragma once

#include "MLClientSession.h"

#include "Helper.h"
#include "MPSkinFactory.h"
#include "MPEventsDispatcher.h"
#include "MLCmd.h"
#include "MPSkinMainWnd.h"
#include "LyricsLocalSearch.h"
#include "MLProfile.h"
#include "../MPlayer/Player.h"

#ifdef _WIN32
#include "win32/MPHotkey.h"
#include "../MusicPlayer/win32/resource.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/MPHotkey.h"
#endif

#ifdef _MAC_OS
#include "mac/MPHotkey.h"
#endif


#define SZ_APP_NAME         "MusicPlayer"
#define SZ_COMPANY_NAME     "CrintSoft"

#define SZ_MACRO_PRODUCT_NAME   "$Product$"
#define SZ_MACRO_COMPANY_NAME   "$Company$"


#define SZ_PROFILE_NAME     "MusicPlayer.ini"
#define SZ_MUTEX_RUNNING    "MusicPlayerRunning"
#define SZ_MAINWND_CLASSNAME "MusicPlayer"

class CDownloadMgr;
class CMLClientSession;
class CPreferenceDlg;
class CDownloadTask;
class MPlayerApp;
class CurrentLyrics;


// lyrics data
extern CurrentLyrics g_currentLyrics;

// lyrics download manager
extern CDownloadMgr g_LyricsDownloader;

// Local lyrics search
extern CLyricsLocalSearch g_LyricSearch;

enum STR_NAME {
    SN_HTTP_DOMAIN,
    SN_HTTP_FAQ_INET,
    SN_HTTP_VERSION_UPDATE,
    // SN_HTTP_AD,
    SN_HTTP_SIGNUP,
    SN_HTTP_LOGIN,
    SN_HTTP_RATE_LRC,
    SN_HTTP_HELP,
    SN_HTTP_HELP_EDIT_LYR,
    SN_HTTP_HELP_SEARCH_LYR_SUGGESTIONS,
    SN_HTTP_FEEDBACK,
    SN_EMAIL,
    SN_SUPPORT_MAIL,
    SN_WEBHOME,
};

string getAppNameLong();

cstr_t getStrName(STR_NAME nameId);

void dispatchPlayPosEvent(int nPlayPos);

class MPlayerApp : public CSkinApp {
public:
    MPlayerApp();
    virtual ~MPlayerApp();

    virtual bool init() override;
    virtual void quit() override;

    virtual void onEvent(const IEvent *pEvent) override;

    virtual void postQuitMessage() override;

protected:
    virtual CSkinFactory *newSkinFactory() override;

public:
    static MPlayerApp *getInstance();

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

    bool isAnotherInstanceRunning();
    bool setRunningFlag();

protected:
    void setDefaultSettings();

    bool _init();
    void _quit();

protected:
    CMPHotkey                   m_hotKey;

#ifdef _WIN32
    HANDLE                      m_hMutexRuning = NULL;
#else
    bool                        m_bRunning = false;
#endif

};

#ifdef _MAC_OS
void reloadMacApplicationMenu();
#endif
