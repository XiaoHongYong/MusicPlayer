#include "../Skin/Skin.h"
#include "MediaLibrary.h"
#include "Player.h"
#include "MediaScanner.h"
#include "Utils/rapidjson.h"
#include <algorithm>
#include <cstring>


#define ALBUM_ARTIST_SEP        "~@~"
#define ALBUM_ARTIST_SEP_LEN    3

#define MEDIA_LIB_VER_NAME  "version"
#define MEDIA_LIB_VER_CUR   "2"


#define SQL_CREATE_SETTINGS        "CREATE TABLE IF NOT EXISTS settings\
(\
    name text Primary Key,\
    value text\
);"

#define SQL_SETTINGS_INSERT        "INSERT INTO settings VALUES (?, ?) "
#define SQL_SETTINGS_QUERY         "SELECT value FROM settings WHERE name=?"
#define SQL_SETTINGS_SET           "INSERT OR REPLACE INTO settings(name, value) VALUES(?, ?);"

#define SQL_CREATE_MEDIALIB        "CREATE TABLE IF NOT EXISTS medialib    \
(\
  id integer Primary Key,\
  url text,\
  file_deleted integer DEFAULT 0,\
  artist text COLLATE NOCASE,\
  album text DEFAULT NULL COLLATE NOCASE,\
  title text COLLATE NOCASE,\
  track integer DEFAULT -1,\
  year integer DEFAULT 0,\
  genre text DEFAULT NULL COLLATE NOCASE,\
  comment text DEFAULT NULL COLLATE NOCASE,\
  duration integer DEFAULT 0,\
  filesize integer DEFAULT 0,\
  time_added integer,\
  time_played integer DEFAULT 0,\
  rating integer DEFAULT 0,\
  count_played integer DEFAULT 0,\
  music_hash text DEFAULT NULL,\
  lyrics_file text DEFAULT NULL,\
  format text DEFAULT NULL,\
  bitRate integer DEFAULT 0,\
  channels integer DEFAULT 0,\
  bitsPerSample integer DEFAULT 0,\
  sampleRate integer DEFAULT 0,\
  version text DEFAULT NULL,\
  disc_number integer DEFAULT -1,\
  metadata_status integer DEFAULT 0\
);\
\
CREATE UNIQUE INDEX IF NOT EXISTS mli_url on medialib (url);\
CREATE INDEX IF NOT EXISTS mli_artist on medialib (artist);\
CREATE INDEX IF NOT EXISTS mli_title on medialib (title);\
CREATE INDEX IF NOT EXISTS mli_genre on medialib (genre);\
CREATE INDEX IF NOT EXISTS mli_time_added on medialib (time_added);\
CREATE INDEX IF NOT EXISTS mli_rating on medialib (rating);\
CREATE INDEX IF NOT EXISTS mli_music_hash on medialib (music_hash);"

#define DROP_MEDIALIB_TABLE "DROP TABLE IF EXISTS medialib;\
DROP INDEX IF EXISTS mli_url;\
DROP INDEX IF EXISTS mli_artist;\
DROP INDEX IF EXISTS mli_title;\
DROP INDEX IF EXISTS mli_genre;\
DROP INDEX IF EXISTS mli_time_added;\
DROP INDEX IF EXISTS mli_rating;\
DROP INDEX IF EXISTS mli_music_hash;"

#define SQL_MAX_MEDIALIB_ID   "select MAX(id)  from medialib"

#define SQL_COUNT_OF_MEDIA    "select count(*)  from medialib"

#define SQL_ADD_MEDIA    "INSERT INTO medialib (url, artist, album, title, track, year, genre, comment, duration, filesize, time_added, music_hash, lyrics_file, version, disc_number, metadata_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"

#define SQL_ADD_MEDIA_FAST    "INSERT INTO medialib (url, artist, title, filesize, time_added, format) VALUES (?, ?, ?, ?, ?, ?);"

#define SQL_QUERY_MEDIA_BY_URL  "select * from medialib where url=? "

#define SQL_UPDATE_MEDIA_PLAY_TIME "update medialib set count_played=count_played+1, time_played=? where id=?"

#define SQL_MEDIA_UPDATE_INFO "update medialib set url=?, artist=?, album=?, title=?, track=?,"\
    "year=?, genre=?, comment=?, duration=?, filesize=?, lyrics_file=?, music_hash=?,"\
    "format=?, bitRate=?, channels=?, bitsPerSample=?, sampleRate=?, version=?, disc_number=?, metadata_status=? where id=?"

#define SQL_CREATE_TABLE_PLAYLIST   "CREATE TABLE IF NOT EXISTS playlists \
(\
  id integer Primary Key,\
  name text,\
  duration integer DEFAULT 0,\
  count integer DEFAULT 0,\
  rating integer DEFAULT 300,\
  is_up_to_date integer DEFAULT 0,\
  time_modified integer DEFAULT 0,\
  media_ids text DEFAULT NULL\
);"

#define SQL_QUERY_ALL_PLAYLISTS     "SELECT * FROM playlists"
#define SQL_ADD_PLAYLIST            "INSERT INTO playlists (name, duration, count, rating, is_up_to_date, time_modified, media_ids) VALUES (?, ?, ?, ?, ?, ?, ?);"
#define SQL_UPDATE_PLAYLIST_BY_ID   "UPDATE playlists SET name=?, duration=?, count=?, rating=?, is_up_to_date=?, time_modified=?, media_ids=? WHERE id=?"
#define SQL_DELETE_PLAYLIST_BY_ID   "DELETE FROM playlists WHERE id=?"

#define SQL_CREATE_TABLE_PLAY_HISTORY "CREATE TABLE IF NOT EXISTS play_history \
(\
  id integer Primary Key,\
  song_id integer NOT NULL,\
  played_at text NOT NULL\
);"

#define SQL_CREATE_IDX_PLAY_HISTORY_AT "CREATE INDEX IF NOT EXISTS idx_play_history_played_at ON play_history(played_at DESC);"
#define SQL_CREATE_IDX_PLAY_HISTORY_SONG "CREATE INDEX IF NOT EXISTS idx_play_history_song_time ON play_history(song_id, played_at DESC);"
#define SQL_ADD_PLAY_HISTORY "INSERT INTO play_history (song_id, played_at) VALUES (?, ?);"
#define SQL_LAST_PLAY_HISTORY "SELECT played_at FROM play_history WHERE song_id=? ORDER BY played_at DESC LIMIT 1;"
#define SQL_QUERY_PLAY_HISTORY_SINCE "SELECT song_id, played_at FROM play_history WHERE played_at >= ? ORDER BY played_at DESC;"
#define SQL_CLEANUP_PLAY_HISTORY "DELETE FROM play_history WHERE played_at < ?;"

#define SQLITE3_BIND_TEXT(sqlstmt, strData)            \
    ret = sqlstmt.bindText(n, strData.c_str(), (int)strData.size());\
    if (ret != ERR_OK)\
        goto RET_FAILED;\
    n++;

#define SQLITE3_BIND_CSTR(sqlstmt, strData)            \
    ret = sqlstmt.bindText(n, strData, strlen(strData));\
    if (ret != ERR_OK)\
        goto RET_FAILED;\
    n++;

