#pragma once

#include <string>
#include <vector>

// 分词（docs/web-console/metadata.md §6）：把目录名/文件名按分隔符拆成词元。
// 词元记录它原本是否被成对括号/方括号/花括号包裹，供后续括号分类。
namespace MetadataTokenizer {

enum BracketType {
    BRACKET_NONE = 0,
    BRACKET_ROUND,    // ( )
    BRACKET_SQUARE,   // [ ]
    BRACKET_CURLY,    // { }
    BRACKET_FULLWIDTH // 【 】
};

struct Token {
    std::string text;       // 保留大小写的原始文本（已去除空白）
    std::string key;        // normalizeKey 归一（小写、全角已折半角）
    BracketType bracket = BRACKET_NONE;
    bool delimited = false; // 前后是否有分隔符（影响 "Artist - Title" 断句）
};

// 分隔符集合（含全角 －．＿ 等，Normalizer 已折半角，所以这里按半角处理即可）。
inline bool isSeparator(char c) {
    switch (c) {
        case '-': case '_': case '.': case '(': case ')':
        case '[': case ']': case '{': case '}': case ';':
        case ',': case '\'': case '"':
            return true;
        default:
            return false;
    }
}

std::vector<Token> tokenize(const std::string &text);

} // namespace MetadataTokenizer