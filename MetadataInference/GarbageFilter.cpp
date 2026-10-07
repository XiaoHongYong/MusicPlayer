#include "GarbageFilter.h"
#include "Utils/rapidjson.h"
#include "Utils/Utils.h"

namespace MetadataInference {

GarbageFilter::GarbageFilter() {
    // 内置默认表（可用 Resources/metadata_words.json 覆盖，见 loadConfig）。
    _hardTechnical = { "320k", "320kbps", "256kbps", "192kbps", "128kbps", "96kbps", "64kbps",
                       "flac", "lossless", "ape", "wav", "mp3", "aac", "ogg", "hi-res", "hires",
                       "16bit", "24bit", "32bit", "48khz", "96khz", "192khz", "v0", "v2", "remastered" };
    _hardSource = { "web", "web-dl", "cdda", "cd", "vinyl", "rip", "cdrip", "dvdrip", "ostrip",
                    "digital", "master", "itunes", "qobuz", "deezer", "tidal", "spotify",
                    "hdtracks", "studio-master" };
    _softPromo = { "高品质", "无损", "超清", "收藏版", "完整版", "原版", "正式版", "试听版",
                   "特别版", "珍藏版", "经典", "精选", "极品", "发烧", "歌词", "伴奏", "纯音乐",
                   "铃声", "广告", "试听", "免费下载", "付费", "单曲", "mv" };
    _semanticVersion = { "live", "remix", "acoustic", "instrumental", "unplugged", "radio edit",
                         "extended mix", "club mix", "mono", "stereo", "demo", "edit", "cover",
                         "karaoke", "original mix" };
}

bool GarbageFilter::loadConfig(const std::string &jsonText) {
    rapidjson::Document doc;
    doc.Parse(jsonText.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }
    auto hasMemberArray = [&](const char *a, const char *b) -> bool {
        if (!doc.HasMember(a)) {
            return false;
        }
        const auto &va = doc[a];
        if (!va.IsObject()) {
            return false;
        }
        return va.HasMember(b) && va[b].IsArray();
    };
    auto fill = [&](std::vector<std::string> &out, const char *a, const char *b) {
        if (!hasMemberArray(a, b)) {
            return;
        }
        const auto &arr = doc[a][b];
        for (auto &v : arr.GetArray()) {
            if (v.IsString()) {
                out.push_back(v.GetString());
            }
        }
    };
    fill(_hardTechnical, "hard", "technical");
    fill(_hardSource, "hard", "source");
    fill(_softPromo, "soft", "promotional");
    fill(_semanticVersion, "semantic", "version");
    return hasMemberArray("hard", "technical") || hasMemberArray("semantic", "version");
}

TokenClass GarbageFilter::classifyToken(const std::string &key) const {
    auto in = [&](const std::vector<std::string> &v) -> bool {
        for (auto &s : v) {
            if (key == s) {
                return true;
            }
        }
        return false;
    };
    if (in(_semanticVersion)) return TokenClass::SemanticVersion;
    if (in(_hardTechnical)) return TokenClass::HardTechnical;
    if (in(_hardSource)) return TokenClass::HardSource;
    if (in(_softPromo)) return TokenClass::SoftPromo;
    return TokenClass::Normal;
}

bool GarbageFilter::matchTechnicalNumber(const std::string &key) const {
    // 320k / 192kbps / 24bit / 44.1khz
    size_t i = 0;
    while (i < key.size() && key[i] >= '0' && key[i] <= '9') {
        ++i;
    }
    if (i == 0) {
        return false;
    }
    size_t j = i;
    if (j < key.size() && key[j] == '.') {   // 小数：44.1khz
        ++j;
        while (j < key.size() && key[j] >= '0' && key[j] <= '9') {
            ++j;
        }
    }
    std::string tail = key.substr(j);
    return tail == "k" || tail == "kbps" || tail == "khz" || tail == "hz"
        || tail == "bit" || tail == "bits" || tail == "mbps";
}

bool isBracketNoise(const std::string &key) {
    static const std::vector<std::string> NOISE = {
        "official", "official video", "official audio", "audio", "video", "lyric video",
        "mv", "music video", "ost", "soundtrack", "clip", "单曲", "mv", "专辑版", "live版", "feat"
    };
    for (auto &n : NOISE) {
        if (key == n) {
            return true;
        }
    }
    return false;
}

} // namespace MetadataInference