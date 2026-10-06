#include "TinyJS/utils/Utils.h"
#include "Player.h"
#include "MediaScanner.h"


MediaScanner g_mediaScanner;

MediaScanner::MediaScanner() {
}

MediaScanner::~MediaScanner() {
    quit();
}

void MediaScanner::scanMedia(const MediaPtr &media) {
    {
        std::lock_guard autolock(_mutex);
        _medias.push_back(media);
    }

    if (!_thread.joinable()) {
        _thread = std::thread(&MediaScanner::threadRun, this);
    }

    _cv.notify_one();
}

bool MediaScanner::startLibraryRescan(const VecMediaPtrs &medias) {
    bool expected = false;
    if (!_rescanRunning.compare_exchange_strong(expected, true)) {
        return false;
    }

    {
        std::lock_guard autolock(_mutex);
        _medias.insert(_medias.end(), medias.begin(), medias.end());
        _rescanTotal.store((int)medias.size());
        _rescanDone.store(0);
        _everFinished.store(false);
    }

    if (!_thread.joinable()) {
        _thread = std::thread(&MediaScanner::threadRun, this);
    }

    _cv.notify_one();
    return true;
}

const char *MediaScanner::rescanStatus() const {
    if (_rescanRunning.load()) {
        return "running";
    }
    if (_everFinished.load()) {
        return "finished";
    }
    return "idle";
}

void MediaScanner::quit() {
    _quit = true;
    _cv.notify_one();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void MediaScanner::threadRun() {
    while (!_quit) {
        VecMediaPtrs medias;

        {
            std::lock_guard autolock(_mutex);
            medias = _medias;
            _medias.clear();
        }

        if (medias.empty()) {
            std::unique_lock lock(_mutex);
            _cv.wait(lock);
            medias = _medias;
            _medias.clear();
        }

        bool fromRescan = _rescanRunning.load();

        for (auto &media : medias) {
            if (_quit) {
                break;
            }
            g_player.updateMediaInfo(media.get());
            if (fromRescan) {
                _rescanDone.fetch_add(1);
            }
        }

        if (fromRescan && !_quit) {
            std::lock_guard autolock(_mutex);
            if (_medias.empty()) {
                _rescanRunning.store(false);
                _everFinished.store(true);
                _snapshotVersion.fetch_add(1);
            }
        }

        // Wait for some time
        Sleep(50);
    }
}
