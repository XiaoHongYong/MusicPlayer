/********************************************************************
    Purpose  :    无封面时在后台线程自动搜索并下载专辑封面
*********************************************************************/

#pragma once

#include <condition_variable>
#include <memory>
#include "../Utils/Thread.h"
#include "AlbumArtQuery.h"
#include "AlbumArtSearchHistory.h"
#include "IAlbumArtSource.h"


struct AlbumArtDownloadTask {
    string                      mediaFile;
    VecMediaIdentities          candidates;
};

class IAlbumArtDownloadLog {
public:
    virtual ~IAlbumArtDownloadLog() {}
    virtual void onLog(cstr_t line) = 0;
};

enum AlbumArtSourceMode {
    ALBUM_ART_SRC_AUTO          = 0, // MusicBrainz，失败后国内源
    ALBUM_ART_SRC_MUSICBRAINZ   = 1,
    ALBUM_ART_SRC_CHINA         = 2,
};

enum AlbumArtSaveMode {
    ALBUM_ART_SAVE_NEXT_TO_SONG = 0, // 歌曲同目录、与歌曲同名
    ALBUM_ART_SAVE_CUSTOM_DIR   = 1, // 专门的封面下载目录
};

#define SZ_KEY_ALBUM_ART_ENABLE     "EnableAutoDownloadAlbumArt"
#define SZ_KEY_ALBUM_ART_SOURCE     "AlbumArtSource"
#define SZ_KEY_ALBUM_ART_SAVE_MODE  "AlbumArtSaveMode"
#define SZ_KEY_ALBUM_ART_DOWN_PATH  "AlbumArtDownPath"

class CAlbumArtDownloadMgr {
public:
    CAlbumArtDownloadMgr();
    virtual ~CAlbumArtDownloadMgr();

    int init();
    void quit();

    void onSongChanged();
    void downloadNow(const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel);

    bool autoDownloadEnabled() const;
    string getCustomSaveDir() const;
    void setCustomSaveDir(cstr_t dir);

protected:
    void addTask(const AlbumArtDownloadTask &task);
    bool isTaskExist(cstr_t mediaFile);

    static void downloadThreadProc(void *lpParam);
    void downloadThread();
    bool processTask(const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel);
    bool waitInterruptible(int ms);
    bool isQuiting();
    bool isCancelled(bool *cancel);

    bool currentMediaHasAlbumArt() const;
    bool saveAlbumArt(cstr_t mediaFile, const MediaIdentity &id, const string &imageData, cstr_t ext, string &savedPath);
    bool trySource(IAlbumArtSource *source, const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel);
    void createSources(vector<std::unique_ptr<IAlbumArtSource>> &sources);

protected:
    CThread                     m_threadDownload;
    std::mutex                  m_mutexAccess;
    std::condition_variable     m_cv;
    bool                        m_quit = false;
    list<AlbumArtDownloadTask>  m_listTasks;
    CAlbumArtSearchHistory      m_history;
};

extern CAlbumArtDownloadMgr g_albumArtDownloader;