#define SQLITE3_BIND_INT(sqlstmt, data)            \
    ret = sqlstmt.bindInt(n, data);\
    if (ret != ERR_OK)\
        goto RET_FAILED;\
    n++;

#define SQLITE3_BIND_INT64(sqlstmt, data)            \
    ret = sqlstmt.bindInt64(n, data);\
    if (ret != ERR_OK)\
        goto RET_FAILED;\
    n++;

#define  MediaGetSqlite3ColumText(mediaAttri)    \
    text = (const char *)sqlStmt.columnText(n++);    \
    if (text)\
        mediaAttri = text;\
    else\
        mediaAttri.resize(0);

MediaPtr newMedia() {
    return make_shared<Media>();
}

cstr_t mediaCategoryTypeToString(MediaCategory::Type type) {
    static cstr_t NAMES[] = { _TLM("All Music"), _TLM("Playlist"), _TLM("Folder"), _TLM("Artist"), _TLM("Album"), _TLM("Genre"), _TLM("Rating") };
    assert(CountOf(NAMES) == MediaCategory::_COUNT);
    assert(type >= 0 && type <= MediaCategory::_COUNT);

    return NAMES[type];
}

MediaPtr sqliteQueryMedia(CSqlite3Stmt &sqlStmt) {
    int n = 0;
    const char *text;
    string str;

    auto media = make_shared<Media>();

    media->ID = sqlStmt.columnInt(n++);

    MediaGetSqlite3ColumText(media->url);
    media->isFileDeleted = tobool(sqlStmt.columnInt(n++));
    MediaGetSqlite3ColumText(media->artist);
    MediaGetSqlite3ColumText(media->album);
    MediaGetSqlite3ColumText(media->title);
    media->trackNumb = sqlStmt.columnInt(n++);
    media->year = sqlStmt.columnInt(n++);
    MediaGetSqlite3ColumText(media->genre);
    MediaGetSqlite3ColumText(media->comments);
    media->duration = sqlStmt.columnInt(n++);
    media->fileSize = sqlStmt.columnInt64(n++);
    media->timeAdded = sqlStmt.columnInt64(n++);
    media->timePlayed = sqlStmt.columnInt64(n++);
    media->rating = sqlStmt.columnInt(n++);
    media->countPlayed = sqlStmt.columnInt(n++);
    MediaGetSqlite3ColumText(media->musicHash);
    MediaGetSqlite3ColumText(media->lyricsFile);
    MediaGetSqlite3ColumText(media->format);
    media->bitRate = sqlStmt.columnInt(n++);
    media->channels = sqlStmt.columnInt(n++);
    media->bitsPerSample = sqlStmt.columnInt(n++);
    media->sampleRate = sqlStmt.columnInt(n++);
    MediaGetSqlite3ColumText(media->version);
    media->discNumb = (int16_t)sqlStmt.columnInt(n++);
    media->metaStatus = (MetadataStatus)sqlStmt.columnInt(n++);

    return media;
}

PlaylistPtr sqliteQueryPlaylist(CMediaLibrary *library, CSqlite3Stmt &stmt) {
    auto playlist = std::make_shared<Playlist>();

    int ret = stmt.step();
    while (ret == ERR_SL_OK_ROW) {
        auto media = sqliteQueryMedia(stmt);
        playlist->insertItem(-1, media);

        ret = stmt.step();
    }

    stmt.reset();
    return playlist;
}

void appendQeuryStatment(MediaLibOrderBy orderBy, int nTopN, string &strSql) {
    if (orderBy == MLOB_NONE) {
    } else if (orderBy == MLOB_ARTIST) {
        strSql += " order by artist";
    } else if (orderBy == MLOB_ALBUM) {
        strSql += " order by album";
    } else if (orderBy == MLOB_GENRE) {
        strSql += " order by genre";
    } else if (orderBy == MLOB_ARTIST_ALBUM) {
        strSql += " order by artist, album";
    } else if (orderBy == MLOB_RATING) {
        strSql += " order by rating desc";
    }

    if (nTopN != -1) {
        strSql += " limit";
        strSql += itos(nTopN);
    }
}

CMediaLibrary::CMediaLibrary() {
}

CMediaLibrary::~CMediaLibrary() {
    close();
}

void CMediaLibrary::getMediaCategories(VecMediaCategories &categoriesOut) {
    updateMediaCategories();

    categoriesOut = m_mediaCategories;
}

PlaylistPtr CMediaLibrary::getMediaCategory(const MediaCategory &category) {
    if (category.type == MediaCategory::ALL) {
        return m_allMedias;
    } else if (category.type == MediaCategory::PLAYLIST) {
        return getPlaylist(atoi(category.value.c_str()));
    }

    int count = m_allMedias->getCount();
    auto pl = std::make_shared<Playlist>();

    switch (category.type) {
        case MediaCategory::FOLDER: {
            for (int i = 0; i < count; i++) {
                auto media = m_allMedias->getItem(i);
                auto &url = media->url;
                if (startsWith(url.c_str(), category.value.c_str())) {
                    pl->insertItem(-1, media);
                }
            }
            break;
        }
        case MediaCategory::ARTIST: {
            for (int i = 0; i < count; i++) {
                auto media = m_allMedias->getItem(i);
                if (category.value == media->artist.c_str()) {
                    pl->insertItem(-1, media);
                }
            }
            break;
        }
        case MediaCategory::ALBUM: {
            for (int i = 0; i < count; i++) {
                auto media = m_allMedias->getItem(i);
                auto str = media->artist;
                if (startsWith(category.value.c_str(), str.c_str())) {
                    StringView key(category.value.c_str(), category.value.size());
                    key.shrink((int)str.size());
                    if (key.startsWith(ALBUM_ARTIST_SEP)) {
                        key.shrink(ALBUM_ARTIST_SEP_LEN);
                        if (key.equal(media->album.c_str())) {
                            pl->insertItem(-1, media);
                        }
                    }
                }
            }
            break;
        }
        case MediaCategory::GENRE: {
            for (int i = 0; i < count; i++) {
                auto media = m_allMedias->getItem(i);
                if (category.value == media->genre) {
                    pl->insertItem(-1, media);
                }
            }
            break;
        }
        case MediaCategory::RATING: {
            int rating = atoi(category.value.c_str());
            for (int i = 0; i < count; i++) {
                auto media = m_allMedias->getItem(i);
                if (media->rating >= rating) {
                    pl->insertItem(-1, media);
                }
            }
            break;
        }
        default:
            assert(0);
            break;
    }

    return pl;
}

VecStrings CMediaLibrary::getAllArtist() {
    return queryVStr("select artist from medialib group by artist");
}

VecStrings CMediaLibrary::getAllAlbum() {
    return queryVStr("select album from medialib group by album");
}

VecStrings CMediaLibrary::getAllGenre() {
    return queryVStr("select genre from medialib group by genre");
}

