#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "IPlayerCore.hpp"
#include "IMPlayer.h"
#include "FBuffer.hpp"
#include "analysis/AudioAnalyzer.h"


class MPlayerCore: public IPlayerCore {
public:
    MPlayerCore();
    ~MPlayerCore();

    void quit() override;

    const char *getDescription() override;
    const char *getFileExtentions() override;
    bool getMediaInfo(const char *mediaUrl, IMediaInfo *pMedia) override;

    bool play(const char *mediaUrl, IMediaInfo *mediaTagsOut) override;
    bool pause() override;
    bool unpause() override;
    bool stop() override;

    bool isSeekable() override;
    bool seek(int pos) override;

    uint32_t getDuration() override { return _mediaDuration; }
    uint32_t getPos() override;
    PlayerState getState() override { return _state; }

    bool setVolume(int volume) override;
    int getVolume() override;

    bool setBalance(int balance) override;
    int getBalance() override;

    bool setEQ(const EQualizer *eq) override;
    bool getEQ(EQualizer *eq) override;

    bool supportsAudioAnalysis() const override { return true; }
    void setAudioAnalysisSink(IAudioAnalysisSink *sink) override;
    void setAudioAnalysisOptions(const AudioAnalysisOptions &options) override;

    void notifyEndOfPlaying();

protected:
    enum Command {
        CMD_NONE,
        CMD_SET_OUTPUT,
        CMD_PLAY,
        CMD_UNPAUSE,
        CMD_PAUSE,
        CMD_STOP,
        CMD_SEEK,
        CMD_QUIT,
    };

    struct AnalysisPcmItem {
        std::shared_ptr<FBuffer> buffer;
        uint64_t sessionId = 0;
        uint64_t samplePosition = 0;
    };

    void threadRun();
    void analysisThreadRun();

    Command waitForCommand();

    IMediaDecoderPtr newMediaDecoder(IMediaInput *input);
    IMediaInputPtr newMediaInput(cstr_t mediaUrl);
    IMediaOutputPtr newMediaOutput();

    void beginAnalysisSession();
    void seekAnalysis(uint64_t samplePosition);
    void enqueueAnalysisPcm(const std::shared_ptr<FBuffer> &buf);
    void clearAnalysisPcmQueue();
    // 当前耳放播放头（样点），不持 analysis 锁；用于门控分析进度
    uint64_t analysisPlayheadSamples() const;

    PlayerState                 _state = PS_STOPPED;

    std::mutex                  _mutex;
    std::condition_variable     _cv;
    std::thread                 _thread;
    volatile bool               _isQuit = false;

    Command                     _command = CMD_NONE;
    string                      _curMediaUrl;
    int                         _seekPos = 0;
    int                         _mediaDuration = 0;
    bool                        _isSeekable = false;

    IMediaOutputPtr             _output = nullptr;

    // —— 音频分析（独立线程，不阻塞 decode→write）——
    // 容量需覆盖声卡预缓冲（mac CoreAudio BUFFER_COUNT≈124），按播放头消费，禁止丢旧
    static constexpr size_t     kAnalysisPcmQueueCap = 256;
    // 略超前播放头，避免频谱「慢半拍」；不可大到秒级
    static constexpr uint64_t   kAnalysisPlayheadLeadMs = 80;

    std::mutex                  _analysisMutex;
    std::condition_variable     _analysisCv;
    std::thread                 _analysisThread;
    std::deque<AnalysisPcmItem> _analysisPcmQueue;
    AudioAnalyzer               _analyzer;
    AudioAnalysisOptions        _analysisOptions;
    IAudioAnalysisSink         *_analysisSink = nullptr;
    std::atomic<uint64_t>       _analysisSessionId{0};
    uint64_t                    _analysisSampleCursor = 0;
    uint32_t                    _analysisLastSampleRate = 0;
    bool                        _analysisSeekPending = false;
    uint64_t                    _analysisSeekPosition = 0;
    bool                        _analysisOptionsDirty = true;
};
