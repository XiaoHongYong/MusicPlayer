//
//  IPlayerCore.hpp
//  MPlayerLib
//
//  Created by henry_xiao on 2023/2/5.
//

#ifndef IPlayerCore_hpp
#define IPlayerCore_hpp

#include <assert.h>
#include <cstddef>
#include <stdint.h>
#include <string>

#include "analysis/AudioAnalysisFrame.h"
#include "analysis/AudioAnalysisOptions.h"

class IAudioAnalysisSink {
public:
    virtual ~IAudioAnalysisSink() = default;

    // 保证：不在 MPlayerCore 实时播放线程（threadRun）上调用。
    virtual void onAudioAnalysisFrame(const AudioAnalysisFrame &frame) = 0;
};


enum {
    EQ_BANDS_COUNT              = 18,
    EQ_DB_MIN                   = -20,
    EQ_DB_DEF                   = 0,
    EQ_DB_MAX                   = 20,
};

enum {
    MP_VOLUME_MAX               = 100,
    MP_VOL_BALANCE_MIN          = -100,
    MP_VOL_BALANCE_MAX          = 100,
};

enum PlayerState {
    PS_STOPPED                   = 1,
    PS_PLAYING                  = 2,
    PS_PAUSED                   = 3
};

enum MediaAttribute {
    MA_ARTIST                   = 0,
    MA_ALBUM                    = 1,
    MA_TITLE                    = 2,
    MA_TRACK_NUMB               = 3,
    MA_YEAR                     = 4,
    MA_GENRE                    = 5,
    MA_COMMENT                  = 6,
    MA_DURATION                 = 7,
    MA_FILESIZE                 = 8,
    MA_TIME_ADDED               = 9,
    MA_TIME_PLAYED              = 10,
    MA_RATING                   = 11,
    MA_TIMES_PLAYED             = 13,
    MA_LYRICS_FILE              = 15,

    // the following info will not be stored in media library
    MA_FORMAT                   = 16,
    MA_BITRATE                  = 18,
    MA_BITS_PER_SAMPLE                      = 19,   // Bits Per Sample.
    MA_CHANNELS                 = 20,
    MA_SAMPLE_RATE              = 21,
};

class IMediaInfo {
public:
    virtual ~IMediaInfo() { }

    //
    // attribute methods
    //
    virtual void setAttribute(MediaAttribute mediaAttr, const char *value) = 0;
    virtual void setAttribute(MediaAttribute mediaAttr, int64_t value) = 0;

};

struct EQualizer {
    EQualizer() {
        isEnable = false;
        preamp = EQ_DB_DEF;

        for (int i = 0; i < EQ_BANDS_COUNT; i++) {
            data[i] = EQ_DB_DEF;
        }
    }
    bool                        isEnable;
    int                         preamp;            // set value EQ_DB_MAX to EQ_DB_MIN (default EQ_DB_DEF)
    int                         data[EQ_BANDS_COUNT]; // set value EQ_DB_MAX to EQ_DB_MIN (default EQ_DB_DEF)
};

class IPlayerCoreCallback {
public:
    virtual void onEndOfPlaying() = 0;
    virtual void onErrorOccured(const char *errorCode) = 0;

};

/**
 * 仅仅提供音乐 播放器的核心支持
 * 
 * 和具体的插件接口等无关，可以使用系统自带的播放器实现，也可从头到尾全部实现一遍.
 */
class IPlayerCore {
public:
    IPlayerCore() { m_callback = nullptr; }
    virtual ~IPlayerCore() { }

    virtual void quit() = 0;

    // 实现的说明
    virtual const char *getDescription() = 0;

    // 返回支持的文件扩展名，比如: ".mp3|MP3 files|.mp4|MP4 files";
    virtual const char *getFileExtentions() = 0;

    // 获取媒体的标签
    virtual bool getMediaInfo(const char *mediaUrl, IMediaInfo *pMedia) = 0;

    //
    // Player control
    //
    virtual bool play(const char *mediaUrl, IMediaInfo *mediaTagsOut) = 0;
    virtual bool pause() = 0;
    virtual bool unpause() = 0;
    virtual bool stop() = 0;

    virtual bool isSeekable() = 0;
    virtual bool seek(int pos) = 0;

    //
    // Current playing Media state
    //
    virtual uint32_t getDuration() = 0;
    virtual uint32_t getPos() = 0;
    virtual PlayerState getState() = 0;

    //
    // Player settings
    //
    // 0 ~ 100
    virtual bool setVolume(int volume) = 0;
    virtual int getVolume() = 0;

    // -100 ~ 100
    virtual bool setBalance(int balance) = 0;
    virtual int getBalance() = 0;

    virtual bool setEQ(const EQualizer *eq) = 0;
    virtual bool getEQ(EQualizer *eq) = 0;

    // 音频分析（可视化）：默认不支持。有 PCM 旁路的核可覆盖。
    virtual bool supportsAudioAnalysis() const { return false; }

    // sink == nullptr：关闭分析（停入队、停算 FFT）
    virtual void setAudioAnalysisSink(IAudioAnalysisSink *sink) { (void)sink; }

    virtual void setAudioAnalysisOptions(const AudioAnalysisOptions &options) {
        (void)options;
    }

    void setCallback(IPlayerCoreCallback *callback) { m_callback = callback; }

protected:
    IPlayerCoreCallback             *m_callback;

};

#endif /* IPlayerCore_hpp */
