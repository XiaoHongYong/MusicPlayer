/********************************************************************
    Created  :    2002/02/21    2:11
    FileName :    DownloadMgr.cpp
    Author   :    xhy

    Purpose  :    下载任务的管理类
*********************************************************************/

#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "../MLProtocol/HttpClient.h"
#include "../MediaTags/LrcParser.h"
#include "../LyricsLib/CurrentLyrics.h"
#include "OnlineSearch.h"
#include "AutoProcessEmbeddedLyrics.h"


#define SZ_USER_AGENT_IE5 "Mozilla/4.0 (compatible; MSIE 5.00; Windows 98)  "

CDownloadMgr::CDownloadMgr() : m_eventShutDown(true, false) {
    m_bAutoDownload = true;
}


CDownloadMgr::~CDownloadMgr() {

}


int CDownloadMgr::init() {
    m_eventShutDown.reset();

    g_nPortDownloadServer = g_profile.getInt("PortDownloadServer", 80);
    g_nPortSearchServer = g_profile.getInt("PortSearchServer", 80);

#ifdef _WIN32
    if (g_profile.getInt("ProxyType", 0xFF) == 0xFF) {
        // 第一次运行，自动加载代理设置
        bool bUseProxy;
        string strSvr;
        int nPort;

        if (loadProxySvrFromIE(bUseProxy, strSvr, nPort)) {
            if (bUseProxy) {
                g_profile.writeInt("ProxyType", HTTP_PROXY_OURS);
            } else {
                g_profile.writeInt("ProxyType", HTTP_PROXY_NONE);
            }
            g_profile.writeString("ProxyServer", strSvr.c_str());
            g_profile.writeInt("ProxyPort", nPort);
        } else {
            g_profile.writeInt("ProxyType", HTTP_PROXY_NONE);
        }
    }
#endif

    // set default save path
    m_strDefSavePath = CMLProfile::getDir(SZ_SECT_LYR_DL, "LyricsDownPath", "");
    if (m_strDefSavePath.empty()) {
        createDefaultLyricsDir(m_strDefSavePath);
    }

    DBG_LOG1("Lyrics download dir: %s", m_strDefSavePath.c_str());

    m_bAutoDownload = g_profile.getBool(SZ_SECT_LYR_DL, "EnableAuoDownload", true);

    return ERR_OK;
}


void CDownloadMgr::quit() {
    g_profile.writeInt("PortDownloadServer", g_nPortDownloadServer);
    g_profile.getInt("PortSearchServer", g_nPortSearchServer);

    m_eventShutDown.set();
    m_threadDownload.join();

    MutexAutolock lock(m_mutexAccess);
    for (LIST_TASK::iterator it = m_listTasks.begin(); it != m_listTasks.end(); ++it) {
        CDownloadTask *pTask = *it;
        delete pTask;
    }
    m_listTasks.clear();
}

void CDownloadMgr::onSongChanged() {
    if (!m_bAutoDownload) {
        return;
    }

    if (g_currentLyrics.hasLyricsOpened()) {
        // For text lyrics and not associated by user, continue to search for LRC lyrics.
        if (!(g_profile.getBool(SZ_SECT_LYR_DL, "DownLrcEvenIfHasTxt", true)
            && g_currentLyrics.getLyrContentType() == LCT_TXT
            && !g_LyricSearch.isAssociatedLyrics(g_player.getMediaKey().c_str()))) {
            return;
        }
    }

    if (!searchInCacheResult(false)) {
        g_OnlineSearch.autoSearch();
    }
}

