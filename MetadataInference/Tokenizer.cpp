#include "Tokenizer.h"
#include "Normalizer.h"

namespace MetadataTokenizer {

static BracketType bracketOf(char c) {
    switch (c) {
        case '(': case ')':   return BRACKET_ROUND;
        case '[': case ']':   return BRACKET_SQUARE;
        case '{': case '}':   return BRACKET_CURLY;
        default:              return BRACKET_NONE;
    }
}

std::vector<Token> tokenize(const std::string &text) {
    std::vector<Token> out;
    BracketType activeBracket = BRACKET_NONE;
    std::string cur;
    bool prevWasSeparator = true;

    auto flush = [&]() {
        while (!cur.empty() && cur.back() == ' ') {
            cur.pop_back();
        }
        while (!cur.empty() && cur.front() == ' ') {
            cur.erase(0, 1);
        }
        if (cur.empty()) {
            return;
        }
        Token t;
        t.text = cur;
        t.key = MetadataNormalizer::normalizeKey(cur);
        t.bracket = activeBracket;
        t.delimited = prevWasSeparator;
        out.push_back(t);
        cur.clear();
    };

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '(' || c == '[' || c == '{') {
            flush();
            activeBracket = bracketOf(c);
            prevWasSeparator = true;
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (activeBracket == bracketOf(c)) {
                flush();
                activeBracket = BRACKET_NONE;
                prevWasSeparator = true;
                continue;
            }
        }
        if (c == ' ' || c == '\t') {
            // 词内空白：合并为单个空格，不进首尾。
            if (!cur.empty() && cur.back() != ' ') {
                cur += ' ';
            }
            prevWasSeparator = false;
            continue;
        }
        if (activeBracket == BRACKET_NONE && isSeparator(c)) {
            // 括号外分隔符分断词元；括号内（如 "WEB-DL"、"1999-2000"）保持为一个词元。
            flush();
            prevWasSeparator = true;
            continue;
        }
        cur += c;
        prevWasSeparator = false;
    }
    flush();

    return out;
}

} // namespace MetadataTokenizer