VecStrings CMediaLibrary::getAlbumOfArtist(cstr_t szArtist) {
    VecStrings results;

    if (!isOK()) {
        return results;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    CSqlite3Stmt sqlQuery;

    int ret = sqlQuery.prepare(&m_db, "select album from medialib where artist=? group by album");
    if (ret != ERR_OK) {
        return results;
    }

    sqlQuery.bindStaticText(1, szArtist);

    ret = sqlQuery.step();
    while (ret == ERR_SL_OK_ROW) {
        auto text = sqlQuery.columnText(0);
        if (text) {
            results.push_back(text);
        }

        ret = sqlQuery.step();
    }

    return results;
}

VecPlaylistBriefs CMediaLibrary::getAllPlaylistBriefs() {
    VecPlaylistBriefs names;

    for (auto &item : m_playlists) {
        names.push_back(item);
    }

    return names;
}

VecPlaylistBriefs CMediaLibrary::getRecentPlaylistBriefs() {
    VecPlaylistBriefs names;

    for (auto &item : m_playlists) {
        names.push_back(item);
    }

    std::sort(names.begin(), names.begin(), [](const PlaylistBrief &a, const PlaylistBrief &b) {
        return a.timeModified > b.timeModified;
    });

    return names;
}

uint32_t CMediaLibrary::getMediaCount() {
    return sqlite_query_int_value(m_db.m_db, SQL_COUNT_OF_MEDIA);
}

MediaPtr CMediaLibrary::getMediaByUrl(cstr_t szUrl) {
    if (!isOK()) {
        return nullptr;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    m_sqlQueryByUrl.bindStaticText(1, szUrl);

    MediaPtr media;
    if (m_sqlQueryByUrl.step() == ERR_SL_OK_ROW) {
        media = sqliteQueryMedia(m_sqlQueryByUrl);

        if (media->isFileDeleted) {
            // recover to undeleted.
            if (undeleteMedia(media->ID) == ERR_OK) {
                media->isFileDeleted = false;
            }
        }
    }

    m_sqlQueryByUrl.reset();

    return media;
}

MediaPtr CMediaLibrary::getMediaByUrlNocase(cstr_t szUrl) {
    if (!isOK() || isEmptyString(szUrl)) {
        return nullptr;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    auto count = m_allMedias->getCount();
    for (uint32_t i = 0; i < count; i++) {
        auto media = m_allMedias->getItem(i);
        if (media && strcasecmp(media->url.c_str(), szUrl) == 0) {
            return media;
        }
    }

    return nullptr;
}

MediaPtr CMediaLibrary::getMediaByID(int id) {
    RMutexAutolock autolock(m_mutexDataAccess);

    auto it = m_mapMedias.find(id);
    if (it != m_mapMedias.end()) {
        auto media = (*it).second;
        if (!media->isFileDeleted) {
            return media;
        }
    }

    return nullptr;
}

PlaylistPtr CMediaLibrary::getMediaByIDs(const VecInts &ids) {
    if (!isOK()) {
        return nullptr;
    }

    auto playlist = std::make_shared<Playlist>();
    RMutexAutolock autolock(m_mutexDataAccess);

    for (auto id : ids) {
        auto it = m_mapMedias.find(id);
        if (it != m_mapMedias.end()) {
            auto media = (*it).second;
            if (!media->isFileDeleted) {
                playlist->insertItem(-1, media);
            }
        }
    }

    return playlist;
}

MediaPtr CMediaLibrary::add(cstr_t mediaUrl) {
    if (!isOK()) {
        return nullptr;
    }

    auto media = getMediaByUrl(mediaUrl);
    if (media) {
        return media;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    media = newMedia();

    media->url = mediaUrl;

    g_player.loadMediaTagInfo(media.get());

    doAddMedia(media);

    return media;
}

// add media to media library fast, won't update media info.
MediaPtr CMediaLibrary::addFast(cstr_t mediaUrl) {
    if (!isOK()) {
        return nullptr;
    }

    auto media = getMediaByUrl(mediaUrl);
    if (media) {
        return media;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    media = newMedia();

    media->url = mediaUrl;
    getArtistTitleFromFileName(media->artist, media->title, mediaUrl);
    FileStatInfo info;
    if (getFileStatInfo(media->url.c_str(), info)) {
        media->timeAdded = info.createdTime;
        media->fileSize = info.fileSize;
    }
    media->format = getMediaFormat(mediaUrl);

    int n = 1, ret = ERR_OK;
    // url, artist, title, filesize, time_added, format
    SQLITE3_BIND_CSTR(m_sqlAddFast, mediaUrl);
    SQLITE3_BIND_TEXT(m_sqlAddFast, media->artist);
    SQLITE3_BIND_TEXT(m_sqlAddFast, media->title);
    SQLITE3_BIND_INT64(m_sqlAddFast, media->fileSize);
    SQLITE3_BIND_INT64(m_sqlAddFast, media->timeAdded);
    SQLITE3_BIND_TEXT(m_sqlAddFast, media->format);

    ret = m_sqlAddFast.step();
    if (ret != ERR_OK) {
        goto RET_FAILED;
    }

    media->ID = m_db.LastInsertRowId();

    m_sqlAddFast.reset();

    m_allMedias->insertItem(-1, media);
    m_mapMedias[media->ID] = media;

    g_mediaScanner.scanMedia(media);

    return media;

RET_FAILED:
    m_sqlAddFast.reset();

    return nullptr;
}

// update Media Info to DB
ResultCode CMediaLibrary::updateMediaInfo(Media *media) {
    if (!isOK()) {
        return m_nInitResult;
    }

    assert(media->ID != MEDIA_ID_INVALID);
    if (media->ID == MEDIA_ID_INVALID) {
        return ERR_NOT_FOUND;
    }

    RMutexAutolock autolock(m_mutexDataAccess);
    CSqlite3Stmt sqlQuery;
    int ret, n = 1;

    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->url);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->artist);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->album);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->title);

    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->trackNumb);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->year);

    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->genre);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->comments);

    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->duration);
    SQLITE3_BIND_INT64(m_stmtUpdateMediaInfo, media->fileSize);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->lyricsFile);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->musicHash);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->format);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->bitRate);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->channels);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->bitsPerSample);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->sampleRate);
    SQLITE3_BIND_TEXT(m_stmtUpdateMediaInfo, media->version);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->discNumb);
    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, (int)media->metaStatus);

    SQLITE3_BIND_INT(m_stmtUpdateMediaInfo, media->ID);
    ret = m_stmtUpdateMediaInfo.step();
    m_stmtUpdateMediaInfo.reset();

    updateMediaInMem(media);

RET_FAILED:
    return ret;
}

// removes the specified item from the media library
ResultCode CMediaLibrary::remove(const MediaPtr &media, bool isDeleteFile) {
    if (!isOK()) {
        return m_nInitResult;
    }

    return remove({media}, isDeleteFile);
}

bool removeIDs(VecInts &ids, const VecInts &toRemove) {
    bool removed = false;
    for (auto id : toRemove) {
        for (auto it = ids.begin(); it != ids.end(); ++it) {
            if ((*it) == id) {
                ids.erase(it);
                removed = true;
                break;
            }
        }
    }

    return removed;
}