bool CDownloadMgr::searchInCacheResult(bool bShowInfoText) {
    ListLyrSearchResults vLrcSearchResult;

    if (!g_OnlineSearch.searchCacheForCur(&vLrcSearchResult)) {
        static int nFailedCount = 0;
        nFailedCount++;
        if (bShowInfoText && nFailedCount > 2) {
            string str;
            str = _TLT("Failed to get lyrics search results.");
            str += " ";
            str += _TLT("Please contact us to report this issue.");
            MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str(), ID_EMAIL);
        }
        return false;
    }

    if (vLrcSearchResult.size() == 0) {
        if (bShowInfoText) {
            string str;
            str = _TLT("Search returned no results.");
            int cmds[] = { ID_NO_SUITTABLE_LYRICS, ID_INSTRUMENTAL_MUSIC, ID_SEARCH_LYR_SUGGESTIONS };
            string strCmd = "cmd://";
            for (int i = 0; i < CountOf(cmds); i++) {
                if (i != 0) {
                    strCmd += "|";
                }
                strCmd += MPlayerApp::getInstance()->getSkinFactory()->getStringOfID(cmds[i]);
            }
            MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str(), strCmd.c_str());
        }
        return false;
    }

    float nValueMax = 0;
    bool bUserSelDown;
    ListLyrSearchResults::iterator itBest;

    bUserSelDown = g_profile.getBool(SZ_SECT_LYR_DL, "DownLrcUserSelect", false);

    //
    // 分值超过 MATCH_VALUE_OK
    itBest = vLrcSearchResult.getTheBestMatchLyrics(false);
    if (itBest != vLrcSearchResult.end()) {
        LrcSearchResult &result = *itBest;
        nValueMax = result.nMatchValue;
        if (nValueMax >= MATCH_VALUE_OK) {
            // Do not download .txt lyrics
            if (fileIsExtSame(result.strUrl.c_str(), ".txt")
                && g_profile.getInt(SZ_SECT_LYR_DL, "OnlyDlSyncLyr", false)) {
                MPlayerApp::getInstance()->dispatchLongErrorText(_TLT("None of these lyrics are .lrc files, so they were not downloaded."));
                return true;
            }

            // If current lyrics is text lyrics, and the match lyrics is also text
            // lyrics, do NOT download it.
            if (g_currentLyrics.getLyrContentType() != LCT_UNKNOWN
                && result.nMatchValue < MATCH_VALUE_BETTER) {
                return true;
            }

            // 就下载它了
            string str;
            str = _TLT("Downloading lyrics:");
            str += " ";
            str += result.strSaveFileName.c_str();
            MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str());

            downloadLyrics(g_player.getMediaKey().c_str(),
                g_player.getSrcMedia(), result.strUrl.c_str(), result.strSaveFileName.c_str());
            return true;
        }
    }

    // If any lyrics is opened, do not continue to download.
    if (g_currentLyrics.hasLyricsOpened()) {
        return true;
    }

    // 由用户选择？
    if (bUserSelDown) {
        // if all lyrics is txt, return directly.
        if (g_profile.getInt(SZ_SECT_LYR_DL, "OnlyDlSyncLyr", false)
            && vLrcSearchResult.isAllTxtLyrics()) {
            MPlayerApp::getInstance()->dispatchLongErrorText(_TLT("None of these lyrics are .lrc files, so they were not downloaded."));
            return true;
        }

        if (bShowInfoText) {
            MPlayerApp::getInstance()->dispatchLongErrorText(
                _TLT("Found lyrics, please select the lyrics in the popup dialog."), ID_OPEN_LRC);
        }

        MPlayerApp::getMainWnd()->onCommand(ID_OPEN_LRC);
        return true;
    } else {
        if (bShowInfoText) {
            MPlayerApp::getInstance()->dispatchLongErrorText(
                _TLT("Found similar lyrics. Please choose them in the Search Lyrics window."), ID_OPEN_LRC);
        }
    }

    return true;
}

void CDownloadMgr::downloadLyrics(cstr_t szMediaKey, cstr_t szSongFile, cstr_t szUrl, cstr_t szFileName) {
    CDownloadTask *pTask = new CDownloadTask(DTT_LYRICS, szUrl);
    pTask->strMediaKey = szMediaKey;
    pTask->m_strLyrFileName = szFileName;

    addTask(pTask);
}

void CDownloadMgr::downloadVersionFile(cstr_t szUrl, bool bNoUI) {
    CDownloadTask *pTask = new CDownloadTask(bNoUI ? DTT_CHECK_VERSION_NOUI : DTT_CHECK_VERSION, szUrl);
    addTask(pTask);
}

void CDownloadMgr::addTask(CDownloadTask *pTask) {
    if (isTaskExist(pTask->m_strURL.c_str())) {
        delete pTask;
        return;
    }

    MutexAutolock lock(m_mutexAccess);
    m_listTasks.push_back(pTask);

    if (!m_threadDownload.isRunning()) {
        if (!m_threadDownload.create(downloadThreadProc, this)) {
            ERR_LOG0("FAILED to create downloading thread.");
        }
    }
}

