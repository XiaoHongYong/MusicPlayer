#include "NumberParser.h"
#include "Utils/Utils.h"

namespace MetadataInference {

int NumberParser::currentYear() const {
    time_t t = time(nullptr);
    struct tm tmv;
#if defined(_MAC_OS) || defined(__APPLE__)
    localtime_r(&t, &tmv);
#else
    localtime_s(&tmv, &t);
#endif
    return tmv.tm_year + 1900;
}

int NumberParser::parseYearStrict(cstr_t tokenKey) const {
    if (!tokenKey || !*tokenKey) {
        return 0;
    }
    // 只接受纯 4 位数字。
    if (tokenKey[0] < '0' || tokenKey[0] > '9') {
        return 0;
    }
    int year = 0;
    int len = 0;
    for (cstr_t p = tokenKey; *p && *p >= '0' && *p <= '9'; ++p) {
        year = year * 10 + (*p - '0');
        if (++len > 4) {
            return 0;
        }
    }
    if (len != 4) {
        return 0;
    }
    int minYear = 1900;
    int maxYear = currentYear() + 1;
    if (year < minYear || year > maxYear) {
        return 0;
    }
    return year;
}

int NumberParser::parseLeadingTrack(const std::string &s, size_t &consumed) const {
    size_t i = 0;
    // 可选单个轨道前缀字母：A01 / B01
    if (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
        ++i;
    }
    size_t digitsStart = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        ++i;
    }
    if (i == digitsStart) {
        consumed = 0;
        return -1;
    }
    int track = 0;
    for (size_t k = digitsStart; k < i; ++k) {
        track = track * 10 + (s[k] - '0');
    }
    // 消费数字后的单个分隔符（点/连字符/空格）。
    size_t j = i;
    if (j < s.size() && (s[j] == '.' || s[j] == '-' || s[j] == '_')) {
        ++j;
    }
    consumed = j;
    return track;
}

int NumberParser::parseTrackDisc(const std::string &s, int &discOut, size_t &consumed) const {
    discOut = -1;

    // "Disc 1 Track 2"
    if (s.compare(0, 4, "Disc") == 0 || s.compare(0, 4, "disc") == 0) {
        size_t i = 4;
        while (i < s.size() && s[i] == ' ') ++i;
        int disc = 0;
        size_t ds = i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') { disc = disc * 10 + (s[i] - '0'); ++i; }
        if (i > ds) {
            // 跳过 " Track "（"Track" 后必须跟空格或连字符，才视为轨道关键字）。
            while (i < s.size() && s[i] == ' ') ++i;
            if (i + 5 < s.size() && (s[i] == 'T' || s[i] == 't') && s[i + 1] == 'r'
                && (s[i + 5] == ' ' || s[i + 5] == '-')) {
                i += 5;   // 越过 "Track"，后续循环处理空格/连字符。
                while (i < s.size() && s[i] == ' ') ++i;
            }
            int track = 0;
            size_t ts = i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') { track = track * 10 + (s[i] - '0'); ++i; }
            if (i > ts) {
                consumed = i;
                discOut = disc;
                return track;
            }
            consumed = 0;
            return -1;
        }
    }

    // "1-01"（碟-轨）
    size_t dash = 0;
    while (dash < s.size() && s[dash] >= '0' && s[dash] <= '9') ++dash;
    if (dash > 0 && s[dash] == '-' && dash + 1 < s.size() && s[dash + 1] >= '0' && s[dash + 1] <= '9') {
        int disc = 0;
        for (size_t k = 0; k < dash; ++k) disc = disc * 10 + (s[k] - '0');
        size_t i = dash + 1;
        int track = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') { track = track * 10 + (s[i] - '0'); ++i; }
        consumed = i;
        discOut = disc;
        return track;
    }

    // 单轨："01" / "01."
    consumed = 0;
    return parseLeadingTrack(s, consumed);
}

} // namespace MetadataInference