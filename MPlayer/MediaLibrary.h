#pragma once

#ifndef MPlayerEngine_MediaLibrary_h
#define MPlayerEngine_MediaLibrary_h

#include <unordered_map>
#include <functional>
#include "../third-parties/sqlite/Sqlite3.hpp"

#include "Playlist.h"


class CPlayer;

using MapMedias = std::unordered_map<int, MediaPtr>;

/**
 * 按照分类统计出媒体库的歌曲.
 *
 * 在搜索时，可以根据分类名来搜索歌曲.
 */
struct MediaCategory {
    enum Type {
        ALL,
        PLAYLIST,
        FOLDER,
        ARTIST,
        ALBUM,
        GENRE,
        RATING,
        _COUNT,
    };

    Type                        type;
    string                      name;
    string                      value;
    uint32_t                    mediaCount;     // 歌曲数量
    uint32_t                    mediaDuration;  // 总共时长，单位秒

    MediaCategory(Type type, cstr_t name, cstr_t value, uint32_t mediaCount = 0, uint32_t mediaDuration = 0) : type(type), name(name), value(value), mediaCount(mediaCount), mediaDuration(mediaDuration) {
    }

};

using VecMediaCategories = vector<MediaCategory>;

// 30 天播放历史上报的聚合结果（docs/web-console/data-model.md）。
struct PlayHistoryItem {
    int                         songId = 0;
    int                         count = 0;
    string                      lastPlayedAt;
};

struct PlayHistoryDay {
    string                      date;
    vector<PlayHistoryItem>     items;
};

using VecPlayHistoryDays = vector<PlayHistoryDay>;

cstr_t mediaCategoryTypeToString(MediaCategory::Type type);

enum MediaLibOrderBy {
    MLOB_NONE,
    MLOB_ARTIST,
    MLOB_ALBUM,
    MLOB_ARTIST_ALBUM,
    MLOB_GENRE,
    MLOB_RATING,

};

class CMediaLibrary {
public:
    CMediaLibrary();
    virtual ~CMediaLibrary();

    void getMediaCategories(VecMediaCategories &categoriesOut);
    PlaylistPtr getMediaCategory(const MediaCategory &category);

    VecStrings getAllArtist();
    VecStrings getAllAlbum();
    VecStrings getAllGenre();
    VecStrings getAlbumOfArtist(cstr_t szArtist);

    VecPlaylistBriefs getAllPlaylistBriefs();
    VecPlaylistBriefs getRecentPlaylistBriefs();

    uint32_t getMediaCount();

    MediaPtr getMediaByUrl(cstr_t szUrl);
    MediaPtr getMediaByID(int id);
    PlaylistPtr getMediaByIDs(const VecInts &ids);

    MediaPtr add(cstr_t szMediaUrl);

    // add media to media library fast, didn't update media info.
    MediaPtr addFast(cstr_t szMediaUrl);

    ResultCode updateMediaInfo(Media *media);

    // removes the specified item from the media library
    ResultCode remove(const MediaPtr &media, bool isDeleteFile = false);
    ResultCode remove(const VecMediaPtrs &medias, bool isDeleteFile = false);

    // If the media file was removed temporarily, set this flag on.
    ResultCode setDeleted(Media *media);

    PlaylistPtr getNowPlaying();
    void saveNowPlaying();

    PlaylistPtr newPlaylist(cstr_t name);
    PlaylistPtr getPlaylist(int playlistId);
    void deltePlaylist(int playlistId);

    ResultCode savePlaylist(const PlaylistPtr &playlist);
    ResultCode addToPlaylist(int idPlaylist, const PlaylistPtr &other);

    const PlaylistPtr &getAll();

    PlaylistPtr getAll(MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByArtist(cstr_t szArtist, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByAlbum(cstr_t szAlbum, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByAlbum(cstr_t szArtist, cstr_t szAlbum, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByTitle(cstr_t szTitle);

    PlaylistPtr getByGenre(cstr_t szGenre, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByYear(int nYear, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getByRating(int nRating, MediaLibOrderBy orderBy, int nTopN);

    PlaylistPtr getRecentPlayed(uint32_t nCount);

    PlaylistPtr getRecentPlayed(int nDayAgoBegin, int nDayAgoEnd);

    PlaylistPtr getTopPlayed(uint32_t nCount);

    PlaylistPtr getTopRating(uint32_t nCount);

    PlaylistPtr getRecentAdded(uint32_t nCount);

    // 0-5, 0 for unrate.
    ResultCode rate(Media *media, uint32_t nRating);

    ResultCode markPlayFinished(Media *media);

    // 写入 play_history，并同步累加 songs.countPlayed（桌面播完与 Web POST /history 共用）。
    ResultCode addPlayHistory(int songId, cstr_t playedAt);
    void cleanupPlayHistory(int keepDays = 30);
    VecPlayHistoryDays getRecentPlayHistory(int days);

public:
    int init();
    void close();

    bool isOK() { return m_nInitResult == ERR_OK; }

    VecStrings queryVStr(cstr_t szSql);

    ResultCode executeSQL(cstr_t szSql);

    ResultCode undeleteMedia(long nMediaID);

    ResultCode add(const MediaPtr &media);

    string getSettingValue(cstr_t name);
    void setSettingValue(cstr_t name, cstr_t value);

protected:
    ResultCode doAddMedia(const MediaPtr &media);

    void updateMediaInMem(Media *media);
    void removeMediaInMem(Media *media);
    void markPlayFinishedInMem(Media *media);
    ResultCode applyPlayRecord(Media *media, cstr_t playedAt);

    int upgradeCheck();
    bool tableExists(cstr_t table);
    bool columnExists(cstr_t table, cstr_t col);
    void addColumnIfMissing(cstr_t table, cstr_t col);

    void loadNowPlaying();
    void loadPlaylists();
    int savePlaylistInDb(const PlaylistInfo &playlist);
    PlaylistInfo *getMemPlaylistInfoById(int id);

    PlaylistPtr getPlaylist(PlaylistInfo &info);

    void updateMediaCategories();
    PlaylistPtr queryPlaylist(cstr_t SQL);
    PlaylistPtr queryPlaylist(cstr_t szSQL, cstr_t szClause);
    PlaylistPtr queryPlaylist(cstr_t szSQL, int clause);
    PlaylistPtr queryPlaylist(cstr_t SQL, std::function<int (CSqlite3Stmt &stmt)> callback);

protected:
    friend class CMLQueryPlaylist;

    PlaylistPtr                 m_allMedias;
    MapMedias                   m_mapMedias;

    PlaylistPtr                 m_nowPlaying;
    string                      m_fnNowPlaying;

    VecPlaylistInfo             m_playlists;
    VecMediaCategories          m_mediaCategories;
    uint64_t                    m_timeMediaCategoryUpdated;

    CSqlite3                    m_db;
    std::recursive_mutex        m_mutexDataAccess;

    CSqlite3Stmt                m_stmtUpdateMediaPlayTime, m_stmtUpdateMediaInfo;
    CSqlite3Stmt                m_sqlAdd, m_sqlAddFast, m_sqlQueryByUrl;
    int                         m_nInitResult;

};

#endif // !defined(MPlayerEngine_MediaLibrary_h)
