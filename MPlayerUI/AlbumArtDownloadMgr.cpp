#include "MPlayerApp.h"
#include "AlbumArtDownloadMgr.h"
#include "CurMediaAlbumArt.h"
#include "../ImageLib/RawImageData.h"
#include "MLProfile.h"
#include <chrono>
#include <vector>


CAlbumArtDownloadMgr g_albumArtDownloader;

static const int kDownloadIntervalMs = 5000;


CAlbumArtDownloadMgr::CAlbumArtDownloadMgr() {
}

CAlbumArtDownloadMgr::~CAlbumArtDownloadMgr() {
}

int CAlbumArtDownloadMgr::init() {
    m_quit = false;
    m_history.load();
    return ERR_OK;
}

void CAlbumArtDownloadMgr::quit() {
    {
        MutexAutolock lock(m_mutexAccess);
        m_quit = true;
        m_listTasks.clear();
    }
    m_cv.notify_all();
    m_threadDownload.join();
}

bool CAlbumArtDownloadMgr::autoDownloadEnabled() const {
    return g_profile.getBool(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_ENABLE, true);
}

string CAlbumArtDownloadMgr::getCustomSaveDir() const {
    string dir = CMLProfile::getDir(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_DOWN_PATH, "");
    if (dir.empty()) {
        dir = albumArtAppDataDir();
    }
    dirStringAddSep(dir);
    return dir;
}

void CAlbumArtDownloadMgr::setCustomSaveDir(cstr_t dir) {
    string d = dir ? dir : "";
    dirStringAddSep(d);
    CMLProfile::writeDir(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_DOWN_PATH, d.c_str());
}

void CAlbumArtDownloadMgr::createSources(vector<std::unique_ptr<IAlbumArtSource>> &sources) {
    sources.clear();
    int mode = g_profile.getInt(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_SOURCE, ALBUM_ART_SRC_AUTO);
    if (mode == ALBUM_ART_SRC_MUSICBRAINZ) {
        sources.emplace_back(createDefaultAlbumArtSource());
    } else if (mode == ALBUM_ART_SRC_CHINA) {
        sources.emplace_back(createChinaAlbumArtSource());
    } else {
        sources.emplace_back(createDefaultAlbumArtSource());
        sources.emplace_back(createChinaAlbumArtSource());
    }
}

bool CAlbumArtDownloadMgr::isCancelled(bool *cancel) {
    if (isQuiting()) {
        return true;
    }
    return cancel && *cancel;
}

bool CAlbumArtDownloadMgr::isQuiting() {
    MutexAutolock lock(m_mutexAccess);
    return m_quit;
}

static void notifyAlbumArtUpdated() {
    auto *disp = MPlayerApp::getEventsDispatcher();
    if (!disp) {
        return;
    }
    // 下载在工作线程：必须切回 UI 线程再刷新 AlbumArt 控件。
    disp->postExecInUIThread([]() {
        MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_PLAYER_CUR_MEDIA_INFO_CHANGED);
    });
}

static void albumArtLog(IAlbumArtDownloadLog *log, const string &line) {
    if (log) {
        log->onLog(line.c_str());
    }
    DBG_LOG1("%s", line.c_str());
}

static string identityLog(const MediaIdentity &id) {
    return stringPrintf("artist=\"%s\", album=\"%s\", title=\"%s\"",
        id.artist.c_str(), id.album.c_str(), id.title.c_str());
}

bool CAlbumArtDownloadMgr::waitInterruptible(int ms) {
    std::unique_lock<std::mutex> lock(m_mutexAccess);
    return m_cv.wait_for(lock, std::chrono::milliseconds(ms), [this]() { return m_quit; });
}

bool CAlbumArtDownloadMgr::currentMediaHasAlbumArt() const {
    CCurMediaAlbumArt albumArt;
    return albumArt.loadNext() != nullptr;
}

