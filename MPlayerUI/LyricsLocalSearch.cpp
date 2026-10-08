/********************************************************************
    Created  :    2002年1月2日 22:54:43
    FileName :    LyricsLocalSearch.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "LyricsLocalSearch.h"


cstr_t SZ_FOLDER_PREFIX = "LyricFolder";

#define SZ_S2L_HEADER_V10   "MPS2LV1.0"
#define LEN_S2L_HEADER_V10  9

#define SZ_S2L_HEADER       "MPS2LV1.1"
#define LEN_S2L_HEADER      9

// 根据关联 keyword 在媒体库中查找对应的歌曲（大小写不敏感）。
static MediaPtr getMediaByAssociateKeyword(cstr_t szAssociateFileKeyword);

// rule: 1) http://xxxx
//       2) without the extention of song file.
bool isShoutcastMedia(cstr_t szMedia) {
    if (strncasecmp(szMedia, "http:", 5) == 0) {
        return true;
    } else if (strncasecmp(szMedia, "uvox:", 5) == 0) {
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////

CLyricsLocalSearch::CLyricsLocalSearch() {
    m_bSaved = true;
}

CLyricsLocalSearch::~CLyricsLocalSearch() {
}

void CLyricsLocalSearch::loadLyricFolderCfg() {
    for (int i = 0; ; i ++) {
        char szKeyName[MAX_PATH];

        // get folder name
        snprintf(szKeyName, CountOf(szKeyName), "%s%d", SZ_FOLDER_PREFIX, i);
        string strFolder = CMLProfile::getDir(SZ_SECT_SEARCH_FOLDER, szKeyName, "");
        if (strFolder.size()) {
            m_vLyricsFolders.push_back(strFolder);
        } else {
            break;
        }
    }
}

void CLyricsLocalSearch::saveLyricFolderCfg() {
    if (m_bSaved) {
        return;
    }

    m_bSaved = true;

    char szKeyName[MAX_PATH];

    for (uint32_t i = 0; i < m_vLyricsFolders.size(); i++) {
        // SAVE folder settings
        snprintf(szKeyName, CountOf(szKeyName), "%s%d", SZ_FOLDER_PREFIX, i);
        CMLProfile::writeDir(SZ_SECT_SEARCH_FOLDER, szKeyName, m_vLyricsFolders[i].c_str());
    }

    // set end positions.
    snprintf(szKeyName, CountOf(szKeyName), "%s%d", SZ_FOLDER_PREFIX, (int)m_vLyricsFolders.size());
    g_profile.writeString(SZ_SECT_SEARCH_FOLDER, szKeyName, "");
}

void CLyricsLocalSearch::searchAllMatchLyrics(cstr_t szSongFile, cstr_t szArtist, cstr_t szTitle, ListLyrSearchResults &vLyrics) {
    CLyricsSearchParameter searchParam(szSongFile, szArtist, szTitle);
    searchLyrics(searchParam, vLyrics);
    //
    //     if (vLyrics.size() == 0 && !isEmptyString(szSongFile))
    //     {
    //         string strArtist, strTitle;
    //         getArtistTitleFromFileName(strArtist, strTitle, szSongFile);
    //         if (strTitle.empty()
    //             || strcasecmp(szArtist, strArtist.c_str()) == 0 && strcasecmp(szTitle, strTitle.c_str()) == 0)
    //             return;
    //
    //         CLyricsSearchParameter searchParam(szSongFile, szArtist, szTitle);
    //         if (searchParam.strTitleFiltered.size() > 0)
    //             searchLyrics(searchParam, &vLyrics);
    //     }
}

// search lyrics by artist and title.
void CLyricsLocalSearch::searchLyrics(CLyricsSearchParameter &searchParam, ListLyrSearchResults &vLyrics) {
    searchEmbeddedLyrics(searchParam.szSongFile, vLyrics, searchParam.bOnlySearchBestMatch);
    if (searchParam.bOnlySearchBestMatch) {
        ListLyrSearchResults::iterator it = vLyrics.getTheBestMatchLyrics();
        if (it != vLyrics.end()) {
            LrcSearchResult &result = *it;
            if (result.nMatchValue >= MATCH_VALUE_EMBEDDED_LRC) {
                searchParam.strBestMatchLyrics = result.strUrl.c_str();
                searchParam.nMatchValueOfBest = (int)result.nMatchValue;
                return;
            }
        }
    }

    string strDir;

    // search lyrics in the same folder of song file.
    {
        if (!isEmptyString(searchParam.szSongFile)) {
            strDir = fileGetPath(searchParam.szSongFile);
            searchMatchLyricsInDir(strDir.c_str(), searchParam, vLyrics, false);
            if (searchParam.isBestMatchLyricsFound()) {
                return;
            }
        }
    }

    // search in lyrics download folder
    {
        strDir = g_LyricsDownloader.getDefSavePath();
        dirStringAddSep(strDir);

        searchMatchLyricsInDir(strDir.c_str(), searchParam, vLyrics, true);
        if (searchParam.isBestMatchLyricsFound()) {
            return;
        }
    }

    // search in the folder that user specified.
    for (int i = getSearchFolerCount() - 1; i >= 0; i --) {
        strDir = m_vLyricsFolders[i].c_str();
        dirStringAddSep(strDir);

        searchMatchLyricsInDir(strDir.c_str(), searchParam, vLyrics, true);
        if (searchParam.isBestMatchLyricsFound()) {
            return;
        }
    }

    // Finally use the best match lyrics in vLyrics.
    ListLyrSearchResults::iterator it = vLyrics.getTheBestMatchLyrics();
    if (it != vLyrics.end()) {
        LrcSearchResult &result = *it;
        if (result.nMatchValue >= MATCH_VALUE_OK) {
            searchParam.strBestMatchLyrics = result.strUrl.c_str();
            searchParam.nMatchValueOfBest = (int)result.nMatchValue;
            return;
        }
    }
}

string CLyricsLocalSearch::getFolder(int nIndex) {
    if (nIndex >= 0 && nIndex < m_vLyricsFolders.size()) {
        return m_vLyricsFolders[nIndex];
    } else {
        return "";
    }
}

bool CLyricsLocalSearch::setFolder(cstr_t szFolder) {
    for (int i = getSearchFolerCount() - 1; i >= 0; i --) {
        if (strcasecmp(m_vLyricsFolders[i].c_str(), szFolder) == 0) {
            return false;
        }
    }

    m_bSaved = false;
    m_vLyricsFolders.push_back(szFolder);

    return true;
}

int CLyricsLocalSearch::getSearchFolerCount() {
    return (int)m_vLyricsFolders.size();
}

bool CLyricsLocalSearch::removeFolder(int nIndex) {
    if (nIndex >= getSearchFolerCount() || nIndex < 0) {
        return false;
    }

    m_bSaved = false;

    m_vLyricsFolders.erase(m_vLyricsFolders.begin() + nIndex);

    return true;
}

void CLyricsLocalSearch::init() {
    loadLyricFolderCfg();

    migrateLyricsAssociation();
}

void CLyricsLocalSearch::quit() {
    saveLyricFolderCfg();
    m_vLyricsFolders.clear();
}

char * getDiskImageFileExtEnd(cstr_t szFile) {
    static cstr_t vImageExts[] = { ".cue", ".tak", ".wv" };

    for (int i = 0; i < CountOf(vImageExts); i++) {
        char * p = stristr(szFile, vImageExts[i]);
        if (p) {
            return p + strlen(vImageExts[i]);
        }
    }

    return nullptr;
}

string CLyricsLocalSearch::getAssociateFileKeyword(cstr_t szSongFile, cstr_t szFulltitle) {
    string strAssociateKey;

    if (isEmptyString(szSongFile)) {
        return szFulltitle;
    }

    strAssociateKey = szSongFile;

    char * szEndPos = getDiskImageFileExtEnd(szSongFile);
    if (szEndPos) {
        // to associate a .cue, .tak track correctly, the title will be appended to it.
        if (strcmp(szEndPos, " - ") == 0
            || strcmp(szEndPos, ",") == 0) {
            *szEndPos = '\0';
        }
        strAssociateKey += szFulltitle;
    } else if (isShoutcastMedia(szSongFile)) {
        // to associate a shoutcast track correctly, the title will be appended to it.
        strAssociateKey += "/[shoutcast]";
        strAssociateKey += szFulltitle;
    }

    return strAssociateKey;
}

bool CLyricsLocalSearch::associateLyrics(cstr_t szAssociateFileKeyword, cstr_t szLyricFile) {
    assert(szAssociateFileKeyword);

    if (isEmptyString(szAssociateFileKeyword)) {
        return false;
    }

    ensureLyricsAssociationMigrated();

    // 优先持久化到媒体库（medialib.lyrics_file），这样网页版能读到同一份歌词关联。
    auto media = getMediaByAssociateKeyword(szAssociateFileKeyword);
    if (media) {
        if (media->lyricsFile == szLyricFile) {
            return false;
        }
        media->lyricsFile = szLyricFile;
        if (g_player.getMediaLibrary()) {
            g_player.getMediaLibrary()->updateMediaInfo(media.get());
        }
        return true;
    }

    // 不在媒体库里的歌曲（cue/shoutcast 音轨、或尚未入库），仅在本次会话内存中保留。
    {
        MutexAutolock lock(m_mutex);
        auto it = m_mapLyricsAssociate.find(toAssociateKeyword(szAssociateFileKeyword));
        if (it != m_mapLyricsAssociate.end() && it->second == szLyricFile) {
            return false;
        }
        m_mapLyricsAssociate[toAssociateKeyword(szAssociateFileKeyword)] = szLyricFile;
    }

    return true;
}

bool CLyricsLocalSearch::cancelAssociate(cstr_t szAssociateFileKeyword) {
    ensureLyricsAssociationMigrated();

    // 优先清理媒体库里的关联。
    auto media = getMediaByAssociateKeyword(szAssociateFileKeyword);
    if (media && !media->lyricsFile.empty()) {
        media->lyricsFile.resize(0);
        if (g_player.getMediaLibrary()) {
            g_player.getMediaLibrary()->updateMediaInfo(media.get());
        }
        return true;
    }

    // 不在媒体库里的歌曲，清理会话内缓存。
    {
        MutexAutolock autoLock(m_mutex);
        auto itLyric = m_mapLyricsAssociate.find(toAssociateKeyword(szAssociateFileKeyword));
        if (itLyric != m_mapLyricsAssociate.end()) {
            m_mapLyricsAssociate.erase(itLyric);
            return true;
        }
    }

    return false;
}

bool CLyricsLocalSearch::isAssociatedLyrics(cstr_t szAssociateFileKeyword) {
    assert(szAssociateFileKeyword);

    ensureLyricsAssociationMigrated();

    // 媒体库（持久化）优先。
    auto media = getMediaByAssociateKeyword(szAssociateFileKeyword);
    if (media && !media->lyricsFile.empty()) {
        return true;
    }

    MutexAutolock autoLock(m_mutex);
    return m_mapLyricsAssociate.find(toAssociateKeyword(szAssociateFileKeyword)) != m_mapLyricsAssociate.end();
}

bool CLyricsLocalSearch::isAssociatedWithNoneLyrics(cstr_t szAssociateKeyword) {
    char szLyricsFile[MAX_PATH];

    if (getAssociatedLyrics(szAssociateKeyword, szLyricsFile, MAX_PATH)) {
        if (strcmp(NONE_LYRCS, szLyricsFile) == 0) {
            return true;
        }
    }
    return false;
}

inline bool filePathIsBeginWithDriver(cstr_t szFile) {
    return szFile[0] && szFile[1] == ':';
}

bool CLyricsLocalSearch::getAssociatedLyrics(cstr_t szAssociateFileKeyword, char * szLyricFile, int nMaxBuff) {
    assert(szLyricFile);
    emptyStr(szLyricFile);

    ensureLyricsAssociationMigrated();

    string strLyrics;

    // 媒体库（持久化）优先：能命中歌曲就用 lyricsFile。
    auto media = getMediaByAssociateKeyword(szAssociateFileKeyword);
    if (media) {
        if (!media->lyricsFile.empty()) {
            strLyrics = media->lyricsFile;
        }
    } else {
        // 不在媒体库里的歌曲（cue/shoutcast、或尚未入库），查会话内缓存。
        MutexAutolock autoLock(m_mutex);
        auto itLyric = m_mapLyricsAssociate.find(toAssociateKeyword(szAssociateFileKeyword));
        if (itLyric != m_mapLyricsAssociate.end()) {
            strLyrics = itLyric->second;
        }
    }

    if (strLyrics.empty()) {
        return false;
    }

    strcpy_safe(szLyricFile, nMaxBuff, strLyrics.c_str());

    if (isLyricsExist(szLyricFile)) {
        return true;
    }

    if (filePathIsBeginWithDriver(szAssociateFileKeyword)
        && filePathIsBeginWithDriver(szLyricFile)) {
        szLyricFile[0] = szAssociateFileKeyword[0];
        return isLyricsExist(szLyricFile);
    }

    return false;
}

bool CLyricsLocalSearch::isLyricsExist(cstr_t szLyricSource) {
    LRC_SOURCE_TYPE sourceType = lyrSrcTypeFromName(szLyricSource);

    if (sourceType == LST_FILE) {
        if (isFileExist(szLyricSource)) {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

// 根据关联 keyword 在媒体库中查找对应的歌曲（大小写不敏感）。
// 关联 keyword 对普通歌曲就是歌曲文件路径，与媒体库 url 大小写不敏感地匹配。
static MediaPtr getMediaByAssociateKeyword(cstr_t szAssociateFileKeyword) {
    if (isEmptyString(szAssociateFileKeyword)) {
        return nullptr;
    }

    auto mediaLib = g_player.getMediaLibrary();
    if (!mediaLib) {
        return nullptr;
    }

    return mediaLib->getMediaByUrlNocase(szAssociateFileKeyword);
}

void CLyricsLocalSearch::migrateLyricsAssociation() {
    // 歌词关联原本保存在单独的 MLyrics.S2L 文件里。
    // 阶段A：把旧文件读进内存，保证启动（媒体库尚未就绪）也能关联；旧文件暂不删除。
    // 阶段B：等媒体库就绪后，在 ensureLyricsAssociationMigrated() 里把关联迁入
    //        medialib.lyrics_file 并删除旧文件。以后关联只存在媒体库一份。
    string file = getAppDataDir() + "MLyrics.S2L";

    // 老格式：头部 + "len,songFileName,len,lyricFileName\n"
    {
        char szBuff[1024];
        char *szStr;
        int nLen, nLenBuff;
        FILE *fp = fopenUtf8(file.c_str(), "rb");
        if (fp == nullptr) {
            return;
        }

        if (!fgets(szBuff, CountOf(szBuff), fp)
            || strncmp(szBuff, SZ_S2L_HEADER, LEN_S2L_HEADER) != 0) {
            fclose(fp);
            return;
        }

        MutexAutolock lock(m_mutex);
        while (fgets(szBuff, CountOf(szBuff), fp)) {
            szStr = szBuff;
            nLenBuff = (int)strlen(szBuff);

            // song file name.
            szStr = readInt_t(szStr, nLen); if (nLen > nLenBuff - int(szStr - szBuff)) continue;
            if (*szStr == ',') szStr++; else continue;

            string mediaFn(szStr, nLen);
            szStr += nLen;
            if (*szStr != ',') {
                continue;
            }
            szStr++;

            // lyrics file name.
            szStr = readInt_t(szStr, nLen); if (nLen > nLenBuff - int(szStr - szBuff)) continue;
            if (*szStr == ',') szStr++; else continue;

            string lyricsFn(szStr, nLen);

            m_mapLyricsAssociate[mediaFn] = lyricsFn;
        }
    }

    // 若媒体库已就绪则立即迁移；否则等首次查询时自动触发。
    ensureLyricsAssociationMigrated();
}

void CLyricsLocalSearch::ensureLyricsAssociationMigrated() {
    // 只迁移一次。
    {
        MutexAutolock lock(m_mutex);
        if (m_bLyricsMigrated) {
            return;
        }
    }

    auto mediaLib = g_player.getMediaLibrary();
    if (!mediaLib) {
        // 媒体库还没就绪，等下一次查询再迁。
        return;
    }

    // 快照会话内关联，随后在无锁状态下访问媒体库，避免与
    // （持媒体库锁 → 调 g_LyricSearch）的路径成环死锁。
    vector<pair<string, string>> snapshot;
    {
        MutexAutolock lock(m_mutex);
        snapshot.assign(m_mapLyricsAssociate.begin(), m_mapLyricsAssociate.end());
    }

    vector<string> migratedKeys;
    for (auto &entry : snapshot) {
        auto media = mediaLib->getMediaByUrlNocase(entry.first.c_str());
        if (media) {
            if (media->lyricsFile != entry.second) {
                media->lyricsFile = entry.second;
                mediaLib->updateMediaInfo(media.get());
            }
            migratedKeys.push_back(entry.first);
        }
        // 不在媒体库的歌（如 shoutcast），保留在会话内关联里。
    }

    {
        MutexAutolock lock(m_mutex);
        for (auto &key : migratedKeys) {
            m_mapLyricsAssociate.erase(key);    // 已迁入媒体库，移出会话缓存
        }
        m_bLyricsMigrated = true;
        // 迁移完成后删除旧的关联文件。
        deleteFile((getAppDataDir() + "MLyrics.S2L").c_str());
    }
}

// Return the best match lyrics only
bool CLyricsLocalSearch::getBestMatchLyrics(cstr_t szSongFile, cstr_t szArtist, cstr_t szTitle, string &strLyrFile) {
    string fullTitle = formatMediaTitle(szArtist, szTitle);
    char szLyricsFile[MAX_PATH];
    if (getAssociatedLyrics(getAssociateFileKeyword(szSongFile, fullTitle.c_str()).c_str(), szLyricsFile, MAX_PATH)) {
        strLyrFile = szLyricsFile;
        return true;
    }

    ListLyrSearchResults vLyrics;
    CLyricsSearchParameter searchParam(szSongFile, szArtist, szTitle, true);
    searchLyrics(searchParam, vLyrics);
    strLyrFile = searchParam.strBestMatchLyrics;
    return strLyrFile.size() > 0;
}
