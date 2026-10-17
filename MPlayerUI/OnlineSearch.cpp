#include "MPlayerApp.h"
#include "../third-parties/sqlite/Sqlite3.hpp"
#include "OnlineSearch.h"
#include "DownloadMgr.h"
#include "Helper.h"
#include "../LyricsLib/LyricsKeywordFilter.h"
#include "../Utils/RegExTool.h"


COnlineSearch g_OnlineSearch;

/*
CREATE TABLE IF NOT EXISTS SearchResults
(
  song_file text,
  modify_time integer,
  xml_data text
);

CREATE INDEX IF NOT EXISTS SR_keyword on SearchResults (song_file);
CREATE INDEX IF NOT EXISTS SR_modify_time on SearchResults (modify_time);
*/

#define SQL_CREATE_SEARCHRESULT        "CREATE TABLE IF NOT EXISTS SearchResults"\
    "("\
    "song_file text,"\
    "modify_time integer,"\
    "xml_data text"\
    ");"\
    "CREATE INDEX IF NOT EXISTS SR_keyword on SearchResults (song_file);"\
    "CREATE INDEX IF NOT EXISTS SR_modify_time on SearchResults (modify_time);"

#define SQL_DROP_SEARCHRESULT        "DROP TABLE SearchResults;"\
    "DROP INDEX SR_keyword;"\
    "DROP INDEX SR_modify_time;"

#define SQL_ADD_SEARCHRESULT    "insert into SearchResults (song_file, modify_time, xml_data) VALUES ("\
                        "?, ?, ?)"

#define SQL_QUERY_SEARCHRESULT  "select * from SearchResults where song_file=?"
//#define SQL_QUERY_SEARCHRESULT    "select * from SearchResults"

#define SQL_DEL_SEARCHRESULT "delete from SearchResults where song_file=?"

void retLyrInfoListToVResult(CLyricsSearchParameter &searchParam, RetLyrInfoList &filelist, ListLyrSearchResults &vResult) {
    for (RetLyrInfoList::iterator it = filelist.begin(); it != filelist.end(); it++) {
        RetLyrInfo &file = *it;
        LrcSearchResult item;
        item.strUrl = file.strLink;
        item.strSaveFileName = file.getFileName();
        item.strArtist = file.strArtist;
        item.strTitle = file.strTitle;
        item.strAlbum = file.strAlbum;
        item.strUploader = file.strUploader;
        item.fRate = file.fRate;
        item.nRateCount = file.nRateCount;
        item.nDownloads = file.nDownloads;
        item.nMatchValue = (float)searchParam.calMatchValueByTitle(item.strArtist.c_str(), item.strTitle.c_str(), getLyricsContentTypeByFileExt(item.strSaveFileName.c_str()));
        // DBG_LOG2("file: %s, value: %d", item.strSaveFileName.c_str(), item.nMatchValue);

        // sort English result before other language.
        if (isAnsiStr(item.strSaveFileName.c_str())) {
            item.nMatchValue += 0.5;
        }

        // increase rating
        if (item.nRateCount > 0) {
            item.nMatchValue += item.fRate - 3;
            if (item.nRateCount > 5) {
                if (item.fRate >= 4.8) {
                    item.nMatchValue++;
                } else if (item.fRate <= 3.0) {
                    item.nMatchValue--;
                }
            }
            item.nMatchValue += item.nRateCount / (float)1000.0;
        }

        vResult.addResult(item);
    }

    vResult.sort();
}

int getCurDaysFrom2000() {
    DateTime dateNow = DateTime::localTime();
    DateTime date2000(2000, 1, 1);

    return (int)((dateNow.getTime() - date2000.getTime()) / DateTime::SECOND_IN_ONE_DAY);
}

static cstr_t SZ_IGNOR_NAMES[] = { ".720p", ".1080p", };

static cstr_t SZ_IGNOR_EXTS[] = { ".mkv", ".flv", ".rmvb", ".mp4", ".dat", ".vob" };