void CAlbumArtDownloadMgr::onSongChanged() {
    if (!autoDownloadEnabled()) {
        return;
    }
    if (!g_player.isMediaOpened()) {
        return;
    }

    string mediaFile = g_player.getSrcMedia();
    if (!isFileExist(mediaFile.c_str())) {
        return;
    }
    if (currentMediaHasAlbumArt()) {
        return;
    }

    AlbumArtDownloadTask task;
    task.mediaFile = mediaFile;
    task.candidates = extractMediaIdentityCandidates(
        g_player.getArtist(), g_player.getAlbum(), g_player.getTitle(), mediaFile.c_str());
    if (task.candidates.empty()) {
        DBG_LOG1("AlbumArt: no identity candidates for %s", mediaFile.c_str());
        return;
    }

    m_history.load();
    if (m_history.wasSearchedRecently(task.candidates)) {
        DBG_LOG1("AlbumArt: skip recently searched %s", albumArtHistoryKey(task.candidates[0]).c_str());
        return;
    }

    // 入队即记历史：失败也不在一个月内重试
    m_history.recordSearch(task.candidates);
    addTask(task);
}

void CAlbumArtDownloadMgr::addTask(const AlbumArtDownloadTask &task) {
    if (isTaskExist(task.mediaFile.c_str())) {
        return;
    }

    {
        MutexAutolock lock(m_mutexAccess);
        m_listTasks.push_back(task);
    }
    m_cv.notify_all();

    if (!m_threadDownload.isRunning()) {
        if (!m_threadDownload.create(downloadThreadProc, this)) {
            ERR_LOG0("FAILED to create album art downloading thread.");
        }
    }
}

bool CAlbumArtDownloadMgr::isTaskExist(cstr_t mediaFile) {
    MutexAutolock lock(m_mutexAccess);
    for (auto &task : m_listTasks) {
        if (strcasecmp(task.mediaFile.c_str(), mediaFile) == 0) {
            return true;
        }
    }
    return false;
}

void CAlbumArtDownloadMgr::downloadThreadProc(void *lpParam) {
    CAlbumArtDownloadMgr *pThis = (CAlbumArtDownloadMgr *)lpParam;
    pThis->downloadThread();
}

void CAlbumArtDownloadMgr::downloadThread() {
    while (true) {
        AlbumArtDownloadTask task;
        {
            std::unique_lock<std::mutex> lock(m_mutexAccess);
            m_cv.wait(lock, [this] { return m_quit || !m_listTasks.empty(); });
            if (m_quit) {
                return;
            }
            task = m_listTasks.front();
            m_listTasks.pop_front();
        }

        processTask(task, nullptr, nullptr);

        {
            MutexAutolock lock(m_mutexAccess);
            if (m_quit) {
                return;
            }
            if (m_listTasks.empty()) {
                continue;
            }
        }

        // 限速：5 秒一张
        if (waitInterruptible(kDownloadIntervalMs)) {
            return;
        }
    }
}

bool CAlbumArtDownloadMgr::saveAlbumArt(cstr_t mediaFile, const MediaIdentity &id, const string &imageData, cstr_t ext, string &savedPath) {
    savedPath.clear();

    string songTitle = fileNameFilterInvalidChars(fileGetTitle(mediaFile).c_str());
    if (songTitle.empty()) {
        songTitle = albumArtFileTitle(id);
    }
    string albumTitle = albumArtFileTitle(id);

    auto tryWrite = [&](const string &path) -> bool {
        string parent = fileGetPath(path.c_str());
        if (!parent.empty()) {
            createDirectoryAll(parent.c_str());
        }
        return writeFile(path.c_str(), imageData.c_str(), imageData.size());
    };

    int mode = g_profile.getInt(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_SAVE_MODE, ALBUM_ART_SAVE_NEXT_TO_SONG);
    string songDir = fileGetPath(mediaFile);
    string customDir = getCustomSaveDir();

    VecStrings candidates;
    if (mode == ALBUM_ART_SAVE_NEXT_TO_SONG) {
        if (isDirExist(songDir.c_str())) {
            candidates.push_back(songDir + songTitle + ext);
        }
        candidates.push_back(customDir + albumTitle + ext);
    } else {
        candidates.push_back(customDir + albumTitle + ext);
        if (isDirExist(songDir.c_str())) {
            candidates.push_back(songDir + songTitle + ext);
        }
    }

    for (auto &path : candidates) {
        if (tryWrite(path)) {
            savedPath = path;
            return true;
        }
    }

    ERR_LOG1("AlbumArt: failed to save cover for %s", mediaFile);
    return false;
}