ResultCode CMediaLibrary::remove(const VecMediaPtrs &medias, bool isDeleteFile) {
    if (!isOK()) {
        return m_nInitResult;
    }

    // Remove from memory
    m_allMedias->removeItems(medias);
    for (auto &media : medias) {
        auto it = m_mapMedias.find(media->ID);
        if (it != m_mapMedias.end()) {
            m_mapMedias.erase(it);
        }
    }

    //
    // Remove from database
    //
    RMutexAutolock autolock(m_mutexDataAccess);
    CSqlite3Stmt sqlQuery;
    sqlQuery.prepare(&m_db, "delete from medialib where id=?");

    VecInts idsToRemove;
    for (auto &media : medias) {
        sqlQuery.bindInt(1, media->ID);
        int ret = sqlQuery.step();
        if (ret == ERR_OK) {
            if (isDeleteFile) {
                deleteFile(media->url.c_str());
            }
        }
        sqlQuery.reset();
        idsToRemove.push_back(media->ID);
    }

    //
    // Remove from playlists
    //
    for (auto &info : m_playlists) {
        if (info.playlist) {
            if (!info.playlist->removeItems(medias)) {
                continue;
            }
        } else {
            if (!removeIDs(info.mediaIds, idsToRemove)) {
                continue;
            }

            auto playlist = getPlaylist(info);
            playlist->refreshTimeModified();
        }
        info = info.playlist->toPlaylistInfo();
        savePlaylistInDb(info);
    }

    //
    // Remove from NowPlaying
    //
    m_nowPlaying->removeItems(medias);
    saveNowPlaying();

    return ERR_OK;
}

// If the media file was removed temporarily, set this flag on.
ResultCode CMediaLibrary::setDeleted(Media *media) {
    char szSql[256];

    snprintf(szSql, CountOf(szSql), "update medialib set file_deleted=1 where id=%d", media->ID);

    removeMediaInMem(media);

    RMutexAutolock autolock(m_mutexDataAccess);
    return executeSQL(szSql);
}

ResultCode CMediaLibrary::savePlaylist(const PlaylistPtr &playlist) {
    auto info = playlist->toPlaylistInfo();
    playlist->refreshTimeModified();

    RMutexAutolock autolock(m_mutexDataAccess);
    auto existing = getMemPlaylistInfoById(playlist->id);
    if (existing) {
        *existing = info;
    } else {
        m_playlists.push_back(info);
    }

    savePlaylistInDb(info);

    return ERR_OK;
}

ResultCode CMediaLibrary::addToPlaylist(int idPlaylist, const PlaylistPtr &other) {
    RMutexAutolock autolock(m_mutexDataAccess);
    auto existing = getMemPlaylistInfoById(idPlaylist);
    if (!existing) {
        return ERR_NOT_FOUND;
    }

    if (!getPlaylist(*existing)->append(other.get())) {
        // Not modified.
        return ERR_OK;
    }

    *existing = existing->playlist->toPlaylistInfo();

    savePlaylistInDb(*existing);

    return ERR_OK;
}

const PlaylistPtr &CMediaLibrary::getAll() {
    return m_allMedias;
}

PlaylistPtr CMediaLibrary::getAll(MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str());
}

PlaylistPtr CMediaLibrary::getByArtist(cstr_t szArtist, MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where artist=? and file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), szArtist);
}

PlaylistPtr CMediaLibrary::getByAlbum(cstr_t szAlbum, MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where album=? and file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), szAlbum);
}

PlaylistPtr CMediaLibrary::getByAlbum(cstr_t szArtist, cstr_t szAlbum, MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where artist=? and album=? and file_deleted!=1";

    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), [szArtist, szAlbum](CSqlite3Stmt &stmt) {
        int ret, n = 1;
        SQLITE3_BIND_CSTR(stmt, szArtist);
        SQLITE3_BIND_CSTR(stmt, szAlbum);
    RET_FAILED:
        assert(0);
        return ret;
    });
}

PlaylistPtr CMediaLibrary::getByTitle(cstr_t szTitle) {
    return queryPlaylist("select * from medialib where title=? and file_deleted!=1", szTitle);
}

PlaylistPtr CMediaLibrary::getByGenre(cstr_t szGenre, MediaLibOrderBy orderBy, int nTopN) {
    string strSql;

    strSql = "select * from medialib where genre=? and file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), szGenre);
}

PlaylistPtr CMediaLibrary::getByYear(int nYear, MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where year=? and file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), nYear);
}

PlaylistPtr CMediaLibrary::getByRating(int nRating, MediaLibOrderBy orderBy, int nTopN) {
    string strSql = "select * from medialib where rating=? and file_deleted!=1";
    appendQeuryStatment(orderBy, nTopN, strSql);

    return queryPlaylist(strSql.c_str(), nRating);
}

PlaylistPtr CMediaLibrary::getRecentPlayed(uint32_t nCount) {
    char szQuery[256];

    snprintf(szQuery, CountOf(szQuery), "select * from medialib where file_deleted!=1 order by time_played desc limit %d", nCount);

    return queryPlaylist(szQuery);
}

PlaylistPtr CMediaLibrary::getTopPlayed(uint32_t nCount) {
    char szQuery[256];
    snprintf(szQuery, CountOf(szQuery), "select * from medialib where file_deleted!=1 order by count_played desc limit %d", nCount);

    return queryPlaylist(szQuery);
}

PlaylistPtr CMediaLibrary::getTopRating(uint32_t nCount) {
    char szQuery[256];
    snprintf(szQuery, CountOf(szQuery), "select * from medialib where file_deleted!=1 order by rating desc limit %d", nCount);

    return queryPlaylist(szQuery);
}

PlaylistPtr CMediaLibrary::getRecentAdded(uint32_t nCount) {
    char szQuery[256];
    snprintf(szQuery, CountOf(szQuery), "select * from medialib where file_deleted!=1 order by time_added desc limit %d", nCount);

    return queryPlaylist(szQuery);
}

PlaylistPtr CMediaLibrary::getRecentPlayed(int nDayAgoBegin, int nDayAgoEnd) {
    char szQuery[256];

    auto begin = time(nullptr) - DateTime::SECOND_IN_ONE_DAY * nDayAgoBegin;
    auto end = time(nullptr) - DateTime::SECOND_IN_ONE_DAY * nDayAgoEnd;

    snprintf(szQuery, CountOf(szQuery), "select * from medialib where time_played between %ld and %ld and file_deleted!=1 order by time_played", begin, end);

    return queryPlaylist(szQuery);
}