bool CDownloadMgr::isTaskExist(cstr_t szURL) {
    MutexAutolock lock(m_mutexAccess);

    LIST_TASK::iterator it;

    for (it = m_listTasks.begin(); it != m_listTasks.end(); ++it) {
        CDownloadTask *pTask = *it;
        if (strcasecmp(pTask->m_strURL.c_str(), szURL) == 0) {
            return true;
        }
    }

    for (it = m_listRunningTasks.begin(); it != m_listRunningTasks.end(); ++it) {
        CDownloadTask *pTask = *it;
        if (strcasecmp(pTask->m_strURL.c_str(), szURL) == 0) {
            return true;
        }
    }

    return false;
}

void CDownloadMgr::createDefaultLyricsDir(string &strDefSavePath) {
#ifdef _WIN32
    // Retrieve the root Dir.
    strDefSavePath = getAppResourceDir();
    strDefSavePath.resize(3);
    if (!isDirWritable(strDefSavePath.c_str())) {
        utf16_t buf[MAX_PATH];

        // Try to save in windows drive
        GetWindowsDirectoryW(buf, CountOf(buf));
        strDefSavePath = ucs2ToUtf8(buf);
        strDefSavePath.resize(3);
        if (!isDirWritable(strDefSavePath.c_str())) {
            strDefSavePath = getAppDataDir();
        }
    }
#else
    strDefSavePath = getAppDataDir();
#endif
    strDefSavePath += "Lyrics";
    dirStringAddSep(strDefSavePath);

    if (!isDirExist(strDefSavePath.c_str())) {
        createDirectoryAll(strDefSavePath.c_str());
    }

    if (isEmptyString(g_profile.getString(SZ_SECT_LYR_DL, "LyricsDownPath", ""))) {
        CMLProfile::writeDir(SZ_SECT_LYR_DL, "LyricsDownPath", strDefSavePath.c_str());
    }
}

string CDownloadMgr::getSaveLyricsFile(cstr_t szSongFile, cstr_t szFileName) {
    DOWN_SAVE_DIR DownSaveDir;
    DOWN_SAVE_NAME DownSaveName;

    DownSaveDir = (DOWN_SAVE_DIR)g_profile.getInt(SZ_SECT_LYR_DL, "DownSaveInSongDir", DOWN_SAVE_IN_CUSTOM_DIR);
    DownSaveName = (DOWN_SAVE_NAME)g_profile.getInt(SZ_SECT_LYR_DL, "DownSaveName", DOWN_SAVE_NAME_KEEP);

    return getSaveLyricsFile(szSongFile, szFileName, DownSaveDir, DownSaveName);
}

string CDownloadMgr::getSaveLyricsFile(cstr_t szSongFile, cstr_t szFileName, DOWN_SAVE_DIR DownSaveDir, DOWN_SAVE_NAME DownSaveName) {
    szFileName = fileGetName(szFileName);

    string strLyricsFile;
    if (DownSaveDir == DOWN_SAVE_IN_SONG_DIR) {
        strLyricsFile = fileGetPath(szSongFile);
    }

    if (strLyricsFile.empty()) {
        strLyricsFile = m_strDefSavePath.c_str();
    }

    dirStringAddSep(strLyricsFile);

    // save lyrics in sub folder by ABC order?
    if (DownSaveDir != DOWN_SAVE_IN_SONG_DIR
        && g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveByABC", false)) {
        char c = *szFileName;
        if (isAlpha(c)) {
            strLyricsFile += toupper(c);
        } else if (c >= '0' && c <='9') {
            strLyricsFile += '#';
        }

        dirStringAddSep(strLyricsFile);
    }

    string name;
    if (DownSaveName == DOWN_SAVE_NAME_AS_SONG_NAME) {
        name = urlGetTitle(szSongFile);
        if (!name.empty()) {
            name += fileGetExt(szFileName);
        }
    }
    if (name.empty() && szFileName) {
        name = szFileName;
    }

    strLyricsFile += name;

    return strLyricsFile;
}

