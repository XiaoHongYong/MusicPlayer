/********************************************************************
    Created  :    2002/01/04    21:41
    FileName :    LyricsLocalSearch.h
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#pragma once

#include "../LyricsLib/LyricsSearch.h"


//
// search local lyrics
//
class CLyricsLocalSearch {
public:
    void init();
    void quit();

    // search match lyrics
    void searchAllMatchLyrics(cstr_t szSongFile, cstr_t szArtist, cstr_t szTitle, ListLyrSearchResults &vLyrics);

    // search best match lyrics
    bool getBestMatchLyrics(cstr_t szSongFile, cstr_t szArtist, cstr_t szTitle, string &strLyrFile);

    // song://lyrics3v2
    // song://id3v2/sylt/xxx
    // song://id3v2/uslt/xxx
    static string getAssociateFileKeyword(cstr_t szSongFile, cstr_t szFulltitle);
    bool getAssociatedLyrics(cstr_t szAssociateFileKeyword, char * szLyricFile, int nMaxBuff);
    bool isAssociatedLyrics(cstr_t szAssociateFileKeyword);
    bool isAssociatedWithNoneLyrics(cstr_t szAssociateKeyword);
    bool associateLyrics(cstr_t szAssociateFileKeyword, cstr_t szLyricFile);
    bool cancelAssociate(cstr_t szAssociateFileKeyword);
    bool isLyricsExist(cstr_t szLyricSource);

    //
    // search folder
    //
    int getSearchFolerCount();
    bool removeFolder(int nIndex);
    string getFolder(int nIndex);
    bool setFolder(cstr_t szFolder);
    void saveLyricFolderCfg();

protected:
    void searchLyrics(CLyricsSearchParameter &searchParam, ListLyrSearchResults &vLyrics);

    // 把旧版单独的歌词关联文件（MLyrics.S2L）迁移进媒体库，并删除旧文件。
    void migrateLyricsAssociation();
    // 懒迁移：媒体库就绪后，把会话内关联逐条写进媒体库并删除旧文件。只执行一次。
    void ensureLyricsAssociationMigrated();

    string toAssociateKeyword(cstr_t szKeyword) {
#ifdef _WIN32
        // Use keyword without driver letter info "C:"
        if (szKeyword[0] && szKeyword[1] == ':') {
            return toLower(szKeyword + 2);
        }
#endif
        return toLower(szKeyword);
    }

    void loadLyricFolderCfg();

public:
    CLyricsLocalSearch();
    virtual ~CLyricsLocalSearch();

protected:
    VecStrings                  m_vLyricsFolders;

    //
    // Lyrics association:
    //
    //  关联持久化保存在媒体库的 lyricsFile 字段（medialib.lyrics_file），网页版通过
    //  /songs/{id}/lyrics 与 has_lyrics 读取同一份数据。
    //
    //  m_mapLyricsAssociate 仅作为运行时缓存，只用于那些不落在媒体库里的关联
    //  （如 cue/shoutcast 音轨、或尚未入库的歌曲），不再写回任何关联文件。
    //
    typedef map<string, string> SONG_LYRIC_MAP;

    SONG_LYRIC_MAP              m_mapLyricsAssociate;
    std::mutex                  m_mutex;

    bool                        m_bSaved;
    bool                        m_bLyricsMigrated = false;
};