// 0-5, 0 for unrate.
ResultCode CMediaLibrary::rate(Media *media, uint32_t nRating) {
    if (!isOK()) {
        return m_nInitResult;
    }

    if (!media) {
        return ERR_OK;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    char szSql[256];
    snprintf(szSql, CountOf(szSql), "update medialib set rating=%d where id=%d",
        nRating, (int)media->ID);

    media->rating = nRating;

    return executeSQL(szSql);
}

static string isoUtcFromTime(time_t t) {
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    return buf;
}

static time_t timeFromIsoUtc(const string &iso) {
    if (iso.size() < 19) {
        return 0;
    }
    struct tm t = {};
    t.tm_year = atoi(iso.c_str()) - 1900;
    t.tm_mon = atoi(iso.c_str() + 5) - 1;
    t.tm_mday = atoi(iso.c_str() + 8);
    t.tm_hour = atoi(iso.c_str() + 11);
    t.tm_min = atoi(iso.c_str() + 14);
    t.tm_sec = atoi(iso.c_str() + 17);
#ifdef _WIN32
    return _mkgmtime(&t);
#else
    return timegm(&t);
#endif
}

ResultCode CMediaLibrary::addPlayHistory(int songId, cstr_t playedAt) {
    auto media = getMediaByID(songId);
    if (!media) {
        return ERR_NOT_FOUND;
    }
    return applyPlayRecord(media.get(), playedAt);
}

void CMediaLibrary::cleanupPlayHistory(int keepDays) {
    if (!isOK() || keepDays <= 0) {
        return;
    }
    time_t cutoff = time(nullptr) - (time_t)keepDays * 86400;
    string iso = isoUtcFromTime(cutoff);
    CSqlite3Stmt stmt;
    if (stmt.prepare(&m_db, SQL_CLEANUP_PLAY_HISTORY) != ERR_OK) {
        return;
    }
    if (stmt.bindText(1, iso.c_str(), iso.size()) != ERR_OK) {
        return;
    }
    stmt.step();
}

VecPlayHistoryDays CMediaLibrary::getRecentPlayHistory(int days) {
    VecPlayHistoryDays out;
    if (!isOK()) {
        return out;
    }
    if (days <= 0) {
        days = 30;
    }
    if (days > 90) {
        days = 90;
    }

    time_t cutoff = time(nullptr) - (time_t)days * 86400;
    string iso = isoUtcFromTime(cutoff);

    RMutexAutolock autolock(m_mutexDataAccess);
    CSqlite3Stmt stmt;
    if (stmt.prepare(&m_db, SQL_QUERY_PLAY_HISTORY_SINCE) != ERR_OK) {
        return out;
    }
    if (stmt.bindText(1, iso.c_str(), iso.size()) != ERR_OK) {
        return out;
    }

    // date -> songId -> item（按 played_at DESC 扫描，首次见到的即 last_played_at）
    unordered_map<string, unordered_map<int, PlayHistoryItem>> grouped;
    vector<string> dateOrder;
    auto ret = stmt.step();
    while (ret == ERR_SL_OK_ROW) {
        int songId = stmt.columnInt(0);
        string playedAt = stmt.columnText(1);
        string date = playedAt.size() >= 10 ? playedAt.substr(0, 10) : playedAt;
        auto git = grouped.find(date);
        if (git == grouped.end()) {
            dateOrder.push_back(date);
            grouped[date][songId] = PlayHistoryItem{ songId, 1, playedAt };
        } else {
            auto &songs = git->second;
            auto sit = songs.find(songId);
            if (sit == songs.end()) {
                songs[songId] = PlayHistoryItem{ songId, 1, playedAt };
            } else {
                sit->second.count++;
            }
        }
        ret = stmt.step();
    }

    for (auto &date : dateOrder) {
        PlayHistoryDay day;
        day.date = date;
        auto &songs = grouped[date];
        vector<PlayHistoryItem> items;
        items.reserve(songs.size());
        for (auto &kv : songs) {
            items.push_back(kv.second);
        }
        std::sort(items.begin(), items.end(), [](const PlayHistoryItem &a, const PlayHistoryItem &b) {
            return a.lastPlayedAt > b.lastPlayedAt;
        });
        day.items = std::move(items);
        out.push_back(std::move(day));
    }
    return out;
}

ResultCode CMediaLibrary::applyPlayRecord(Media *media, cstr_t playedAt) {
    if (!isOK()) {
        return m_nInitResult;
    }
    if (!media || media->ID == MEDIA_ID_INVALID) {
        return ERR_OK;
    }

    string at = playedAt && playedAt[0] ? playedAt : isoUtcFromTime(time(nullptr));

    RMutexAutolock autolock(m_mutexDataAccess);

    // 同一首歌 30 秒内不重复记账（桌面 tick / 网页 POST / 播完 可能连发）。
    {
        CSqlite3Stmt lastStmt;
        if (lastStmt.prepare(&m_db, SQL_LAST_PLAY_HISTORY) == ERR_OK &&
            lastStmt.bindInt(1, media->ID) == ERR_OK &&
            lastStmt.step() == ERR_SL_OK_ROW) {
            string lastIso = lastStmt.columnText(0);
            time_t lastAt = timeFromIsoUtc(lastIso);
            if (lastAt > 0 && time(nullptr) - lastAt < 30) {
                return ERR_OK;
            }
        }
    }

    media->countPlayed++;
    media->timePlayed = time(nullptr);

    m_stmtUpdateMediaPlayTime.bindInt64(1, media->timePlayed);
    m_stmtUpdateMediaPlayTime.bindInt(2, media->ID);

    auto ret = m_stmtUpdateMediaPlayTime.step();
    assert(ret == ERR_OK);
    m_stmtUpdateMediaPlayTime.reset();

    markPlayFinishedInMem(media);

    CSqlite3Stmt stmt;
    ret = stmt.prepare(&m_db, SQL_ADD_PLAY_HISTORY);
    if (ret != ERR_OK) {
        return ret;
    }
    ret = stmt.bindInt(1, media->ID);
    if (ret != ERR_OK) {
        return ret;
    }
    ret = stmt.bindText(2, at.c_str(), at.size());
    if (ret != ERR_OK) {
        return ret;
    }
    ret = stmt.step();
    if (ret != ERR_OK) {
        return ret;
    }

    cleanupPlayHistory(30);
    return ERR_OK;
}

ResultCode CMediaLibrary::markPlayFinished(Media *media) {
    return applyPlayRecord(media, nullptr);
}

int CMediaLibrary::init() {
    string strFile = getAppDataDir() + "medialib.db";

    m_nInitResult = m_db.open(strFile.c_str());
    if (m_nInitResult != ERR_OK) {
        goto INIT_FAILED;
    }

    // 检查 MediaLibrary 数据的版本，并且进行升级
    m_nInitResult = upgradeCheck();
    if (m_nInitResult != ERR_OK) {
        goto INIT_FAILED;
    }

    // create medialib talbe
    m_nInitResult = m_db.exec(SQL_CREATE_MEDIALIB);
    if (m_nInitResult != ERR_OK) {
        return m_nInitResult;
    }

    // 创建 playlists 的 table
    m_nInitResult = m_db.exec(SQL_CREATE_TABLE_PLAYLIST);
    if (m_nInitResult != ERR_OK) {
        return m_nInitResult;
    }

    // 播放历史：CREATE IF NOT EXISTS，不走 destructive 版本升级。
    m_nInitResult = m_db.exec(SQL_CREATE_TABLE_PLAY_HISTORY);
    if (m_nInitResult != ERR_OK) {
        return m_nInitResult;
    }
    m_nInitResult = m_db.exec(SQL_CREATE_IDX_PLAY_HISTORY_AT);
    if (m_nInitResult != ERR_OK) {
        return m_nInitResult;
    }
    m_nInitResult = m_db.exec(SQL_CREATE_IDX_PLAY_HISTORY_SONG);
    if (m_nInitResult != ERR_OK) {
        return m_nInitResult;
    }
    cleanupPlayHistory(30);

    m_nInitResult = m_stmtUpdateMediaPlayTime.prepare(&m_db, SQL_UPDATE_MEDIA_PLAY_TIME);
    assert(m_nInitResult == ERR_OK);

    m_nInitResult = m_stmtUpdateMediaInfo.prepare(&m_db, SQL_MEDIA_UPDATE_INFO);
    assert(m_nInitResult == ERR_OK);

    m_nInitResult = m_sqlAdd.prepare(&m_db, SQL_ADD_MEDIA);
    assert(m_nInitResult == ERR_OK);

    m_nInitResult = m_sqlAddFast.prepare(&m_db, SQL_ADD_MEDIA_FAST);
    assert(m_nInitResult == ERR_OK);

    m_nInitResult = m_sqlQueryByUrl.prepare(&m_db, SQL_QUERY_MEDIA_BY_URL);
    assert(m_nInitResult == ERR_OK);

    loadPlaylists();

    m_allMedias = getAll(MLOB_ARTIST, -1);
    if (m_nInitResult != ERR_OK) {
        goto INIT_FAILED;
    }
    for (auto &media : m_allMedias->getAll()) {
        m_mapMedias[media->ID] = media;
    }

    m_fnNowPlaying = getAppDataDir() + "now-playing.json";
    loadNowPlaying();

    return m_nInitResult;

INIT_FAILED:
    close();
    return m_nInitResult;
}

void CMediaLibrary::close() {
    m_sqlAdd.finalize();
    m_sqlAddFast.finalize();
    m_sqlQueryByUrl.finalize();
    m_db.close();
}

PlaylistPtr CMediaLibrary::queryPlaylist(cstr_t szSQL, cstr_t szClause) {
    return queryPlaylist(szSQL, [szClause](CSqlite3Stmt &stmt) {
        return stmt.bindText(1, szClause);
    });
}

PlaylistPtr CMediaLibrary::queryPlaylist(cstr_t szSQL, int clause) {
    return queryPlaylist(szSQL, [clause](CSqlite3Stmt &stmt) {
        return stmt.bindInt(1, clause);
    });
}

PlaylistPtr CMediaLibrary::queryPlaylist(cstr_t szSQL) {
    return queryPlaylist(szSQL, [](CSqlite3Stmt &stmt) {
        return ERR_OK;
    });
}

PlaylistPtr CMediaLibrary::queryPlaylist(cstr_t SQL, std::function<int (CSqlite3Stmt &stmt)> callback) {
    if (!isOK()) {
        return nullptr;
    }

    RMutexAutolock autolock(m_mutexDataAccess);
    CSqlite3Stmt stmt;

    int ret = stmt.prepare(&m_db, SQL);
    if (ret != ERR_OK) {
        return nullptr;
    }

    ret = callback(stmt);
    if (ret != ERR_OK) {
        return nullptr;
    }

    return sqliteQueryPlaylist(this, stmt);
}

VecStrings CMediaLibrary::queryVStr(cstr_t szSql) {
    VecStrings results;

    if (!isOK()) {
        return results;
    }

    RMutexAutolock autolock(m_mutexDataAccess);
    CSqlite3Stmt sqlQuery;

    sqlQuery.prepare(&m_db, szSql);

    int ret = sqlQuery.step();
    while (ret == ERR_SL_OK_ROW) {
        auto text = (const char *)sqlQuery.columnText(0);
        if (text) {
            results.push_back(text);
        }

        ret = sqlQuery.step();
    }

    return results;
}

ResultCode CMediaLibrary::executeSQL(cstr_t szSql) {
    return m_db.exec(szSql);
}

ResultCode CMediaLibrary::undeleteMedia(long nMediaID) {
    char szSql[256];

    snprintf(szSql, CountOf(szSql), "update medialib set file_deleted!=1 where id=%d", (int)nMediaID);

    return executeSQL(szSql);
}

ResultCode CMediaLibrary::add(const MediaPtr &media) {
    if (!isOK()) {
        return m_nInitResult;
    }

    auto existedMedia = getMediaByUrl(media->url.c_str());
    if (existedMedia) {
        return ERR_EXIST;
    }

    RMutexAutolock autolock(m_mutexDataAccess);

    return doAddMedia(media);
}

string CMediaLibrary::getSettingValue(cstr_t name) {
    CSqlite3Stmt stmtQuery;
    auto ret = stmtQuery.prepare(&m_db, SQL_SETTINGS_QUERY);
    assert(ret == ERR_OK);

    ret = stmtQuery.bindStaticText(1, name);
    assert(ret == ERR_OK);

    ret = stmtQuery.step();
    if (ret == ERR_SL_OK_ROW) {
        return stmtQuery.columnText(0);
    }
    assert(ret == ERR_OK);

    return "";
}

void CMediaLibrary::setSettingValue(cstr_t name, cstr_t value) {
    CSqlite3Stmt stmtQuery;
    auto ret = stmtQuery.prepare(&m_db, SQL_SETTINGS_SET);
    assert(ret == ERR_OK);

    ret = stmtQuery.bindStaticText(1, name);
    assert(ret == ERR_OK);

    ret = stmtQuery.bindStaticText(2, value);
    assert(ret == ERR_OK);

    ret = stmtQuery.step();
    assert(ret == ERR_OK);
}

ResultCode CMediaLibrary::doAddMedia(const MediaPtr &media) {
    int ret;
    int n = 1;

    // url, artist, album, title, track, year, genre, comment, duration, filesize, time_added, music_hash, lyrics_file
    SQLITE3_BIND_TEXT(m_sqlAdd, media->url);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->artist);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->album);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->title);
    m_sqlAdd.bindInt(n++, media->trackNumb);
    m_sqlAdd.bindInt(n++, media->year);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->genre);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->comments);
    m_sqlAdd.bindInt(n++, (int)media->duration);
    m_sqlAdd.bindInt64(n++, (int)media->fileSize);
    m_sqlAdd.bindInt64(n++, media->timeAdded);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->musicHash);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->lyricsFile);
    SQLITE3_BIND_TEXT(m_sqlAdd, media->version);
    m_sqlAdd.bindInt(n++, media->discNumb);
    m_sqlAdd.bindInt(n++, (int)media->metaStatus);

    ret = m_sqlAdd.step();
    if (ret == ERR_OK) {
        media->ID = m_db.LastInsertRowId();
    }

    m_sqlAdd.reset();

    m_allMedias->insertItem(-1, media);
    m_mapMedias[media->ID] = media;