void CDownloadMgr::onEndDownload(CDownloadTask *pTask) {
    if (pTask->m_errResult != ERR_OK) {
        if (pTask->taskType == DTT_LYRICS) {
            CEventDownloadEnd *pEvent = new CEventDownloadEnd;
            pEvent->eventType = ET_DOWNLOAD_END;
            pEvent->pTask = pTask;
            pEvent->downloadType = CEventDownloadEnd::DT_DL_LRC_FAILED;
            MPlayerApp::getEventsDispatcher()->dispatchSyncEventByNoUIThread(pEvent);
        } else if (pTask->taskType == DTT_CHECK_VERSION_NOUI
            || pTask->taskType == DTT_CHECK_VERSION) {
            CEventDownloadEnd *pEvent = new CEventDownloadEnd;
            pEvent->eventType = ET_DOWNLOAD_END;
            pEvent->pTask = pTask;
            pEvent->downloadType = CEventDownloadEnd::DT_DL_CHECK_NEW_VERSION_OK;
            MPlayerApp::getEventsDispatcher()->dispatchSyncEventByNoUIThread(pEvent);
        }
        return;
    }

    switch (pTask->taskType) {
    case DTT_LYRICS:
        //
        // 本地链接下载任务完成时的处理
        //
        {
            if (saveDownloadedLyrics(pTask->strMediaKey.c_str(), pTask->m_strLyrFileName.c_str(),
                pTask->buffContent.c_str(), (int)pTask->buffContent.size()) == ERR_OK) {
                MPlayerApp::getInstance()->dispatchResearchLyrics();

                // show rate link
                if (!g_profile.getBool(SZ_SECT_UI, "HideRateLink", false)) {
                    if (g_currentLyrics.properties().id.size()) {
                        MPlayerApp::getInstance()->dispatchLongErrorText(_TLT("Do these lyrics match the song? Rate them!"), ID_RATE_LYR);
                    }
                }
            }
        }
        break;
    case DTT_CHECK_VERSION:
    case DTT_CHECK_VERSION_NOUI:
        {
            CEventDownloadEnd *pEvent = new CEventDownloadEnd;
            pEvent->eventType = ET_DOWNLOAD_END;
            pEvent->pTask = pTask;
            pEvent->downloadType = CEventDownloadEnd::DT_DL_CHECK_NEW_VERSION_OK;
            MPlayerApp::getEventsDispatcher()->dispatchSyncEventByNoUIThread(pEvent);
        }
        break;
    default:
        break;
    }
}

void CDownloadMgr::downloadThreadProc(void *lpParam) {
    CDownloadMgr *pThis = (CDownloadMgr *)lpParam;

    pThis->downloadThread();
}


void CDownloadMgr::downloadThread() {
    while (!isQuiting()) {
        CDownloadTask *pTask = nullptr;

        // Pick up one task
        {
            MutexAutolock lock(m_mutexAccess);
            if (m_listTasks.empty()) {
                return;
            }
            pTask = m_listTasks.front();
            m_listTasks.pop_front();
            m_listRunningTasks.push_back(pTask);
        }

        if (isQuiting()) {
            return;
        }

        if (strncasecmp(pTask->m_strURL.c_str(), "http://", 7) == 0) {
            runHttpTask(pTask);
        }

        if (isQuiting()) {
            return;
        }

        onEndDownload(pTask);

        // remove task from queue
        MutexAutolock lock(m_mutexAccess);
        for (LIST_TASK::iterator it = m_listRunningTasks.begin(); it != m_listRunningTasks.end(); ++it) {
            if (*it == pTask) {
                delete pTask;
                m_listRunningTasks.erase(it);
                break;
            }
        }
    }
}

int CDownloadMgr::tryToConnect(CHttpClient &httpClient) {
    int nRet;

    // Latest port
    nRet = httpClient.connect();
    if (nRet == ERR_OK) {
        return nRet;
    }

    if (isQuiting()) {
        return ERR_FALSE;
    }

    // 80
    if (g_nPortDownloadServer != 80) {
        httpClient.setHostPort(80);
        nRet = httpClient.connect();
        if (nRet == ERR_OK) {
            g_nPortDownloadServer = 80;
            return ERR_OK;
        }
    }

    // 8000 ~ 8080
    if (g_nPortDownloadServer < SERVER_PORT_BEGIN) {
        g_nPortDownloadServer = SERVER_PORT_BEGIN - 1;
    }
    for (int i = SERVER_PORT_BEGIN; i <= SERVER_PORT_END; i++) {
        if (isQuiting()) {
            return ERR_FALSE;
        }

        g_nPortDownloadServer++;
        if (g_nPortDownloadServer > SERVER_PORT_END) {
            g_nPortDownloadServer = SERVER_PORT_BEGIN;
        }
        httpClient.setHostPort(g_nPortDownloadServer);
        nRet = httpClient.connect();
        if (nRet == ERR_OK) {
            break;
        }
    }

    return nRet;
}

