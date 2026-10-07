#include "MPlayerCore.h"
#include "MediaInputFile.h"
#include "MDMiniMp3.h"
#include "MDFaad.h"
#include "MDFlac.h"

#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include "win32/MOSoundCard.h"
#elif defined(_MAC_OS)
#include "mac/CoreAudioOutput.h"
#else
#endif


MPlayerCore::MPlayerCore() {
    if (!_thread.joinable()) {
        _thread = std::thread(&MPlayerCore::threadRun, this);
    }
    if (!_analysisThread.joinable()) {
        _analysisThread = std::thread(&MPlayerCore::analysisThreadRun, this);
    }
    _cv.notify_one();
}

MPlayerCore::~MPlayerCore() {
    quit();
}

void MPlayerCore::quit() {
    _isQuit = true;
    _cv.notify_one();
    _analysisCv.notify_all();
    if (_thread.joinable()) {
        _thread.join();
    }
    if (_analysisThread.joinable()) {
        _analysisThread.join();
    }
}

const char *MPlayerCore::getDescription() {
    return "MPlayer Core Implementation";
}

const char *MPlayerCore::getFileExtentions()  {
    return ".mp3|MP3 files|.mp4|MP4 files|.wma|WMA files|.mp2|MP2 files|.m4a|M4A files|.flac|FLAC files|.aac|AAC files";
}

bool MPlayerCore::getMediaInfo(const char *mediaUrl, IMediaInfo *pMedia)  {
    return false;
}

bool MPlayerCore::play(const char *mediaUrl, IMediaInfo *mediaTagsOut)  {
    {
        std::lock_guard<std::mutex> autolock(_mutex);
        _curMediaUrl = mediaUrl;
    }

    _command = CMD_PLAY;
    _cv.notify_one();
    return true;
}

bool MPlayerCore::pause()  {
    _command = CMD_PAUSE;
    _cv.notify_one();
    return true;
}

bool MPlayerCore::unpause()  {
    _command = CMD_UNPAUSE;
    _cv.notify_one();
    return true;
}

bool MPlayerCore::stop()  {
    _command = CMD_STOP;
    _cv.notify_one();
    return true;
}

bool MPlayerCore::isSeekable()  {
    std::lock_guard<std::mutex> autolock(_mutex);
    return _isSeekable;
}

bool MPlayerCore::seek(int pos)  {
    if (_isSeekable && _command == CMD_NONE) {
        _seekPos = pos;
        _command = CMD_SEEK;
    }
    return true;
}

uint32_t MPlayerCore::getPos() {
    std::lock_guard<std::mutex> autolock(_mutex);
    return _seekPos + _output->getPos();
}

bool MPlayerCore::setVolume(int volume) {
    std::lock_guard<std::mutex> autolock(_mutex);
    return _output->setVolume(volume, 0) == ERR_OK;
}

int MPlayerCore::getVolume() {
    std::lock_guard<std::mutex> autolock(_mutex);
    return _output->getVolume();
}

bool MPlayerCore::setBalance(int balance) {
    return false;
}

int MPlayerCore::getBalance() {
    return 0;
}

bool MPlayerCore::setEQ(const EQualizer *eq) {
    return false;
}

bool MPlayerCore::getEQ(EQualizer *eq) {
    return false;
}

void MPlayerCore::setAudioAnalysisSink(IAudioAnalysisSink *sink) {
    std::lock_guard<std::mutex> lock(_analysisMutex);
    _analysisSink = sink;
    if (!sink) {
        _analysisPcmQueue.clear();
    }
    _analysisCv.notify_one();
}

void MPlayerCore::setAudioAnalysisOptions(const AudioAnalysisOptions &options) {
    std::lock_guard<std::mutex> lock(_analysisMutex);
    _analysisOptions = options;
    _analysisOptionsDirty = true;
    _analysisCv.notify_one();
}

