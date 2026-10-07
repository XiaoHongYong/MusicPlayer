#pragma once

#include <string>
#include "Utils/Utils.h"

// 序号与年份解析（docs/web-console/metadata.md §10）。
namespace MetadataInference {

class NumberParser {
public:
    // 从字符串开头解析轨道号：可带前缀字母 A01/B01、后缀点/连字符/空格。
    // 返回轨道号；无法识别时返回 -1。*consumed 为已消费的字符数。
    int parseLeadingTrack(const std::string &s, size_t &consumed) const;

    // 识别 "01"（单轨）或 "1-01"（碟-轨）形式：返回轨道号，disc 交给 out（无则 -1）。
    int parseTrackDisc(const std::string &s, int &discOut, size_t &consumed) const;

    // 年份：仅当是 1900..今年+1 的有效年份才返回，否则 0。
    int parseYearStrict(cstr_t tokenKey) const;

    bool isYear(const std::string &key) const { return parseYearStrict(key.c_str()) != 0; }
    int currentYear() const;
};

} // namespace MetadataInference