int CDownloadMgr::runHttpTask(CDownloadTask *pTask) {
    CHttpClient httpClient;
    int nHttpCode = 0;
    string &buffRet = pTask->buffContent;
    int nRet;
    int iRetry;

    int nProxyPort = 80;
    string proxyServer;
    bool bUseProxy = CMLProfile::inetGetProxy(proxyServer, nProxyPort);
    string strProxyPwd = CMLProfile::inetGetBase64ProxyUserPass();

    // LOG1(LOG_LVL_INFO, "Enter lyrics download thread: %s", pTask->m_strURL.c_str());
    // LOG2(LOG_LVL_INFO, "Proxy settings: %d, %s", bUseProxy, szProxyPwd);

    bool bMLServer = false;

    httpClient.setUserAgent(getAppNameLong().c_str());
    nRet = httpClient.init(pTask->m_strURL.c_str(), nullptr);
    if (nRet != ERR_OK) {
        ERR_LOG1("Failed to init http url: %s",pTask->m_strURL.c_str());
        nRet = ERR_HTTP_BAD_REQUEST;
        nHttpCode = 0;
        goto RETURN_END;
    }

    if (httpClient.getHostPort() == 80) {
        bMLServer = true;
        httpClient.setHostPort(g_nPortDownloadServer);
    }

    httpClient.setProxy(bUseProxy, proxyServer.c_str(), nProxyPort, strProxyPwd.c_str());

    for (iRetry = 1; iRetry <= 2 && !isQuiting(); iRetry++) {
        if (iRetry != 1) {
            //
            // wait for 10 sec to try again
            if (pTask->taskType == DTT_LYRICS) {
                MPlayerApp::getInstance()->dispatchLongErrorText(_TLT("Wait 10 seconds and retry."));
            }

            if (m_eventShutDown.acquire(10 * 1000)) {
                break;
            }
        }

        if (bUseProxy || !bMLServer) {
            nRet = httpClient.connect();
        } else {
            nRet = tryToConnect(httpClient);
        }
        if (nRet != ERR_OK) {
            string str;
            str = _TLT("Failed to download lyrics with error:");
            str += "\r\n";
            str += (cstr_t)ERROR2STR_LOCAL(nRet);
            if (pTask->taskType == DTT_LYRICS) {
                MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str(), getStrName(SN_HTTP_FAQ_INET));
            }
            DBG_LOG1("%s", str.c_str());

            continue;
        }

        if (isQuiting()) {
            break;
        }

        nRet = httpClient.sendRequest();
        if (nRet != ERR_OK) {
            string str;
            str = _TLT("Failed to download lyrics with error:");
            str += "\r\n";
            str += (cstr_t)ERROR2STR_LOCAL(nRet);
            if (pTask->taskType == DTT_LYRICS) {
                MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str(), getStrName(SN_HTTP_FAQ_INET));
            }
            DBG_LOG1("%s", str.c_str());
            httpClient.close();
            continue;
        }

        if (isQuiting()) {
            break;
        }

        nRet = httpClient.getRespond(nHttpCode, buffRet);
        if (nRet != ERR_OK) {
            string str;
            str = _TLT("Failed to download lyrics with error:");
            str += "\r\n";
            str += (cstr_t)ERROR2STR_LOCAL(nRet);
            if (pTask->taskType == DTT_LYRICS) {
                MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str(), getStrName(SN_HTTP_FAQ_INET));
            }
            DBG_LOG1("%s", str.c_str());
            httpClient.close();
            if (nHttpCode != 0) {
                break;
            }
            continue;
        }

        httpClient.close();

        break;
    }

