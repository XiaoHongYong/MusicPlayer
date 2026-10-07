#pragma once

#include "../MPlayerEngine/IPlayerCore.hpp"
#include <mutex>
#include <string>

/**
 * 把引擎 AudioAnalysisFrame 编成传输 JSON，推到 AudioAnalysisStream。
 * 由 AudioAnalysisStream 在有/无订阅者时 subscribe/unsubscribe。
 */
class AudioAnalysisSender : public IAudioAnalysisSink {
public:
    static AudioAnalysisSender &instance();

    void onAudioAnalysisFrame(const AudioAnalysisFrame &frame) override;

private:
    AudioAnalysisSender() = default;

    std::string encodeFrame(const AudioAnalysisFrame &frame) const;
};