bool isTitleIgnoredStep1(cstr_t szTitle) {
    // Ignore if includes: .720p, 1080p
    string strTitleLower = toLower(szTitle);
    for (int i = 0; i < CountOf(SZ_IGNOR_NAMES); i++) {
        if (strstr(strTitleLower.c_str(), SZ_IGNOR_NAMES[i]) != 0) {
            return true;
        }
    }

    // Ignore if title ends with movie extention: .mkv, .flv .avi .mp4
    for (int i = 0; i < CountOf(SZ_IGNOR_EXTS); i++) {
        if (endsWith(strTitleLower.c_str(), SZ_IGNOR_EXTS[i])) {
            return true;
        }
    }

    return false;
}

bool isTitleIgnoredStep2(cstr_t szTitle) {
    string strTitleFiltered;

    CLyricsKeywordFilter::filter(szTitle, strTitleFiltered);
    if (strTitleFiltered.empty()) {
        return true;
    }

    static cstr_t szExcludedTitle[] = { "track", "music", "cd", "record", "movie", "wav", "song", "unknown", "unknown title" };

    for (int i = 0; i < CountOf(szExcludedTitle); i++) {
        if (strcasecmp(szExcludedTitle[i], strTitleFiltered.c_str()) == 0) {
            return true;
        }
        if (strcasecmp(_TL(szExcludedTitle[i]), strTitleFiltered.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

bool isArtistIgnoredStep2(cstr_t szArtist) {
    string strArtistFiltered;

    CLyricsKeywordFilter::filter(szArtist, strArtistFiltered);
    if (strArtistFiltered.empty()) {
        return true;
    }

    static cstr_t szExcludedArtist[] = { "unknown", "http", "unknown artist", "unknown artists" };

    for (int i = 0; i < CountOf(szExcludedArtist); i++) {
        if (strcmp(szExcludedArtist[i], strArtistFiltered.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

static cstr_t SZ_DOMAIN_NAMES[] = {
    "[[\\w\\-]+\\.[[\\w\\-]+\\.[\\w\\-]+\\.(com|info|fm|blogspot|net|org)\\b",
                 "[[\\w\\-]+\\.[\\w\\-]+\\.(com|info|fm|blogspot|net|org)\\b",
                              "[\\w\\-]+\\.(com|info|fm|blogspot|net|org)\\b",
    "www\\b\\.[[\\w\\-]+\\.[[\\w\\-]+\\.[\\w\\-]+",
    "www\\b\\.[[\\w\\-]+\\.[[\\w\\-]+",
};

cstr_t SZ_SYMBOLS_TRIM = " \t_-!~@#$%^&*'\"?/\\|+=";

string removeSpamInTag(cstr_t szValue) {
    string strValueLower = toLower(szValue);

    // remove xxx.xxx.com domain names:
    string strNewValue = szValue;
    for (int i = 0; i < CountOf(SZ_DOMAIN_NAMES); i++) {
        strNewValue = regExSub(SZ_DOMAIN_NAMES[i], strNewValue.c_str(), "");
    }

    // remove http:
    strrep(strNewValue, "http:", "");

    trimStr(strNewValue, SZ_SYMBOLS_TRIM);

    return strNewValue;
}

//
// Return false: Indicate that the spam info was failed to remove from tag, should stop search lyrics.
//
bool filterSpamInfo(string &strArtist, string &strTitle) {
    if (uriIsQuoted(strArtist.c_str())) {
        strArtist = uriUnquote(strArtist.c_str());
    }

    if (uriIsQuoted(strTitle.c_str())) {
        strArtist = uriUnquote(strTitle.c_str());
    }

    if (isTitleIgnoredStep1(strTitle.c_str())) {
        return false;
    }

    strTitle = removeSpamInTag(strTitle.c_str());
    strArtist = removeSpamInTag(strArtist.c_str());

    // If title = 'artist - title', remove extra artist in title.
    if (iStartsWith(strTitle.c_str(), strArtist.c_str())) {
        string strNewTitle(strTitle.c_str() + strArtist.size());
        trimStr(strNewTitle, " \t");

        int c = strNewTitle[0];
        // Artist is long enough or with separator - . _ etc.
        if (strArtist.size() >= 15 || c == '_' || c == '-' || c == '.' || c == '/') {
            strTitle = strNewTitle;
            trimStr(strTitle, SZ_SYMBOLS_TRIM);
        }
    }

    // If title contains 01. xxxx, remove begin numbers.
    if (strTitle.size() > 4
        && isDigit(strTitle[0])
        && isDigit(strTitle[1])
        && strTitle[2] == '.' && strTitle[3] == ' ') {
        strTitle.erase(0, 4);
    }

    // Check whether title is in excluded from searching.
    if (isTitleIgnoredStep2(strTitle.c_str())) {
        return false;
    }

    // Check whether artist is in excluded from searching.
    if (isArtistIgnoredStep2(strArtist.c_str())) {
        return false;
    }

    return true;
}

void ILyrSearchResultCacheDB::resultToXML(MLMsgRetSearch &retSearch, CXMLWriter &xmlWriter) {
    xmlWriter.writeStartElement("SearchResults");

    string strEmptyServerUrl;
    for (list<RetLyrInfo>::iterator it = retSearch.listResultFiles.begin();
    it != retSearch.listResultFiles.end(); it++)
        {
        assert(iStartsWith((*it).strLink.c_str(), "http://"));
        (*it).toXML(xmlWriter, strEmptyServerUrl, true);
    }

    xmlWriter.writeEndElement();
}

void ILyrSearchResultCacheDB::resultFromXML(MLMsgRetSearch &retSearch, cstr_t szXML) {
    CSimpleXML xml;
    SXNode *pNode;

    if (!xml.parseData((void*)szXML, strlen(szXML))) {
        return;
    }

    pNode = xml.m_pRoot;
    if (strcmp(pNode->name.c_str(), "SearchResults") != 0) {
        return;
    }

    SXNode::iterator it;
    string strEmptyServerUrl;
    for (it = pNode->listChildren.begin(); it != pNode->listChildren.end(); it++) {
        RetLyrInfo Info;
        Info.fromXML(*it, strEmptyServerUrl.c_str());
        assert(iStartsWith(Info.strLink.c_str(), "http://"));
        retSearch.listResultFiles.push_back(Info);
    }
}

CLyrSearchResultCacheDB::CLyrSearchResultCacheDB() {
}

CLyrSearchResultCacheDB::~CLyrSearchResultCacheDB() {
    close();
}

bool CLyrSearchResultCacheDB::open() {
    int nRet;
    string strFileT;

    strFileT = getAppDataDir();
    strFileT += "dbSearchCache.db";

    if (getFileLength(strFileT.c_str()) >= 1024 * 1024 * 2) {
        deleteFile(strFileT.c_str());
    }

    nRet = m_db.open(strFileT.c_str());
    if (nRet != ERR_OK) {
        return false;
    }

    /*nRet = sqlite3_exec(m_db, SQL_DROP_SEARCHRESULT, nullptr, nullptr, nullptr);
    if (nRet != SQLITE_OK)
    {
        LogSqlite3Error(m_db);
    }*/

    nRet = m_db.exec(SQL_CREATE_SEARCHRESULT);
    if (nRet != ERR_OK) {
        return false;
    }

    return true;
}

void CLyrSearchResultCacheDB::close() {
    m_db.close();
}

bool CLyrSearchResultCacheDB::searchCache(cstr_t szFileAssociateKeyword, cstr_t szArtist, cstr_t szTitle, ListLyrSearchResults *pvResult, uint32_t *pdwSearchTime) {
    int nRet;
    CSqlite3Stmt sqlQuery;

    nRet = sqlQuery.prepare(&m_db, SQL_QUERY_SEARCHRESULT);
    if (nRet != ERR_OK) {
        return false;
    }

    sqlQuery.bindStaticText(1, szFileAssociateKeyword, strlen(szFileAssociateKeyword));

    nRet = sqlQuery.step();
    if (nRet != ERR_SL_OK_ROW) {
        return false;
    }

    if (pdwSearchTime) {
        *pdwSearchTime = sqlQuery.columnInt(1);
    }

    if (pvResult) {
        cstr_t str = sqlQuery.columnText(2);
        if (str) {
            MLMsgRetSearch retSearch;
            resultFromXML(retSearch, str);

            CLyricsSearchParameter searchParam(szFileAssociateKeyword, szArtist, szTitle);
            retLyrInfoListToVResult(searchParam, retSearch.listResultFiles, *pvResult);
        }
    }

    return true;
}

bool CLyrSearchResultCacheDB::updateSearchLrcInfo(cstr_t szFileAssociateKeyword, MLMsgRetSearch &retSearch) {
    int nRet;
    int n = 1;
    CXMLWriter xml;
    CSqlite3Stmt sqlDelete;
    CSqlite3Stmt sqlAdd;

    resultToXML(retSearch, xml);

    // delete old one
    nRet = sqlDelete.prepare(&m_db, SQL_DEL_SEARCHRESULT);
    if (nRet != ERR_OK) {
        return false;
    }

    sqlDelete.bindStaticText(1, szFileAssociateKeyword, strlen(szFileAssociateKeyword));

    nRet = sqlDelete.step();
    if (nRet != ERR_OK) {
        return false;
    }
    sqlDelete.finalize();

    // add new
    nRet = sqlAdd.prepare(&m_db, SQL_ADD_SEARCHRESULT);
    if (nRet != ERR_OK) {
        return false;
    }

    n = 1;
    sqlAdd.bindStaticText(n++, szFileAssociateKeyword, strlen(szFileAssociateKeyword));
    sqlAdd.bindInt(n++, getCurDaysFrom2000());
    string &buf = xml.getBuffer();
    sqlAdd.bindStaticText(n++, buf.c_str(), buf.size());

    nRet = sqlAdd.step();
    if (nRet != ERR_OK) {
        return false;
    }

    return true;
}

CLyrSearchResultCacheMemDB::ListItems CLyrSearchResultCacheMemDB::m_listResults;

bool CLyrSearchResultCacheMemDB::searchCache(cstr_t szFileAssociateKeyword, cstr_t szArtist, cstr_t szTitle, ListLyrSearchResults *pvResult, uint32_t *pdwSearchTime) {
    CLyricsSearchParameter searchParam(szFileAssociateKeyword, szArtist, szTitle);

    for (ListItems::iterator it = m_listResults.begin(); it != m_listResults.end(); ++it) {
        ITEM &item = *it;
        if (strcmp(item.strKeyword.c_str(), szFileAssociateKeyword) == 0) {
            if (pdwSearchTime) {
                *pdwSearchTime = getCurDaysFrom2000();
            }

            if (pvResult) {
                cstr_t str = item.strXml.c_str();
                if (str) {
                    MLMsgRetSearch retSearch;
                    resultFromXML(retSearch, str);

                    retLyrInfoListToVResult(searchParam, retSearch.listResultFiles, *pvResult);
                }
            }

            return true;
        }
    }

    return false;
}

bool CLyrSearchResultCacheMemDB::updateSearchLrcInfo(cstr_t szFileAssociateKeyword, MLMsgRetSearch &retSearch) {
    ListItems::iterator it, itEnd;
    CXMLWriter xml;

    resultToXML(retSearch, xml);

    if (m_listResults.size() >= 5) {
        m_listResults.pop_front();
    }

    itEnd = m_listResults.end();
    for (it = m_listResults.begin(); it != itEnd; ++it) {
        ITEM &item = *it;
        if (strcmp(item.strKeyword.c_str(), szFileAssociateKeyword) == 0) {
            item.strXml = xml.getBuffer().c_str();
            return true;
        }
    }

    ITEM item;

    item.strKeyword = szFileAssociateKeyword;
    item.strXml = xml.getBuffer().c_str();

    m_listResults.push_back(item);

    return true;
}

void CLyrSearchResultCacheMemDB::free() {
    m_listResults.clear();
}


COnlineSearch::COnlineSearch() {
    m_bRunning = false;
}

COnlineSearch::~COnlineSearch() {

}

bool COnlineSearch::init() {
    return true;
}

void COnlineSearch::quit() {
    // 不 join：搜索线程可能堵在网络；destroy() 也只是清 pthread id，并不能取消 I/O。
    m_bRunning = false;
    m_dbSearchResult.close();
    m_dbSearchResultMem.close();
}

bool COnlineSearch::searchCacheForCur(ListLyrSearchResults *pvResult, uint32_t *pdwSearchTime) {
    string strAssociateMediaKey = g_player.getMediaKey();

    MutexAutolock autolock(m_mutexDB);

    if (m_dbSearchResultMem.searchCache(strAssociateMediaKey.c_str(),
        g_player.getArtist(), g_player.getTitle(), pvResult, pdwSearchTime)) {
        return true;
    }

    if (!m_dbSearchResult.isOpened()) {
        m_dbSearchResult.open();
    }

    if (m_dbSearchResult.isOpened()) {
        return m_dbSearchResult.searchCache(strAssociateMediaKey.c_str(),
            g_player.getArtist(), g_player.getTitle(), pvResult, pdwSearchTime);
    }

    return false;
}

void COnlineSearch::threadSearchOnline(void *lpParam) {
    int nRet;
    CMLClientSession session;
    MLMsgRetSearch retSearch;

    COnlineSearchThreadParam *pSearchParam = (COnlineSearchThreadParam *)lpParam;
    assert(pSearchParam);

    pSearchParam->pOnlineSearch->m_bRunning = true;

    session.init(getAppNameLong().c_str());

    nRet = session.connect();
    if (nRet != ERR_OK) {
        string str;
        str += _TLT("Failed to search lyrics with error:");
        str += " ";
        str += cstr_t(ERROR2STR_LOCAL(nRet));
        MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str());

        pSearchParam->pOnlineSearch->m_bRunning = false;
        delete pSearchParam;
        return;
    }

    nRet = session.search(pSearchParam->bOnlyMatched, pSearchParam->strArtist.c_str(), pSearchParam->strTitle.c_str(), 0, retSearch);
    session.close();

    if (retSearch.strMessage.size() && MPlayerApp::getMainWnd()) {
        MPlayerApp::getInstance()->messageOut(retSearch.strMessage.c_str());
    }

    if (retSearch.listResultFiles.size() > 0 || nRet == ERR_NOT_FOUND) {
        pSearchParam->pOnlineSearch->updateSearchLrcInfo(pSearchParam->strAssociateMediaKey.c_str(), retSearch);

        //
        // 通知给调用者
        IEvent *pEvent = new IEvent;
        pEvent->eventType = ET_LYRICS_SEARCH_END;
        MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);
    } else {
        string str;
        str += _TLT("Failed to search lyrics with error:");
        str += " ";
        str += (cstr_t)ERROR2STR_LOCAL(nRet);
        MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str());
    }

    pSearchParam->pOnlineSearch->m_bRunning = false;
    delete pSearchParam;
}

// 创建一个线程搜索歌词，线程执行完毕之后会通知目标
bool COnlineSearch::addSearchJob(bool bOnlyMatched, cstr_t szMediaKey, cstr_t szArtist, cstr_t szTitle) {
    if (m_bRunning) {
        return false;
    }

    COnlineSearchThreadParam *pParam = new COnlineSearchThreadParam;
    pParam->strArtist = szArtist;
    pParam->strTitle = szTitle;
    pParam->strAssociateMediaKey = g_player.getMediaKey();
    pParam->pOnlineSearch = this;
    pParam->bOnlyMatched = bOnlyMatched;

    m_threadSearch.create(threadSearchOnline, pParam);

    return true;
}

bool COnlineSearch::autoSearch() {
    string strArtist = g_player.getArtist();
    string strTitle = g_player.getTitle();

    // Won't search is title is too short, or artist is empty.
    if (strTitle.size() < 2 && strArtist.size() == 0) {
        return false;
    }

    if (!filterSpamInfo(strArtist, strTitle)) {
        return false;
    }

    string str;
    str = _TLT("Searching lyrics");
    str += ": ";
    str += g_player.getFullTitle();
    MPlayerApp::getInstance()->dispatchLongErrorText(str.c_str());

    return addSearchJob(true, g_player.getMediaKey().c_str(), strArtist.c_str(), strTitle.c_str());
}

int COnlineSearch::searchOnline(CMLClientSession &session, cstr_t szArtist, cstr_t szTitle, cstr_t szMediaKey, ListLyrSearchResults &vResult, string &strMsg, int &nCurPage, int &nPageCount) {
    int nRet;

    string strAssociateMediaKey = szMediaKey;

    session.init(getAppNameLong().c_str());

    nRet = session.connect();
    if (nRet != ERR_OK) {
        return nRet;
    }

    MLMsgRetSearch retSearch;

    nRet = session.search(false, szArtist, szTitle, nCurPage, retSearch);
    session.close();

    if (retSearch.strMessage.size() && MPlayerApp::getMainWnd()) {
        MPlayerApp::getInstance()->messageOut(retSearch.strMessage.c_str());
    }

    nCurPage = retSearch.nCurPage;
    nPageCount = retSearch.nPageCount;

    if (retSearch.listResultFiles.size() > 0) {
        updateSearchLrcInfo(strAssociateMediaKey.c_str(), retSearch);

        CLyricsSearchParameter searchParam(szMediaKey, szArtist, szTitle);
        retLyrInfoListToVResult(searchParam, retSearch.listResultFiles, vResult);
        if (vResult.front().nMatchValue >= MATCH_VALUE_OK) {
            return ERR_OK;
        }
    }

    //     // Use LyricsInetSearch to search for results.
    //     m_lyricsInetSearch.search(szArtist, szTitle, vResult);
    //     if (vResult.size() > 0)
    //         return ERR_OK;

    return nRet;
}

int COnlineSearch::batchSearch(CMLClientSession &session, MLListSearchItems &listSearch, MLMsgRetBatchSearch &retLyrics) {
    session.init(getAppNameLong().c_str());

    int nRet = session.connect();
    if (nRet != ERR_OK) {
        return nRet;
    }

    nRet = session.batchSearch(listSearch, retLyrics);
    session.close();

    return nRet;
}

int COnlineSearch::batchSearch(CMLClientSession &session, MLListSearchItems &listSearch, uint32_t nLyrSaveFlag, string &strMsg, INotifyEvent *pNotify) {
    MLMsgRetBatchSearch retLyrics;
    int nRet = batchSearch(session, listSearch, retLyrics);
    if (nRet != ERR_OK) {
        return nRet;
    }

    if (retLyrics.strMessage.size() && MPlayerApp::getMainWnd()) {
        MPlayerApp::getInstance()->messageOut(retLyrics.strMessage.c_str());
    }

    if (listSearch.size() != retLyrics.listLyricsInfo.size()) {
        assert(0 && "Invalid protocol.");
        return ERR_BAD_MSG;
    }

    int i = 0;
    for (ListLyricsInfoLite::iterator it = retLyrics.listLyricsInfo.begin();
    it != retLyrics.listLyricsInfo.end() && i < (int)listSearch.size(); ++it, i++)
        {
        MLSearchItem &search = listSearch[i];
        MLLyricsInfoLite &lyr = *it;

        if (lyr.bufLyrContent.empty()) {
            if (pNotify) {
                pNotify->onNoLyricsReturned(i);
            }

            g_LyricSearch.associateLyrics(search.strMediaFile.c_str(), NONE_LYRCS);
        } else {
            int nRet = g_LyricsDownloader.saveDownloadedLyrics(search.strMediaFile.c_str(), lyr.strFile.c_str(), lyr.bufLyrContent.c_str(), (int)lyr.bufLyrContent.size());
            if (pNotify) {
                pNotify->onLyricsReturned(i, lyr.bufLyrContent);
                if (nRet != ERR_OK) {
                    pNotify->onErrorSaveLyrics(i, ERROR2STR_LOCAL(nRet));
                }
            }
        }
    }

    return nRet;
}

int COnlineSearch::updateSearchLrcInfo(cstr_t szFileAssociateKeyword, MLMsgRetSearch &retSearch) {
    if (isEmptyString(szFileAssociateKeyword)) {
        return ERR_NOT_FOUND;
    }

    MutexAutolock autolock(m_mutexDB);

    m_dbSearchResultMem.updateSearchLrcInfo(szFileAssociateKeyword, retSearch);

    if (!m_dbSearchResult.isOpened()) {
        m_dbSearchResult.open();
    }

    if (m_dbSearchResult.isOpened()) {
        m_dbSearchResult.updateSearchLrcInfo(szFileAssociateKeyword, retSearch);
    }

    return ERR_OK;
}

#if UNIT_TEST

#include "utils/unittest.h"


TEST(OnlineSearch, FilterSearchKeywords) {
    cstr_t cases[] = { "artist.movie.720p.xxxx", "artist.movie.1080p.xxx",
        "artist.movie.mkv", "artist.movie.mp4", };

        for (int i = 0; i < CountOf(cases); i++) {
        ASSERT_TRUE(isTitleIgnoredStep1(cases[i]));

        string strTitle = cases[i], strArtist = "artist";
        ASSERT_TRUE(filterSpamInfo(strArtist, strTitle) == false);
    }

    ASSERT_TRUE(isTitleIgnoredStep1("artist title.mp3") == false);
}

TEST(OnlineSearch, RemoveSpamInTag) {
    CLyricsKeywordFilter::init(getAppResourceFile("LyrKeywordFilter.xml").c_str());

    cstr_t cases[] = { "www.test.com - abc", "abc | ass.com",
        "http://a.com | title", "hi.come", };

        cstr_t results[] = { "abc", "abc",
        "title", "hi.come", };

        for (int i = 0; i < CountOf(cases); i++) {
        string r = removeSpamInTag(cases[i]);
        ASSERT_TRUE(strcmp(r.c_str(), results[i]) == 0);

        string strTitle = cases[i], strArtist = "artist";
        ASSERT_TRUE(filterSpamInfo(strArtist, strTitle));
        ASSERT_TRUE(strcmp(strTitle.c_str(), results[i]) == 0);
        ASSERT_TRUE(strcmp(strArtist.c_str(), "artist") == 0);
    }
}

TEST(OnlineSearch, FilterSpamInfo) {
    string strTitle = "artist must be long - title", strArtist = "artist must be long";
    ASSERT_TRUE(filterSpamInfo(strArtist, strTitle));
    ASSERT_TRUE(strcmp(strArtist.c_str(), "artist must be long") == 0);
    ASSERT_TRUE(strcmp(strTitle.c_str(), "title") == 0);

    strTitle = "01. title";
    ASSERT_TRUE(filterSpamInfo(strArtist, strTitle));
    ASSERT_TRUE(strcmp(strArtist.c_str(), "artist must be long") == 0);
    ASSERT_TRUE(strcmp(strTitle.c_str(), "title") == 0);

    strTitle = "track";
    ASSERT_TRUE(filterSpamInfo(strArtist, strTitle) == false);
}

#endif