void CAlbumArtDownloadMgr::downloadNow(const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel) {
    albumArtLog(log, stringPrintf("File: %s", task.mediaFile.c_str()));
    if (task.mediaFile.empty() || !isFileExist(task.mediaFile.c_str())) {
        albumArtLog(log, "Error: not a local song file.");
        return;
    }
    if (task.candidates.empty()) {
        albumArtLog(log, "Error: cannot extract artist/album/title.");
        return;
    }

    for (size_t i = 0; i < task.candidates.size(); i++) {
        albumArtLog(log, stringPrintf("Candidate %d: %s", (int)i + 1, identityLog(task.candidates[i]).c_str()));
    }

    m_history.recordSearch(task.candidates);

    if (processTask(task, log, cancel)) {
        return;
    }
    if (isCancelled(cancel)) {
        albumArtLog(log, "Cancelled.");
        return;
    }
    albumArtLog(log, "Finished: no album art downloaded.");
}

bool CAlbumArtDownloadMgr::processTask(const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel) {
    vector<std::unique_ptr<IAlbumArtSource>> sources;
    createSources(sources);
    for (auto &source : sources) {
        if (isCancelled(cancel)) {
            return false;
        }
        if (trySource(source.get(), task, log, cancel)) {
            return true;
        }
    }
    return false;
}

bool CAlbumArtDownloadMgr::trySource(IAlbumArtSource *source, const AlbumArtDownloadTask &task, IAlbumArtDownloadLog *log, bool *cancel) {
    if (!source) {
        return false;
    }

    albumArtLog(log, stringPrintf("Searching %s...", source->name()));

    for (auto &query : task.candidates) {
        if (isCancelled(cancel)) {
            return false;
        }

        albumArtLog(log, stringPrintf("  Query: %s", identityLog(query).c_str()));

        VecAlbumArtSearchResults results;
        if (!source->search(query, results)) {
            cstr_t err = source->lastError();
            if (err && err[0]) {
                albumArtLog(log, stringPrintf("  Error: %s", err));
            } else {
                albumArtLog(log, "  No search results.");
            }
            continue;
        }

        albumArtLog(log, stringPrintf("  Found %d result(s).", (int)results.size()));
        for (size_t i = 0; i < results.size(); i++) {
            auto &item = results[i];
            albumArtLog(log, stringPrintf("    [%d] score=%d %s %s",
                (int)i + 1, item.score, item.sourceName.c_str(), item.imageUrl.c_str()));
        }

        for (auto &item : results) {
            if (isCancelled(cancel)) {
                return false;
            }
            albumArtLog(log, stringPrintf("  Downloading %s", item.imageUrl.c_str()));
            string imageData, ext;
            if (!source->downloadImage(item, imageData, ext)) {
                cstr_t err = source->lastError();
                if (err && err[0]) {
                    albumArtLog(log, stringPrintf("  Download error: %s", err));
                } else {
                    albumArtLog(log, "  Download failed.");
                }
                continue;
            }

            string savedPath;
            if (!saveAlbumArt(task.mediaFile.c_str(), query, imageData, ext.c_str(), savedPath)) {
                albumArtLog(log, "  Error: failed to save image file.");
                continue;
            }

            albumArtLog(log, stringPrintf("Saved: %s", savedPath.c_str()));
            if (strIsISame(task.mediaFile.c_str(), g_player.getSrcMedia())) {
                notifyAlbumArtUpdated();
            }
            return true;
        }
    }
    return false;
}