RETURN_END:
    pTask->m_nHttpRetCode = nHttpCode;
    pTask->m_errResult = nRet;
    if (!(nRet == ERR_OK && nHttpCode == 200)) {
        DBG_LOG3("Downloaded Failed, Error: %d, httpcode: %d, %s", nRet, nHttpCode, pTask->m_strURL.c_str());
    }

    return ERR_OK;
}

int CDownloadMgr::saveDownloadedLyrics(cstr_t szMediaFile, cstr_t szLyrFileName, const void *lpData, int nSize, uint32_t nLyrSaveFlag) {
    assert(lpData && nSize > 0 && nSize < 1024 * 200);
    VecStrings vEmbeddedLyrNames;

    // cancel old lyrics association.
    g_LyricSearch.cancelAssociate(szMediaFile);

    if (nLyrSaveFlag == LST_NONE) {
        // load default settings
        if (g_profile.getInt(SZ_SECT_LYR_DL, "DownSaveInSongDir", DOWN_SAVE_IN_CUSTOM_DIR)
            != DOWN_SAVE_NO_FILE) {
            nLyrSaveFlag = LST_FILE;
        }

        // save lyrics in song file?
        if (g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveEmbeded", false)) {
            if (g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveId3v2Sylt", true)) {
                vEmbeddedLyrNames.push_back(SZ_SONG_ID3V2_SYLT);
            }
            if (g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveId3v2Uslt", true)) {
                vEmbeddedLyrNames.push_back(SZ_SONG_ID3V2_USLT);
            }
            if (g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveLyrics3v2", false)) {
                vEmbeddedLyrNames.push_back(SZ_SONG_LYRICS3V2);
            }
            if (g_profile.getBool(SZ_SECT_LYR_DL, "DownSaveLyricsM4a", false)) {
                vEmbeddedLyrNames.push_back(SZ_SONG_M4A_LYRICS);
            }
        }
    }

    bool bLrcSaved = false;
    int nRet = ERR_OK;

    if (vEmbeddedLyrNames.size() > 0
            && MediaTags::canSaveEmbeddedLyrics(szMediaFile)) {
        string lyrics((cstr_t)lpData, nSize);
        lyrics = convertBinLyricsToUtf8(lyrics, false, ED_SYSDEF);
        nRet = g_autoProcessEmbeddedLyrics.saveEmbeddedLyrics(szMediaFile, lyrics, vEmbeddedLyrNames);
        if (nRet == ERR_OK) {
            bLrcSaved = true;
        }
    }

    if (nLyrSaveFlag & LST_FILE || !bLrcSaved) {
        string strLyrFile = getSaveLyricsFile(szMediaFile, szLyrFileName);
        DBG_LOG1("save downloaded lyrics: %s", strLyrFile.c_str());
        nRet = saveDownloadedLyrAsFile(strLyrFile, lpData, nSize);
        if (nRet == ERR_OK) {
            g_LyricSearch.associateLyrics(szMediaFile, strLyrFile.c_str());
        }
    }

    return nRet;
}

int CDownloadMgr::saveDownloadedLyrAsFile(string &strFile, const void *lpData, int nSize) {
    if (isFileExist(strFile.c_str())) {
        setFileNoReadOnly(strFile.c_str());
    }

    strFile = fileNameFilterInvalidChars(strFile.c_str());

    if (!writeFile(strFile.c_str(), lpData, nSize)) {
        string strDir = fileGetPath(strFile.c_str());
        createDirectoryAll(strDir.c_str());

        if (!writeFile(strFile.c_str(), lpData, nSize)) {
            if (m_strDefSavePath.empty() || !isDirWritable(m_strDefSavePath.c_str())) {
                createDefaultLyricsDir(m_strDefSavePath);
            }

            // save in the default directory
            string strSaveFile = m_strDefSavePath;
            dirStringAddSep(strSaveFile);
            strSaveFile += fileGetName(strFile.c_str());
            strFile = strSaveFile;

            strDir = fileGetPath(strFile.c_str());
            createDirectoryAll(strDir.c_str());
            if (!writeFile(strFile.c_str(), lpData, nSize)) {
                ERR_LOG1("Failed to save downloaded lyrics file: %s.", strFile.c_str());
                return ERR_OPEN_FILE;
            }
        }
    }

    return ERR_OK;
}
