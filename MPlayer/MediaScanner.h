#pragma once


#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Media.h"


class MediaScanner {
public:
    MediaScanner();
    virtual ~MediaScanner();

    void scanMedia(const MediaPtr &media);

    // Web Console：异步刷新已入库媒体的标签（不阻塞 HTTP 线程）.
    bool startLibraryRescan(const VecMediaPtrs &medias);
    bool isLibraryRescanRunning() const { return _rescanRunning.load(); }
    int rescanTotal() const { return _rescanTotal.load(); }
    int rescanDone() const { return _rescanDone.load(); }
    int snapshotVersion() const { return _snapshotVersion.load(); }
    // idle | running | finished
    const char *rescanStatus() const;

    void quit();

protected:
    void threadRun();

protected:
    std::mutex                  _mutex;
    std::condition_variable     _cv;
    std::thread                 _thread;
    VecMediaPtrs                _medias;
    volatile bool               _quit = false;

    std::atomic<bool>           _rescanRunning { false };
    std::atomic<int>            _rescanTotal { 0 };
    std::atomic<int>            _rescanDone { 0 };
    std::atomic<int>            _snapshotVersion { 1 };
    std::atomic<bool>           _everFinished { false };
};

extern MediaScanner g_mediaScanner;
