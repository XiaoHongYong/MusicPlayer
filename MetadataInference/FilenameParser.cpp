#include "FilenameParser.h"
#include "Tokenizer.h"
#include "Normalizer.h"
#include "Utils/Utils.h"

namespace MetadataInference {

// 交 " - " 与单 '-' 拆分意图不同；这里统一把一段按 '-' 拆成原始分段（psplit）。
// 括号内的 '-'（如 "[WEB-DL]"）不拆分，保持为括号整体，便于后续整体判垃圾/版本。
static void splitSections(cstr_t text, VecStrings &out) {
    string cur;
    int bracketDepth = 0;
    for (cstr_t p = text; *p; ++p) {
        char c = *p;
        if (c == '(' || c == '[' || c == '{') {
            ++bracketDepth;
        } else if (c == ')' || c == ']' || c == '}') {
            if (bracketDepth > 0) {
                --bracketDepth;
            }
        }
        if (c == '-' && bracketDepth == 0) {
            trimStr(cur);
            if (!cur.empty()) {
                out.push_back(cur);
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    trimStr(cur);
    if (!cur.empty()) {
        out.push_back(cur);
    }
}

// 单个分段的清洗：分词 → 括号分类 / 垃圾过滤 / 年份 / 版本。
struct CleanedSegment {
    string text;       // 保留的文本
    string version;    // 本段抓到的版本词
    int year = 0;
    bool allDropped = true;
};

static CleanedSegment cleanSegment(const string &seg, const GarbageFilter &gf, const NumberParser &np) {
    CleanedSegment out;
    auto tokens = MetadataTokenizer::tokenize(seg);
    bool first = true;

    for (const auto &t : tokens) {
        bool dropped = false;
        if (t.bracket != MetadataTokenizer::BRACKET_NONE) {
            // 括号内容分类：先查语义/噪音/技术/垃圾/年份。
            if (np.isYear(t.key)) {
                out.year = np.parseYearStrict(t.key.c_str());
                dropped = true;
            } else if (gf.isSemanticVersion(t.key) || gf.matchTechnicalNumber(t.key)
                       || isBracketNoise(t.key) || gf.isHard(t.key) || gf.isSoft(t.key)) {
                if (gf.isSemanticVersion(t.key)) {
                    out.version = t.text;
                }
                dropped = true;
            }
            // 其余（如 "Official MV" / 未知内容）保守保留，避免丢标题信息。
        } else {
            if (np.isYear(t.key)) {
                out.year = np.parseYearStrict(t.key.c_str());
                dropped = true;
            } else if (gf.matchTechnicalNumber(t.key) || gf.isHard(t.key) || gf.isSoft(t.key)) {
                dropped = true;
            } else if (gf.isSemanticVersion(t.key)) {
                out.version = t.text;
                dropped = true;
            }
        }

        if (!dropped) {
            if (!first) {
                out.text += ' ';
            }
            out.text += t.text;
            first = false;
            out.allDropped = false;
        }
    }

    return out;
}

FilenameParseResult FilenameParser::parse(const std::string &fileNameNoExt) const {
    FilenameParseResult result;
    string base = fileNameNoExt;

    // 取 basename（去掉路径）
    auto slash = base.find_last_of(PATH_SEP_CHAR);
    if (slash != string::npos) {
        base = base.substr(slash + 1);
    }
    base = MetadataNormalizer::normalizeDisplay(base);
    if (base.empty()) {
        return result;
    }

    // 先解析前导轨道/碟片前缀（"1-01" / "Disc 1 Track 2" / "01 -" / "01."），
    // 再按 '-' 分段。若不先剥离，"1-01" 内部连字符被分段拆开，碟-轨关联丢失。
    {
        int leadDisc = -1;
        size_t leadConsumed = 0;
        int leadTrack = _np.parseTrackDisc(base.c_str(), leadDisc, leadConsumed);
        // 碟-轨形式（leadDisc>0）一定接受；纯轨道号限制为合理值，避免把前导年份当轨道。
        if (leadTrack > 0 && (leadDisc > 0 || leadTrack <= 127)) {
            result.track = leadTrack;
            result.disc = leadDisc;
            base = base.substr(leadConsumed);
            while (!base.empty()
                   && (base[0] == ' ' || base[0] == '\t' || base[0] == '-' || base[0] == '.')) {
                base.erase(0, 1);
            }
        }
    }

    VecStrings rawSections;
    splitSections(base.c_str(), rawSections);
    if (rawSections.empty()) {
        return result;
    }

    result.hasSeparator = rawSections.size() >= 2;

    std::vector<CleanedSegment> cleaned;
    cleaned.reserve(rawSections.size());

    for (size_t i = 0; i < rawSections.size(); ++i) {
        CleanedSegment c = cleanSegment(rawSections[i], _gf, _np);

        if (!c.version.empty()) {
            if (!result.version.empty()) {
                result.version += ' ';
            }
            result.version += c.version;
        }
        if (c.year) {
            result.year = c.year;
        }
        if (!c.text.empty()) {
            cleaned.push_back(c);
        }
    }

    // version 去重（多段可能重复同一词，如 Live/Live）
    {
        VecStrings vs;
        splitSections(result.version.c_str(), vs);
        string joined;
        for (auto &v : vs) {
            if (!joined.empty()) {
                joined += ' ';
            }
            joined += v;
        }
        result.version = joined;
    }

    // 组装 artist / album / title
    if (cleaned.empty()) {
        return result;
    }
    if (cleaned.size() == 1) {
        result.title = cleaned[0].text;
    } else if (cleaned.size() == 2) {
        result.artist = cleaned[0].text;
        result.title = cleaned[1].text;
    } else {
        result.artist = cleaned[0].text;
        result.album = cleaned[1].text;
        result.title = cleaned.back().text;
    }

    return result;
}

} // namespace MetadataInference