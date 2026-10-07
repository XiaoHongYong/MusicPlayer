#include "AudioAnalysisSender.hpp"
#include "Http/AudioAnalysisStream.hpp"
#include "Utils/rapidjson.h"

#include <algorithm>
#include <cmath>


AudioAnalysisSender &AudioAnalysisSender::instance() {
    static AudioAnalysisSender s;
    return s;
}

std::string AudioAnalysisSender::encodeFrame(const AudioAnalysisFrame &frame) const {
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
    w.StartObject();
    w.Key("sessionId"); w.Uint64(frame.sessionId);
    w.Key("sequence"); w.Uint64(frame.sequence);
    w.Key("samplePosition"); w.Uint64(frame.samplePosition);
    w.Key("sampleRate"); w.Uint(frame.sampleRate);
    w.Key("rms"); w.Double(frame.rms);
    w.Key("peak"); w.Double(frame.peak);
    w.Key("bass"); w.Double(frame.bass);
    w.Key("mid"); w.Double(frame.mid);
    w.Key("treble"); w.Double(frame.treble);
    w.Key("beat"); w.Double(frame.beat);
    w.Key("bands");
    w.StartArray();
    const uint16_t n = std::min<uint16_t>(frame.bandCount, (uint16_t)AudioAnalysisFrame::kMaxBands);
    for (uint16_t i = 0; i < n; ++i) {
        const int v = (int)std::lround(std::clamp(frame.bands[i], 0.f, 1.f) * 255.f);
        w.Int(v);
    }
    w.EndArray();
    w.EndObject();
    return buf.GetString();
}

void AudioAnalysisSender::onAudioAnalysisFrame(const AudioAnalysisFrame &frame) {
    if (HttpServer::AudioAnalysisStream::instance().subscriberCount() <= 0) {
        return;
    }
    HttpServer::AudioAnalysisStream::instance().publish(encodeFrame(frame));
}