RET_FAILED:

    return ret;
}

PlaylistPtr CMediaLibrary::getNowPlaying() {
    return m_nowPlaying;
}

void CMediaLibrary::loadNowPlaying() {
    m_nowPlaying = std::make_shared<Playlist>();

    string data;
    if (!readFile(m_fnNowPlaying.c_str(), data)) {
        return;
    }

    rapidjson::Document doc;
    doc.Parse(data.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        return;
    }

    PlaylistInfo info;

    try {
        info.id = getMemberInt(doc, "id", -1);
        info.mediaIds = getMemberIntArray(doc, "media_ids");
    } catch (std::exception &e) {
        return;
    }

    m_nowPlaying = getPlaylist(info);
}

void CMediaLibrary::saveNowPlaying() {
    auto info = m_nowPlaying->toPlaylistInfo();

    RapidjsonWriterEx writer;
    writer.startObject();
    writer.writePropInt("id", info.id);
    writer.writePropIntArray("media_ids", info.mediaIds);
    writer.endObject();

    writeFile(m_fnNowPlaying.c_str(), writer.getStringView());
}

PlaylistPtr CMediaLibrary::newPlaylist(cstr_t name) {
    auto pl = make_shared<Playlist>();
    pl->name = name;
    pl->refreshTimeModified();

    auto info = pl->toPlaylistInfo();
    RMutexAutolock autolock(m_mutexDataAccess);
    info.id = pl->id = savePlaylistInDb(info);
    m_playlists.push_back(info);
    return pl;
}