void MPlayerCore::beginAnalysisSession() {
    std::lock_guard<std::mutex> lock(_analysisMutex);
    _analysisSessionId.fetch_add(1);
    _analysisSampleCursor = 0;
    _analysisLastSampleRate = 0;
    _analysisPcmQueue.clear();
    _analysisSeekPending = true;
    _analysisSeekPosition = 0;
    _analysisCv.notify_one();
}

void MPlayerCore::seekAnalysis(uint64_t samplePosition) {
    std::lock_guard<std::mutex> lock(_analysisMutex);
    _analysisSampleCursor = samplePosition;
    _analysisPcmQueue.clear();
    _analysisSeekPending = true;
    _analysisSeekPosition = samplePosition;
    _analysisCv.notify_one();
}

void MPlayerCore::clearAnalysisPcmQueue() {
    std::lock_guard<std::mutex> lock(_analysisMutex);
    _analysisPcmQueue.clear();
}

uint64_t MPlayerCore::analysisPlayheadSamples() const {
    // 不抢 _mutex / _analysisMutex，避免与 decode→enqueue 死锁
    const uint32_t seekMs = (uint32_t)std::max(0, _seekPos);
    uint32_t outMs = 0;
    if (_output) {
        outMs = _output->getPos();
    }
    uint32_t sr = _analysisLastSampleRate;
    if (sr == 0) {
        sr = 44100;
    }
    return ((uint64_t)seekMs + (uint64_t)outMs) * (uint64_t)sr / 1000ull;
}

