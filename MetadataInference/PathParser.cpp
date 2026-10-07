#include "PathParser.h"
#include "Normalizer.h"
#include "Tokenizer.h"
#include "GarbageFilter.h"
#include "Utils/Utils.h"

namespace MetadataInference {

// 目录里常见的分类/层形容词，跳过（作为 Genre/根层级，不算 Artist/Album）。
bool looksLikeGenreDir(const std::string &key) {
    static const std::vector<std::string> SKIP = {
        "music", "songs", "song", "downloads", "download", "库", "音乐", "我的音乐",
        "华语", "流行", "欧美", "古典", "摇滚", "民谣", "电子", "爵士", "英文", "中文",
        "chinese", "mandarin", "pop", "rock", "jazz", "classical", "electronic",
        "best of", "合集", "精选集", "ost", "soundtrack", "single", "单曲"
    };
    for (auto &s : SKIP) {
        if (key == s) {
            return true;
        }
    }
    return false;
}

static bool isSkippableDir(const std::string &key) { return looksLikeGenreDir(key); }

PathParseResult PathParser::parse(cstr_t filePath) const {
    PathParseResult result;
    string dir = fileGetPath(filePath);
    if (dir.empty()) {
        return result;
    }

    // 拆成目录分量（去掉末尾分隔符与空段）。
    VecStrings comps;
    string cur;
    for (cstr_t p = dir.c_str(); *p; ++p) {
        if (*p == PATH_SEP_CHAR) {
            if (!cur.empty()) {
                comps.push_back(cur);
            }
            cur.clear();
        } else {
            cur += *p;
        }
    }
    if (!cur.empty()) {
        comps.push_back(cur);
    }

    // 从最近的目录往上扫描：跳过年份/序号目录与 genre 层。
    VecStrings meaningful;   // 从近到远
    std::vector<int> years;  // 从近到远
    for (int i = (int)comps.size() - 1; i >= 0; --i) {
        string key = MetadataNormalizer::normalizeKey(comps[i]);
        if (key.empty()) {
            continue;
        }
        int y = _np.parseYearStrict(key.c_str());
        if (y) {
            years.push_back(y);
            continue;   // 纯年份目录（如 "2004" 或 "2004" 单独一层），不是专辑/歌手
        }
        // "2004 七里香" 混合目录：取其中的年份，再当作专辑候选。
        if (isSkippableDir(key)) {
            continue;
        }
        meaningful.push_back(comps[i]);
    }

    if (!years.empty()) {
        result.year = years[0];
    }
    if (meaningful.empty()) {
        return result;
    }

    result.hasDirectory = true;
    // 最近目录 = 专辑候选；上一层 = 歌手候选。
    result.album = MetadataNormalizer::normalizeDisplay(meaningful[0]);
    if (meaningful.size() >= 2) {
        result.artist = MetadataNormalizer::normalizeDisplay(meaningful[1]);
    }
    return result;
}

} // namespace MetadataInference