int CMediaLibrary::upgradeCheck() {
    // create version table.
    auto ret = m_db.exec(SQL_CREATE_SETTINGS);
    if (ret != ERR_OK) {
        return ret;
    }

    string version = getSettingValue(MEDIA_LIB_VER_NAME);
    if (version == MEDIA_LIB_VER_CUR) {
        return ERR_OK;
    }

    // 追加式升级：只增列，不 DROP 重建（否则丢库）。
    // fresh 安装时 medialib 表还不存在（init 里 CREATE 在 upgradeCheck 之后），无需 ALTER。
    if (tableExists("medialib")) {
        addColumnIfMissing("medialib", "version");
        addColumnIfMissing("medialib", "disc_number");
        addColumnIfMissing("medialib", "metadata_status");
    }

    setSettingValue(MEDIA_LIB_VER_NAME, MEDIA_LIB_VER_CUR);

    return ERR_OK;
}

bool CMediaLibrary::tableExists(cstr_t table) {
    string sql = string("SELECT name FROM sqlite_master WHERE type='table' AND name='") + table + "'";
    CSqlite3Stmt stmt;
    if (stmt.prepare(&m_db, sql.c_str()) != ERR_OK) {
        return false;
    }
    bool found = stmt.step() == ERR_SL_OK_ROW;
    stmt.reset();
    return found;
}

bool CMediaLibrary::columnExists(cstr_t table, cstr_t col) {
    string sql = string("PRAGMA table_info(") + table + ")";
    CSqlite3Stmt stmt;
    if (stmt.prepare(&m_db, sql.c_str()) != ERR_OK) {
        return false;
    }
    bool found = false;
    while (stmt.step() == ERR_SL_OK_ROW) {
        string name;
        stmt.columnText(1, name);   // PRAGMA table_info: 第2列是 name
        if (name == col) {
            found = true;
            break;
        }
    }
    stmt.reset();
    return found;
}

void CMediaLibrary::addColumnIfMissing(cstr_t table, cstr_t col) {
    if (columnExists(table, col)) {
        return;
    }
    string sql = string("ALTER TABLE ") + table + " ADD COLUMN " + col + " ";
    // 列类型与默认值（对应 SQL_CREATE_MEDIALIB 的定义）。
    if (strcmp(col, "version") == 0) {
        sql += "text DEFAULT NULL";
    } else if (strcmp(col, "disc_number") == 0) {
        sql += "integer DEFAULT -1";
    } else if (strcmp(col, "metadata_status") == 0) {
        sql += "integer DEFAULT 0";
    }
    executeSQL(sql.c_str());
}

void CMediaLibrary::updateMediaInMem(Media *media) {
    auto it = m_mapMedias.find(media->ID);
    if (it != m_mapMedias.end()) {
        auto existing = (*it).second.get();
        if (existing != media) {
            *existing = *media;
        }
    }
}

void CMediaLibrary::removeMediaInMem(Media *media) {
    auto id = media->ID;
    auto count =  m_allMedias->getCount();

    for (uint32_t i = 0; i < count; i++) {
        auto item = m_allMedias->getItem(i);
        if (item) {
            if (item->ID == id) {
                m_allMedias->removeItem(i);
                auto it = m_mapMedias.find(item->ID);
                if (it != m_mapMedias.end()) {
                    m_mapMedias.erase(it);
                }
                return;
            }
        }
    }
}

void CMediaLibrary::markPlayFinishedInMem(Media *media) {
    auto id = media->ID;
    auto count =  m_allMedias->getCount();

    for (uint32_t i = 0; i < count; i++) {
        auto item = m_allMedias->getItem(i);
        if (item) {
            if (item->ID == id) {
                item->timePlayed = media->timePlayed;
                item->countPlayed = media->countPlayed;
                return;
            }
        }
    }
}

void CMediaLibrary::loadPlaylists() {
    m_playlists.clear();

    CSqlite3Stmt stmt;
    int ret = stmt.prepare(&m_db, SQL_QUERY_ALL_PLAYLISTS);
    assert(ret == ERR_OK);

    ret = stmt.step();
    while (ret == ERR_SL_OK_ROW) {
        PlaylistInfo info;
        int idx = 0;
        info.id = stmt.columnInt(idx++);
        info.name = stmt.columnText(idx++);
        info.duration = stmt.columnInt(idx++);
        info.count = stmt.columnInt(idx++);
        info.rating = stmt.columnInt(idx++);
        info.isUpToDate = stmt.columnInt(idx++) != 0;
        info.timeModified = stmt.columnInt64(idx++);
        string idsText = stmt.columnText(idx++);

        VecStrings ids;
        strSplit(idsText.c_str(), ',', ids);
        for (auto &id : ids) {
            info.mediaIds.push_back(atoi(id.c_str()));
        }

        m_playlists.push_back(info);

        ret = stmt.step();
    }
}

int CMediaLibrary::savePlaylistInDb(const PlaylistInfo &playlist) {
    CSqlite3Stmt stmt;

    if (playlist.id == -1) {
        auto ret = stmt.prepare(&m_db, SQL_ADD_PLAYLIST);
        assert(ret == ERR_OK);
    } else {
        auto ret = stmt.prepare(&m_db, SQL_UPDATE_PLAYLIST_BY_ID);
        assert(ret == ERR_OK);
    }

    string ids = strJoin(playlist.mediaIds.begin(), playlist.mediaIds.end(), "%d", ",");

    int idx = 1;
    auto ret = stmt.bindStaticText(idx++, playlist.name.c_str()); assert(ret == ERR_OK);
    ret = stmt.bindInt(idx++, playlist.duration); assert(ret == ERR_OK);
    ret = stmt.bindInt(idx++, playlist.count); assert(ret == ERR_OK);
    ret = stmt.bindInt(idx++, playlist.rating); assert(ret == ERR_OK);
    ret = stmt.bindInt(idx++, playlist.isUpToDate); assert(ret == ERR_OK);
    ret = stmt.bindInt64(idx++, playlist.timeModified); assert(ret == ERR_OK);
    ret = stmt.bindStaticText(idx++, ids.c_str()); assert(ret == ERR_OK);

    if (playlist.id != -1) {
        ret = stmt.bindInt(idx++, playlist.id); assert(ret == ERR_OK);
    }

    ret = stmt.step();
    assert(ret == ERR_OK);
    if (playlist.id == -1) {
        return m_db.LastInsertRowId();
    }
    return playlist.id;
}

PlaylistInfo *CMediaLibrary::getMemPlaylistInfoById(int id) {
    for (auto &item : m_playlists) {
        if (item.id == id) {
            return &item;
        }
    }

    return nullptr;
}