void MPlayerCore::enqueueAnalysisPcm(const std::shared_ptr<FBuffer> &buf) {
    if (!buf || buf->size() == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(_analysisMutex);
    if (_analysisSink == nullptr) {
        return;
    }

    const int channels = std::max(1, buf->channels());
    const int bytesPerSample = std::max(1, buf->bps() / 8);
    const size_t frameCount = buf->size() / (size_t)(bytesPerSample * channels);
    if (buf->sampleRate() > 0) {
        _analysisLastSampleRate = (uint32_t)buf->sampleRate();
    }

    // 队列满：丢「最新」而不是丢「最旧」。最旧对应即将播出的声音，必须留给分析线程按播放头消费。
    if (_analysisPcmQueue.size() >= kAnalysisPcmQueueCap) {
        _analysisSampleCursor += frameCount;
        return;
    }

    AnalysisPcmItem item;
    item.buffer = buf;
    item.sessionId = _analysisSessionId.load();
    item.samplePosition = _analysisSampleCursor;
    _analysisSampleCursor += frameCount;

    _analysisPcmQueue.push_back(std::move(item));
    _analysisCv.notify_one();
}

void MPlayerCore::analysisThreadRun() {
    uint64_t appliedSession = 0;
    AudioAnalysisFrame frames[8];

    while (!_isQuit) {
        AnalysisPcmItem item;
        IAudioAnalysisSink *sink = nullptr;
        bool doSeek = false;
        uint64_t seekPos = 0;
        uint64_t sessionNow = 0;
        bool applyOptions = false;
        AudioAnalysisOptions options;
        bool waitedForPlayhead = false;

        {
            std::unique_lock<std::mutex> lock(_analysisMutex);
            _analysisCv.wait(lock, [&]() {
                return _isQuit || _analysisOptionsDirty || _analysisSeekPending
                    || !_analysisPcmQueue.empty();
            });
            if (_isQuit) {
                break;
            }

            if (_analysisOptionsDirty) {
                options = _analysisOptions;
                _analysisOptionsDirty = false;
                applyOptions = true;
            }

            sessionNow = _analysisSessionId.load();
            if (_analysisSeekPending) {
                doSeek = true;
                seekPos = _analysisSeekPosition;
                _analysisSeekPending = false;
            }

            sink = _analysisSink;

            // 门控：只有 PCM 时间进入播放头附近才分析，避免声卡大缓冲时频谱超前数秒
            if (!_analysisPcmQueue.empty() && sink != nullptr) {
                const uint64_t frontPos = _analysisPcmQueue.front().samplePosition;
                uint32_t sr = _analysisLastSampleRate;
                if (sr == 0 && _analysisPcmQueue.front().buffer) {
                    sr = (uint32_t)_analysisPcmQueue.front().buffer->sampleRate();
                }
                if (sr == 0) {
                    sr = 44100;
                }
                const uint64_t leadSamples =
                    (uint64_t)kAnalysisPlayheadLeadMs * (uint64_t)sr / 1000ull;

                lock.unlock();
                const uint64_t playhead = analysisPlayheadSamples();
                lock.lock();

                if (_isQuit) {
                    break;
                }
                // unlock 期间队列可能被 session/seek 清空
                if (_analysisPcmQueue.empty()
                    || _analysisPcmQueue.front().samplePosition != frontPos) {
                    continue;
                }

                if (playhead + leadSamples < frontPos) {
                    waitedForPlayhead = true;
                    _analysisCv.wait_for(lock, std::chrono::milliseconds(20));
                } else {
                    item = std::move(_analysisPcmQueue.front());
                    _analysisPcmQueue.pop_front();
                }
            }
        }

        // Analyzer 只在本线程访问（即使在等播放头，也先应用 session/options/seek）
        if (applyOptions) {
            _analyzer.setOptions(options);
        }
        if (sessionNow != appliedSession) {
            _analyzer.reset(sessionNow, doSeek ? seekPos : 0);
            appliedSession = sessionNow;
            doSeek = false;
        } else if (doSeek) {
            _analyzer.seek(seekPos);
        }

        if (waitedForPlayhead || !item.buffer || sink == nullptr) {
            continue;
        }
        if (item.sessionId != _analysisSessionId.load()) {
            continue;
        }

        // 仅处理 16-bit PCM（当前解码器主流输出）
        if (item.buffer->bps() != 16) {
            continue;
        }

        const int channels = item.buffer->channels();
        const uint32_t sampleRate = (uint32_t)item.buffer->sampleRate();
        const size_t frameCount =
            item.buffer->size() / (size_t)(std::max(1, item.buffer->bps() / 8) * std::max(1, channels));
        if (frameCount == 0 || sampleRate == 0) {
            continue;
        }

        const auto *samples = reinterpret_cast<const int16_t *>(item.buffer->data());
        const size_t n = _analyzer.pushInterleavedS16(
            samples, frameCount, channels, sampleRate, frames, 8);
        for (size_t i = 0; i < n; ++i) {
            IAudioAnalysisSink *s = nullptr;
            {
                std::lock_guard<std::mutex> lock(_analysisMutex);
                s = _analysisSink;
            }
            if (s) {
                s->onAudioAnalysisFrame(frames[i]);
            }
        }
    }
}

/**
 * To simplify the implementation of decoder/output, MPlayerCore::threadRun() is responsible for
 * the state management of these componenets, receive the commands from user, process the commands.
 *
 * Playback path stays single-threaded (decode → write). Analysis runs on a separate thread.
 */
void MPlayerCore::threadRun() {
    _output = newMediaOutput();

    while (!_isQuit) {
        if (_command != CMD_PLAY) {
            notifyEndOfPlaying();
        }

        auto cmd = waitForCommand();
        if (cmd == CMD_QUIT) {
            return;
        }
        assert(cmd == CMD_PLAY || _isQuit);
        if (cmd != CMD_PLAY) {
            continue;
        }

        auto input = newMediaInput(_curMediaUrl.c_str());
        if (input->open(_curMediaUrl.c_str()) != ERR_OK) {
            notifyEndOfPlaying();
            continue;
        }

        auto decoder = newMediaDecoder(input.get());
        int ret = decoder->open(input.get());
        if (ret != ERR_OK) {
            DBG_LOG1("Failed to play media: %s", _curMediaUrl.c_str());
            Sleep(300);
            notifyEndOfPlaying();
            continue;
        }

        auto _stop = [this]() {
            _state = PS_STOPPED;
            _output->stop();
            clearAnalysisPcmQueue();
        };

        _isSeekable = decoder->isSeekable();

        _mediaDuration = decoder->getDuration();
        _seekPos = 0;
        _state = PS_PLAYING;
        beginAnalysisSession();
        _output->stop();
        _output->play();
        while (_state != PS_STOPPED && !_isQuit) {
            while (_state == PS_PAUSED || (_command != CMD_NONE && _state != PS_STOPPED)) {
                auto cmd = waitForCommand();
                switch (cmd) {
                    case CMD_NONE:
                        assert(0);
                        break;
                    case CMD_SET_OUTPUT:
                        _output = newMediaOutput();
                        break;
                    case CMD_PLAY:
                        _command = cmd;
                        _stop();
                        break;
                    case CMD_UNPAUSE:
                        if (_state == PS_PAUSED) {
                            _output->play();
                        }
                        _state = PS_PLAYING;
                        break;
                    case CMD_PAUSE:
                        _state = PS_PAUSED;
                        _output->pause();
                        break;
                    case CMD_STOP:
                        _stop();
                        break;
                    case CMD_SEEK: {
                        assert(decoder->isSeekable());
                        decoder->seek(_seekPos);
                        _output->flush();
                        {
                            uint32_t sr = 0;
                            {
                                std::lock_guard<std::mutex> lock(_analysisMutex);
                                sr = _analysisLastSampleRate;
                            }
                            if (sr == 0) {
                                sr = 44100;
                            }
                            const uint64_t samplePos =
                                (uint64_t)_seekPos * (uint64_t)sr / 1000ull;
                            seekAnalysis(samplePos);
                        }
                        break;
                    }
                    case CMD_QUIT:
                        return;
                }
            }

            int emptyBufCount = 0;
            while (_state == PS_PLAYING && !_isQuit) {
                bool canWrite = _output->waitForWrite(100) == ERR_OK;
                if (_command != CMD_NONE) {
                    break;
                }

                if (canWrite) {
                    auto buf = std::make_shared<FBuffer>();
                    if (decoder->decode(buf.get()) == ERR_OK) {
                        if (buf->size() > 0) {
                            _output->write(buf.get());
                            enqueueAnalysisPcm(buf);
                            emptyBufCount = 0;
                            continue;
                        } else if (emptyBufCount < 20) {
                            emptyBufCount++;
                            continue;
                        }
                    }

                    Sleep(50);
                    if (!_output->isPlaying()) {
                        notifyEndOfPlaying();
                        break;
                    }
                }
            }
        }
    }
}

MPlayerCore::Command MPlayerCore::waitForCommand() {
    if (_command == CMD_NONE) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock);
    }

    auto cmd = _command;
    _command = CMD_NONE;
    return cmd;
}

IMediaDecoderPtr MPlayerCore::newMediaDecoder(IMediaInput *input) {
    StringView ext(fileGetExt(input->getSource()));
    if (ext.iEqual(".mp3")) {
        return std::make_shared<MDMiniMp3>();
    } else if (ext.iEqual(".mp4") || ext.iEqual(".m4a") || ext.iEqual(".aac")) {
        return std::make_shared<MDFaad>();
    } else if (ext.iEqual(".flac")) {
        return std::make_shared<MDFlac>();
    } else {
        return std::make_shared<MDMiniMp3>();
    }
}

IMediaInputPtr MPlayerCore::newMediaInput(cstr_t mediaUrl) {
    auto input = std::make_shared<MediaInputFile>();
    return input;
}

IMediaOutputPtr MPlayerCore::newMediaOutput() {
#ifdef _WIN32
    return std::make_shared<MOSoundCard>();
#elif defined(_MAC_OS)
    return std::make_shared<CoreAudioOutput>();
#else
    return nullptr;
#endif
}

void MPlayerCore::notifyEndOfPlaying() {
    _state = PS_STOPPED;
    clearAnalysisPcmQueue();

    if (m_callback) {
        m_callback->onEndOfPlaying();
    }
}
