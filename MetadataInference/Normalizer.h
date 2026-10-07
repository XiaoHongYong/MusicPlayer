#pragma once

#include <string>

// 字符串规范化（docs/web-console/metadata.md §8）。
// 统一的展示形：全角→半角、合并/去除多余空白、去首尾空白。
// 比较用 key：额外做 ASCII 小写。
namespace MetadataNormalizer {

// 把全角字母/数字/符号折成全角对应字符，全角空格(0x3000)折成普通空格。
// 当前实现覆盖面：0xFF01~0xFF5E（含全角连字符 －）、全角空格。完整 Unicode NFC 依赖 ICU，属后续。
std::string normalizeFullWidth(const std::string &s);

// 去首尾空白，并把内部连续空白合并成单个空格。
std::string collapseSpaces(const std::string &s);

// 展示用归一：UTF-8 文本的规范化形（normalizeFullWidth + 去空白）。
std::string normalizeDisplay(const std::string &s);

// 比较用：normalizeDisplay 后再做 ASCII 小写。
std::string normalizeKey(const std::string &s);

// 判断内容是否"通用/纯模板"标题，如 Track 01 / Unknown / untitled。
bool looksLikeGenericTitle(const std::string &key);

} // namespace MetadataNormalizer