PlaylistPtr CMediaLibrary::getPlaylist(int playlistId) {
    RMutexAutolock autolock(m_mutexDataAccess);

    for (auto &info : m_playlists) {
        if (info.id == playlistId) {
            return getPlaylist(info);
        }
    }

    return nullptr;
}

PlaylistPtr CMediaLibrary::getPlaylist(PlaylistInfo &info) {
    if (info.playlist) {
        return info.playlist;
    }

    auto playlist = getMediaByIDs(info.mediaIds);
    playlist->setInfo(info);
    info.playlist = playlist;
    return playlist;
}

void CMediaLibrary::deltePlaylist(int playlistId) {
    string str = stringPrintf("DELETE FROM playlists WHERE id=%d", playlistId);

    RMutexAutolock autolock(m_mutexDataAccess);

    auto ret = m_db.exec(str.c_str());
    assert(ret == ERR_OK);

    for (auto it = m_playlists.begin(); it != m_playlists.end(); ++it) {
        auto &info = *it;
        if (info.id == playlistId) {
            m_playlists.erase(it);
            break;
        }
    }
}

/**
 * 根据目录名，将歌曲统计到对应的目录下
 *
 *  在统计的时候使用树状的结构. 目录下的子目录一般也不多，所以没有必要使用 map 之类
 */
struct MediaTree {
    string                      name;
    int                         mediaCount;     // 歌曲数量
    int                         mediaDuration;  // 总共时长，单位秒

    vector<MediaTree>           children;

    MediaTree(const string &name, int duration, int count = 0) : name(name), mediaDuration(duration), mediaCount(count) {
    }
};

void addMediaTree(MediaTree &parent, VecStrings::iterator itPathName, const VecStrings::iterator &end, int duration) {
    parent.mediaDuration += duration;
    parent.mediaCount++;

    if (itPathName == end) {
        return;
    }

    string &name = *itPathName;
    for (auto &child : parent.children) {
        if (child.name == name) {
            addMediaTree(child, ++itPathName, end, duration);
            return;
        }
    }

    // 添加
    parent.children.push_back(MediaTree(*itPathName, duration));
    addMediaTree(parent.children.back(), ++itPathName, end, duration);
}

void mediaTreeToFolderCategory(string path, MediaTree &parent, VecMediaCategories &mediaCategories) {
    path += PATH_SEP_STR + parent.name;

    if (parent.children.size() == 1 && parent.mediaCount == parent.children[0].mediaCount) {
        // 只有一层目录，就不必添加了
        mediaTreeToFolderCategory(path, parent.children[0], mediaCategories);
        return;
    }

    mediaCategories.push_back(MediaCategory(MediaCategory::FOLDER, parent.name.c_str(), path.c_str(), parent.mediaCount, parent.mediaDuration));

    for (auto &child : parent.children) {
        mediaTreeToFolderCategory(path, child, mediaCategories);
    }
}

void CMediaLibrary::updateMediaCategories() {
    const int64_t UPDATE_DURATION = 60 * 3;

    auto now = getTickCount();
    if (abs((int64_t)(now - m_timeMediaCategoryUpdated)) < UPDATE_DURATION) {
        return;
    }
    m_timeMediaCategoryUpdated = now;

    m_mediaCategories.clear();

    uint32_t totalDuration = 0;
    m_mediaCategories.push_back(MediaCategory(MediaCategory::ALL,
        "", "", m_allMedias->getCount(), 0));

    for (auto &info : m_playlists) {
        m_mediaCategories.push_back(MediaCategory(MediaCategory::PLAYLIST,
            info.name.c_str(), std::to_string(info.id).c_str(), info.count, info.duration));
    }

    unordered_map<string, MediaCategory> byArtist;
    unordered_map<string, MediaCategory> byAlbum;
    unordered_map<string, MediaCategory> byGenre;
    unordered_map<int, MediaCategory> byRating;
    MediaTree root("", 0, 0);
    int count = m_allMedias->getCount();

    for (int i = 0; i < count; i++) {
        auto media = m_allMedias->getItem(i);
        if (media) {
            int duration = media->duration / 1000;
            totalDuration += duration;

            // Artist
            auto artist = media->artist.c_str();
            auto it = byArtist.find(artist);
            if (it == byArtist.end()) {
                byArtist.insert(std::pair<string, MediaCategory>(string(artist), MediaCategory(MediaCategory::ARTIST, artist, artist, 1, duration)));
            } else {
                auto &info = (*it).second;
                info.mediaCount++;
                info.mediaDuration += duration;
            }

            // Artist-Album
            auto album = media->album.c_str();
            string key(media->artist);
            key.append(ALBUM_ARTIST_SEP);
            key.append(album);
            it = byAlbum.find(key);
            if (it == byAlbum.end()) {
                byAlbum.insert(std::pair<string, MediaCategory>(key, MediaCategory(MediaCategory::ALBUM, album, key.c_str(), 1, duration)));
            } else {
                auto &info = (*it).second;
                info.mediaCount++;
                info.mediaDuration += duration;
            }

            // Genre
            auto genre = media->genre.c_str();
            it = byGenre.find(genre);
            if (it == byGenre.end()) {
                byGenre.insert(std::pair<string, MediaCategory>(string(genre), MediaCategory(MediaCategory::GENRE, genre, genre, 1, duration)));
            } else {
                auto &info = (*it).second;
                info.mediaCount++;
                info.mediaDuration += duration;
            }

            // Rating
            auto rating = int(media->rating);
            {
                auto it = byRating.find(rating);
                if (it == byRating.end()) {
                    auto ratingStr = to_string(rating);
                    byRating.insert(std::pair<int, MediaCategory>(rating, MediaCategory(MediaCategory::RATING, ratingStr.c_str(), ratingStr.c_str(), 1, duration)));
                } else {
                    auto &info = (*it).second;
                    info.mediaCount++;
                    info.mediaDuration += duration;
                }
            }

            VecStrings paths;
            strSplit(media->url.c_str(), PATH_SEP_CHAR, paths);
            paths.pop_back(); // 去掉文件名
            addMediaTree(root, paths.begin(), paths.end(), duration);
        }
    }

    m_mediaCategories.front().mediaDuration = totalDuration;

    for (auto &item : byArtist) {
        m_mediaCategories.push_back(item.second);
    }

    for (auto &item : byAlbum) {
        m_mediaCategories.push_back(item.second);
    }

    for (auto &item : byGenre) {
        m_mediaCategories.push_back(item.second);
    }

    for (auto &item : byRating) {
        m_mediaCategories.push_back(item.second);
    }

    for (auto &parent : root.children) {
        auto &children = parent.children;
        for (auto &child : children) {
            mediaTreeToFolderCategory(parent.name, child, m_mediaCategories);
        }
    }

    std::sort(m_mediaCategories.begin(), m_mediaCategories.end(),
            [](const MediaCategory &a, const MediaCategory &b) {
        // 按照 type, mediaCount, name 来排序
        if (a.type < b.type) {
            return true;
        } else if (a.type == b.type) {
            if (a.mediaCount > b.mediaCount) {
                return true;
            } else if (a.mediaCount == b.mediaCount) {
                return a.name < b.name;
            }
        }
        return false;
    });
}
