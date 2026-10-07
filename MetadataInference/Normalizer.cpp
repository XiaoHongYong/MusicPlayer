#include "Normalizer.h"
#include "Utils/Utils.h"

namespace MetadataNormalizer {

static bool isAsciiSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// 逐个字节输入，遇到下面两种宽字符时替换：
//   EF BC xx      -> U+FF01~U+FF3F（全角西文符号/数字/大写）
//   EF BD xy      -> U+FF40~U+FF5E（全角小写/其余）
//   E3 80 80      -> U+3000（全角空格）
// 其余字节原样输出。
std::string normalizeFullWidth(const std::string &s) {
    const int n = (int)s.size();
    std::string out;
    out.reserve(n);

    for (int i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];

        if (c == 0xE3 && (i + 2) < n
            && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) {
            out += ' ';            // U+3000 全角空格
            i += 2;
            continue;
        }

        if (c == 0xEF && (i + 2) < n) {
            unsigned char b1 = (unsigned char)s[i + 1];
            unsigned char b2 = (unsigned char)s[i + 2];
            if (b1 == 0xBC || b1 == 0xBD) {
                int cp = ((0x0F & c) << 12) | ((0x3F & b1) << 6) | (0x3F & b2);
                int half = cp - 0xFEE0;    // U+FF01..FF5E -> U+0021..U+007E
                if (half >= 0x21 && half <= 0x7E) {
                    out += (char)half;
                    i += 2;
                    continue;
                }
            }
        }

        out += (char)c;
    }

    return out;
}

std::string collapseSpaces(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    bool pendingSpace = false;

    for (unsigned char ch : s) {
        if (isAsciiSpace((char)ch)) {
            pendingSpace = true;
        } else {
            if (pendingSpace && !out.empty()) {
                out += ' ';
            }
            pendingSpace = false;
            out += (char)ch;
        }
    }
    return out;
}

std::string normalizeDisplay(const std::string &s) {
    return collapseSpaces(normalizeFullWidth(s));
}

std::string normalizeKey(const std::string &s) {
    return toLower(normalizeDisplay(s).c_str());
}

bool looksLikeGenericTitle(const std::string &key) {
    if (key.empty()) {
        return true;
    }
    // Track 01 / Track-01 / Trk1 / untitled / unknown / 未知
    if (key == "untitled" || key == "unknown" || key == "unknown artist" || key == "unknown title"
        || key == "unknown album" || key == "unknown genre" || key == "track" || key == "title"
        || key == "未知" || key == "未知歌曲" || key == "无名") {
        return true;
    }
    // "track 01" / "track1" / "trk1" / "track-01"
    auto allDigitsRest = [](const std::string &s) -> bool {
        if (s.empty()) {
            return false;
        }
        for (char ch : s) {
            if (ch < '0' || ch > '9') {
                return false;
            }
        }
        return true;
    };
    auto digitsAfter = [&](const char *prefix, int plen) -> bool {
        if ((int)key.size() <= plen) {
            return false;
        }
        if (key.compare(0, plen, prefix) != 0) {
            return false;
        }
        return allDigitsRest(key.substr(plen));
    };

    if (key.compare(0, 6, "track ") == 0) {
        return allDigitsRest(key.substr(6));
    }
    if (digitsAfter("track", 5) || digitsAfter("track-", 6) || digitsAfter("trk", 3)) {
        return true;
    }
    return false;
}

} // namespace MetadataNormalizer