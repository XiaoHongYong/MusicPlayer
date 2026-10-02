/********************************************************************
    Created  :    2002/01/03    17:23:02
    FileName :    LrcParser.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include <algorithm>
#include "LrcParser.h"
#include "LrcParserHelper.h"
#include "LyrTimestamps.h"
#include "../TinyJS/utils/StringParser.hpp"


#define SZ_ENCODING            "encoding:"
extern class CProfile g_profile;

CharEncodingType g_defaultLyricsEncoding = ED_SYSDEF;

void setDefaultLyricsEncoding(CharEncodingType encoding) {
    g_defaultLyricsEncoding = encoding;
}

CharEncodingType getDefaultLyricsEncoding() {
    return g_defaultLyricsEncoding;
}

/**
 * 将歌词数据转换为 UTF-8 的编码
 */
string convertBinLyricsToUtf8(const StringView &data, bool useSpecifiedEncoding, CharEncodingType encoding) {
    if (useSpecifiedEncoding) {
        // 使用指定编码
        string str;
        if (mbcsToUtf8(data.data, data.len, str, encoding) == 0) {
            return data.toString();
        }
        return str;
    }

    int bomSize = 0;
    detectFileEncoding(data.data, data.len, bomSize);
    if (bomSize > 0) {
        return strToUtf8ByBom(data.data, data.len);
    }

    // 没有 BOM，搜索是否有 Encoding: tag
    static StringView LRC_ENCODING("\n[encoding:");
    static StringView TXT_ENCODING("\nencoding:");

    StringView strEncoding;
    int pos = data.stristr(LRC_ENCODING), end = -1;
    if (pos != -1) {
        int end1 = data.strchr(']', pos);
        int end2 = data.strchr('\n', pos + 1);
        if (end1 != -1 && end1 < end2) {
            // 结束的 ']' 必须在此行内
            strEncoding.data = data.data + pos + LRC_ENCODING.len;
            strEncoding.len = (uint32_t)(end1 - pos + LRC_ENCODING.len);
            end = end1 + 1;
        }
    } else {
        pos = data.stristr(TXT_ENCODING);
        if (pos != -1) {
            end = data.strchr('\n', pos);
            if (end != -1) {
                strEncoding.data = data.data + pos + TXT_ENCODING.len;
                strEncoding.len = (uint32_t)(end - pos + TXT_ENCODING.len);
            }
        }
    }

    if (pos != -1 && end != -1) {
        // 使用歌词中指定的 encoding 转码
        encoding = getCharEncodingID(strEncoding.trim().toString().c_str());

        // 去掉上一个和 \n 相连接的 \r
        if (pos > 0 && data.data[pos - 1] == '\r') {
            pos--;
        }

        // 转换编码时去掉 Encoding: xxx 这部分
        string p1, p2;
        mbcsToUtf8(data.data, pos, p1, encoding);
        mbcsToUtf8(data.data + end, data.len - end, p2, encoding);
        return p1 + p2;
    } else {
        // 使用缺省编码
        if (g_defaultLyricsEncoding != ED_SYSDEF) {
            string str;
            if (mbcsToUtf8(data.data, data.len, str, g_defaultLyricsEncoding) == 0) {
                return data.toString();
            }
            return str;
        } else {
            return data.toString();
        }
    }
}

bool readLyricsFileByEncoding(cstr_t fileName, string &contentOut) {
    if (!readFile(fileName, contentOut)) {
        return false;
    }

    contentOut = convertBinLyricsToUtf8(contentOut, false, ED_SYSDEF);

    return true;
}

LyrTagParser::LyrTagParser(LyricsProperties &props) : _props(props) {
    _items = {
        { SZ_LRC_AR,        SZ_TXT_AR,          &props.artist,      },
        { SZ_LRC_TI,        SZ_TXT_TI,          &props.title,       },
        { SZ_LRC_AL,        SZ_TXT_AL,          &props.album,       },
        { SZ_LRC_BY,        "",                 &props.by,          },
        { SZ_LRC_OFFSET,    SZ_LRC_OFFSET,      &_offsetTime,       },
        { SZ_LRC_LENGTH,    "",                 &props.mediaLength, },
        { SZ_LRC_ID,        SZ_TXT_ID,          &props.id,          },
        { SZ_ENCODING,      SZ_ENCODING,        &_unused,           },
    };
}

void LyrTagParser::parseTxtAllTags(const StringView &content) {
    VecStringViews lines;

    content.splitLines(lines);
    for (auto &line : lines) {
        parseTxtTag(line);
    }

    _props.lyrContentType = LCT_TXT;
}

void LyrTagParser::parseLrcAllTags(const StringView &content) {
    VecStringViews lines;

    content.splitLines(lines);
    for (auto &line : lines) {
        parseLrcTag(line);
    }
}

bool LyrTagParser::parseTxtTag(const StringView &line) {
    for (auto &item : _items) {
        if (!item.txtName.empty() && line.iStartsWith(item.txtName)) {
            setValue(item, line.substr(item.txtName.len));
            return true;
        }
    }

    return false;
}

bool LyrTagParser::parseLrcTag(const StringView &line) {
    StringParser parser(line);
    StringView value;

    try {
        parser.ignoreSpaces();
        parser.expect('[');
        value = parser.readTill(']').trim();
    } catch (std::exception &e) {
        return false;
    }

    for (auto &item : _items) {
        if (value.iStartsWith(item.lrcName)) {
            setValue(item, value.substr(item.lrcName.len));
            return true;
        }
    }

    return false;
}

bool LyrTagParser::isTxtTag(const StringView &line) {
    StringParser parser(line.trim());

    try {
        for (auto &item : _items) {
            if (parser.isI(item.lrcName)) {
                return true;
            }
        }
    } catch (std::exception &e) {
    }

    return false;
}

bool LyrTagParser::isLrcTag(const StringView &line) {
    StringParser parser(line.trim());

    try {
        parser.expect('[');
        parser.ignoreSpaces();
        for (auto &item : _items) {
            if (parser.isI(item.lrcName)) {
                parser.readTill(']');
                return true;
            }
        }
    } catch (std::exception &e) {
    }

    return false;
}

void LyrTagParser::toTxtTags(string &headTagsOut, string &tailTagsOut, RawLyrics *rawLyrics) {
    headTagsOut.reserve(256);

    if (rawLyrics && rawLyrics->properties().lyrContentType >= LCT_LRC) {
        _offsetTime = LyrTimestamps::toString(*rawLyrics);
    }

    for (auto &item : _items) {
        auto &name = item.txtName;
        assert(item.value);
        if (!name.empty() && !item.value->empty()) {
            string *output = nullptr;
            if ((item.value == &_props.id || item.value == &_offsetTime)) {
                // 只有 ID, offset 在尾部
                output = &tailTagsOut;
            } else if (item.value == &_unused) {
                continue;
            } else {
                output = &headTagsOut;
            }

            output->append((char *)name.data, name.len);
            output->append(" ");
            output->append(*item.value);
            output->append(SZ_NEW_LINE);
        }
    }
}

string LyrTagParser::toLrcTags() {
    string text;
    text.reserve(256);

    if (_props.getOffsetTime() != 0) {
        _offsetTime = _props.getOffsetTimeStr();
    }

    for (auto &item : _items) {
        auto &name = item.lrcName;
        assert(item.value);
        if (!name.empty() && !item.value->empty() && item.value != &_unused) {
            text.append(1, '[');
            text.append((char *)name.data, name.len);
            text.append(" ");
            text.append(*item.value);
            text.append("]" SZ_NEW_LINE);
        }
    }

    return text;
}

string LyrTagParser::joinTagsLyrics(const StringView &lyrics, bool isLrcFormat) {
    string headTags, tailTags;

    if (isLrcFormat) {
        headTags = toLrcTags();
    } else {
        toTxtTags(headTags, tailTags);
    }

    string ret;
    if (!headTags.empty()) {
        ret = headTags;
    }

    ret.append(lyrics.data, lyrics.len);

    if (!tailTags.empty()) {
        if (!lyrics.endsWith("\n")) {
            ret.append(SZ_NEW_LINE);
        }
        ret.append(tailTags);
    }

    return ret;
}

void LyrTagParser::setValue(Item &item, StringView value) {
    value = value.trim();
    if (item.value == &_offsetTime) {
        // Must be offset
        _props.setOffsetTime(value.toString().c_str());
    } else if (item.value->empty()) {
        // 只取第一个值，忽略后续的值
        item.value->assign((cstr_t)value.data, value.len);
    }
}

bool LyrTagParser::replaceInFile(cstr_t fileName, bool isLrcFormat) {
    string content;
    if (!readLyricsFileByEncoding(fileName, content)) {
        return false;
    }

    VecStringViews lines, newLines;
    StringView(content).splitLines(lines);

    for (auto &line : lines) {
        if (isLrcFormat && isLrcTag(line)) {
            // Ignore lrc tag
        } else if (!isLrcFormat && isTxtTag(line)) {
            // Ignore txt tag
        } else {
            newLines.push_back(line);
        }
    }

    content = strJoin(newLines.begin(), newLines.end(), SZ_NEW_LINE);
    content = joinTagsLyrics(content, isLrcFormat);

    autoInsertWithUtf8Bom(content);

    return writeFile(fileName, content);
}

bool readIntValue(cstr_t &szBeg, cstr_t szEnd, int nSize, int &value) {
    cstr_t p = szBeg;
    value = 0;
    while (isDigit(*p) && nSize > 0 && p < szEnd) {
        value *= 10;
        value += *p - '0';
        p++;
        nSize--;
    }

    if (nSize == 0) {
        szBeg = p;
        return true;
    }

    return false;
}

// [mm:ss.xx]word 1<mm:ss.xx> word 2 <mm:ss.xx> ...
cstr_t eatFrag(cstr_t szLyricsBeg, cstr_t szLyricsEnd, int &endTime, string &buffLyrics) {
    cstr_t szPos;

    buffLyrics.clear();
    szPos = szLyricsBeg;
    while (szPos < szLyricsEnd) {
        if (*szPos == '<') {
            int n;
            cstr_t szTemp = szPos + 1;
            endTime = 0;
            if (readIntValue(szTemp, szLyricsEnd, 2, n) && *szTemp == ':') {
                szTemp++;
                endTime = n * 60 * 1000;
                if (readIntValue(szTemp, szLyricsEnd, 2, n) && *szTemp == '.') {
                    szTemp++;
                    endTime += n * 1000;
                    if (readIntValue(szTemp, szLyricsEnd, 3, n) && *szTemp == '>') {
                        szTemp++;
                        endTime += n;
                        buffLyrics.append(szLyricsBeg, int(szPos - szLyricsBeg));
                        return szTemp;
                    } else if (readIntValue(szTemp, szLyricsEnd, 2, n) && *szTemp == '>') {
                        szTemp++;
                        endTime += n * 10;
                        buffLyrics.append(szLyricsBeg, int(szPos - szLyricsBeg));
                        return szTemp;
                    }
                }
            }
        }
        szPos++;
    }

    buffLyrics.append(szLyricsBeg, int(szLyricsEnd - szLyricsBeg));
    return szLyricsEnd;
}

class _LrcParser {
public:
    _LrcParser(RawLyrics &rawLyrics) : _rawLyrics(rawLyrics) { }

    int parse(const StringView &lyrics, bool isAddTimeStampForTextLine) {
        _rawLyrics.clear();

        if (lyrics.empty()) {
            return ERR_EMPTY_LYRICS;
        }

        LyrTagParser tagParser(_rawLyrics.properties());

        VecStringViews lines;
        lyrics.trim().splitLines(lines);

        int continuiousBlankLines = 0, countBlankLines = 0, countNonBlankLines = 0;
        for (auto line : lines) {
            line = line.trimStart(stringViewBlanks);

            // 忽略额外的空行
            if (line.empty()) {
                continuiousBlankLines++;
                if (continuiousBlankLines > 2 ||
                    (continuiousBlankLines > 1 && countBlankLines >= countNonBlankLines)) {
                    // 最多只空两行
                    // 或者只空一行，如果空白行数量 >= 歌词行
                    continue;
                }
                countBlankLines++;
            } else {
                continuiousBlankLines = 0;
                countNonBlankLines++;
            }

            if (line.len > 0 && line.data[0] == '[') {
                int nNextPos = 0, nTime = 0;
                vector<int> vTimes;
                while (nNextPos < line.len && parseLrcTimeTag(line.data, nNextPos, nTime)) {
                    vTimes.push_back(nTime);
                }

                if (!vTimes.empty()) {
                    // Time tag.
                    addLyrLines(vTimes, line.substr(nNextPos));
                    continue;
                } else if (tagParser.parseLrcTag(line)) {
                    continue;
                }
            }

            // NOT lrc tag or time stamps.
            if (isAddTimeStampForTextLine) {
                if (tagParser.parseTxtTag(line)) {
                    continue;
                }

                _rawLyrics.addLyricsLine(line);
            } else {
                _rawLyrics.addNoneLyricsLine(line);
            }
        }

        _rawLyrics.analyzeContentType();
        if (_rawLyrics.properties().lyrContentType == LCT_UNKNOWN && !isAddTimeStampForTextLine) {
            // parse as text lyrics
            parse(lyrics, true);
            _rawLyrics.properties().lyrContentType = LCT_TXT;

            if (LyrTimestamps::isTimeStamps(_rawLyrics.properties().getOffsetTimeStr().c_str())) {
                LyrTimestamps::parse(_rawLyrics.properties().getOffsetTimeStr().c_str(), _rawLyrics);
            }
        }

        return ERR_OK;
    }

protected:
    //
    // add lyrics lyrics line
    // [mm:ss.xx]word 1<mm:ss.xx> word 2 <mm:ss.xx> ...
    void addLyrLines(vector<int> &vTimes, const StringView &text) {
        string buff;

        std::sort(vTimes.begin(), vTimes.end());

        for (auto beginTime : vTimes) {
            LyricsLine line(beginTime, -1);

            cstr_t textBeg = text.data, textEnd = text.data + text.len;
            do {
                int endTime = -1;
                textBeg = eatFrag(textBeg, textEnd, endTime, buff);

                line.appendPiece(beginTime, endTime, buff);

                if (endTime != -1) {
                    beginTime = endTime;
                }
            }
            while (textBeg < textEnd);

            _rawLyrics.push_back(line);
        }
    }

protected:
    RawLyrics            &_rawLyrics;

};

int parseLyricsBinary(const StringView &lyrics, bool useSpecifiedEncoding,
    CharEncodingType encoding, RawLyrics &rawLyricsOut) {
    auto text = convertBinLyricsToUtf8(lyrics, useSpecifiedEncoding, encoding);
    return parseLyricsString(text, false, rawLyricsOut);
}

int parseLyricsString(const StringView &lyrics, bool isAddTimeStampForTextLine, RawLyrics &lyrLinesOut) {
    _LrcParser parser(lyrLinesOut);
    return parser.parse(lyrics, isAddTimeStampForTextLine);
}

string formatLrcTimeTag(int time, bool isLongTimeFmt) {
    int min, sec, ms;
    if (isLongTimeFmt) {
        time += 10 / 2;
    } else {
        time += 1000 / 2;
    }

    ms = (time / 10) % 100;
    sec = (time / 1000) % 60;
    min = time / (1000 * 60);

    if (isLongTimeFmt) {
        return stringPrintf("[%02d:%02d.%02d]", min, sec, ms).c_str();
    } else {
        return stringPrintf("[%02d:%02d]", min, sec).c_str();
    }
}

bool lyricsLineToLrcString(const LyricsLine &line, string &strBuff) {
    strBuff.clear();

    if (line.isTempLine) {
        return false;
    }

    if (!line.isLyricsLine) {
        strBuff = line.content;
        return true;
    }

    if (!line.pieces.empty()) {
        strBuff = formatLrcTimeTag(line.beginTime, true);
    }

    char szTime[64];
    for (int i = 0; i < (int)line.pieces.size(); i++) {
        strBuff += line.pieces[i].text;

        if (i + 1 < (int)line.pieces.size()) {
            int nMin, nSec, nMs;
            int nTime = line.pieces[i + 1].beginTime + 10 / 2;

            nMs = (nTime / 10) % 100;
            nSec = (nTime / 1000) % 60;
            nMin = nTime / (1000 * 60);
            snprintf(szTime, CountOf(szTime), "<%02d:%02d.%02d>", nMin, nSec, nMs);
            strBuff += szTime;
        }
    }

    return !strBuff.empty();
}

void lyricsLineToTxtString(const LyricsLine &line, string &strBuff) {
    assert(line.isLyricsLine);
    strBuff.clear();

    if (line.isTempLine) {
        return;
    }

    strBuff = line.joinPiecesText();
}

string toLrcString(const RawLyrics &lyrLines, bool isIncTags) {
    // reserver 4k spaces for lyrics data.
    string str;
    str.reserve(1024 * 4);

    if (isIncTags) {
        LyrTagParser tagParser(const_cast<RawLyrics&>(lyrLines).properties());

        str.append(tagParser.toLrcTags());
    }

    // 添加时间用于排序
    auto lines = lyrLines.lines();
    int timeLatest = 0x7ffffff;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        auto &line = *it;
        if (line.pieces.empty()) {
            line.beginTime = timeLatest;
        } else {
            line.beginTime = timeLatest = line.pieces.front().beginTime;
        }
    }

    // 按照时间顺序排序
    std::stable_sort(lines.begin(), lines.end(), [](const LyricsLine &a, const LyricsLine &b) {
        return a.beginTime < b.beginTime;
    });

    // save every lyrics line
    string strBuff;
    for (auto &line : lines) {
        if (lyricsLineToLrcString(line, strBuff)) {
            str.append(strBuff);
            str.append(SZ_NEW_LINE);
        }
    }

    if (str.size() > 2 && str[str.size() - 1] == '\n') {
        str.resize(str.size() - strlen(SZ_NEW_LINE));
    }

    return str;
}

string toTxtString(const RawLyrics &lyrLines, bool isIncTags) {
    // reserver 4k spaces for lyrics data.
    string str;
    str.reserve(1024 * 4);

    //
    // Convert and remove lyrics line which is text lyrics tag.
    //
    bool isRemoveExtraBlankLines = g_profile.getBool("RemoveExtraBlankLines", true);
    bool isLastLineBlank = false;

    for (auto &line : lyrLines) {
        if (line.isLyricsLine) {
            isLastLineBlank = false;
            if (!line.isTempLine) {
                for (auto &piece : line.pieces) {
                    str.append(piece.text);
                }

                str.append(SZ_NEW_LINE);
            }
        } else {
            // Don't write extra blank lines.
            if (isRemoveExtraBlankLines && isLastLineBlank) {
                continue;
            }

            // Only append blank spaces, not "line.content";
            str += SZ_NEW_LINE;
            isLastLineBlank = true;
        }
    }

    if (isIncTags) {
        LyrTagParser tagParser(const_cast<RawLyrics&>(lyrLines).properties());
        string headTags, tailTags;

        tagParser.toTxtTags(headTags, tailTags);

        if (!headTags.empty()) {
            str.insert(0, headTags);
        }

        if (!tailTags.empty()) {
            if (str.size() && str[str.size() - 1] != '\n') {
                str += SZ_NEW_LINE;
            }
            str += tailTags;
        }
    }

    if (str.size() > 2 && str[str.size() - 2] == '\r' && str[str.size() - 1] == '\n') {
        str.resize(str.size() - 2);
    }

    return str;
}

string toLyricsBinary(const RawLyrics &rawLyrics) {
    auto lyrics = toLyricsString(rawLyrics, true, rawLyrics.contentType() <= LCT_TXT);

    autoInsertWithUtf8Bom(lyrics);

    return lyrics;
}

string toLyricsString(const RawLyrics &lyrLines, bool isIncTags, bool isToTxtFormat) {
    if (isToTxtFormat || lyrLines.properties().lyrContentType == LCT_TXT) {
        return toTxtString(lyrLines, isIncTags);
    } else {
        return toLrcString(lyrLines, isIncTags);
    }
}

string toLyricsString(const RawLyrics &rawLyrics) {
    return toLyricsString(rawLyrics, true, rawLyrics.contentType() <= LCT_TXT);
}

bool compressLyrics(string &lyrics) {
    VecStringViews textLines;
    StringView(lyrics).splitLines(textLines);

    struct Line {
        VecInts     timestamps;
        StringView  text;
    };

    AllocatorPool pool;
    vector<Line *> lines;
    std::map<StringView, Line *, SizedStrCmpLess> mapLines;
    LyricsProperties props;
    LyrTagParser tagParser(props);
    bool isCompressed = false;
    int emptyLines = 0; // 连续空行数量

    for (auto textOrg : textLines) {
        auto text = textOrg.trim();
        if (tagParser.parseLrcTag(text)) {
            continue;
        }

        Line *line = PoolNew(pool, Line);
        int nextPos = 0, timestamp = 0;
        while (parseLrcTimeTag(text.data, nextPos, timestamp)) {
            line->timestamps.push_back(timestamp);
        }

        if (nextPos > 0) {
            // LRC lyrics line
            line->text = text.substr(nextPos);
            auto it = mapLines.find(line->text);
            if (it == mapLines.end()) {
                // No existing lyrics line
                mapLines[line->text] = line;
                lines.push_back(line);
                emptyLines = 0;
            } else {
                // Found existing lyrics line
                auto &oldTimestamps = (*it).second->timestamps;
                oldTimestamps.insert(oldTimestamps.begin(), line->timestamps.begin(), line->timestamps.end());
                isCompressed = true;
            }
        } else {
            // text
            line->text = textOrg;
            if (text.len == 0) {
                if (emptyLines >= 1) {
                    // 只保留一行空行
                    continue;
                } else {
                    emptyLines++;
                }
            } else {
                emptyLines = 0;
            }
            lines.push_back(line);
        }
    }

    if (!isCompressed) {
        return false;
    }

    string out;

    out.reserve(lyrics.size() + 50);
    out.append(tagParser.toLrcTags());

    for (auto line : lines) {
        std::sort(line->timestamps.begin(), line->timestamps.end(), std::greater<int>());
        for (auto t : line->timestamps) {
            out.append(formatLrcTimeTag(t, t % 1000 != 0));
        }
        out.append(line->text.data, line->text.len);
        out.push_back('\n');
    }

    out.resize(out.size() - 1);
    lyrics.assign(out);

    return true;
}

#if UNIT_TEST

#include "utils/unittest.h"

TEST(LrcParser, CLrcParser_Lrc) {
    cstr_t SZ_LYRICS_TEST = "[ar:artist_name]\n[ti:title_name]\n[encoding:gb2312]\n[offset:-500]\n[00:01.12]L1中\n[00:02.01]\nDummy Line\r\n[00:03:01]\n[00:04:02]L3\nL4\n\n";
    cstr_t valArtist = "artist_name";
    cstr_t valTitle = "title_name";
    cstr_t vLyrLines[] = { "L1中", "", "", "L3" };
    int vTime[] = { 1 * 1000 + 120, 2 * 1000 + 10, 3 *1000 + 10, 4 *1000 + 20 };

    string gbStr;
    utf8ToMbcs(SZ_LYRICS_TEST, -1, gbStr);

    utf16string wStr;
    utf8ToUCS2(SZ_LYRICS_TEST, -1, wStr);

    utf16string wLyrics;
    wLyrics.append((const WCHAR *)SZ_FE_UCS2, 1);
    wLyrics.append(wStr.c_str(), wStr.size());

    utf16string wLyricsBe;
    wLyricsBe.append(wLyrics.c_str(), wLyrics.size());
    ucs2EncodingReverse((utf16_t *)wLyricsBe.data(), (int)wLyricsBe.size());

    string utf8Lyrics;
    utf8Lyrics.append(SZ_FE_UTF8);
    utf8Lyrics.append(SZ_LYRICS_TEST);

    const StringView vLyrics[] = {
        gbStr,
        StringView((uint8_t *)wLyrics.c_str(), wLyrics.size() * sizeof(utf16_t)),
        StringView((uint8_t *)wLyricsBe.c_str(), wLyricsBe.size() * sizeof(utf16_t)),
        utf8Lyrics,
    };

    for (int i = 0; i < CountOf(vLyrics); i++) {
        RawLyrics rawLyrics;

        int ret = parseLyricsBinary(vLyrics[i], false, ED_SYSDEF, rawLyrics);
        ASSERT_TRUE(ret == ERR_OK);

        auto &props = rawLyrics.properties();
        ASSERT_EQ(props.lyrContentType, LCT_LRC);

        ASSERT_EQ(props.artist, valArtist);
        ASSERT_EQ(props.title, valTitle);
        ASSERT_EQ(props.getOffsetTime(), -500);

        LyricsLines lyrLines = rawLyrics.toLyricsLinesOnly();
        ASSERT_EQ(lyrLines.size(), CountOf(vLyrLines));
        for (int k = 0; k < lyrLines.size(); k++) {
            LyricsLine &line = lyrLines[k];

            printf("Round: %d, Line: %d\n", i, k);

            ASSERT_TRUE(line.pieces.size() == 1);
            ASSERT_TRUE(k < CountOf(vLyrLines));
            ASSERT_EQ(line.pieces[0].text, vLyrLines[k]);
            ASSERT_EQ(line.pieces[0].beginTime, vTime[k]);
            ASSERT_EQ(line.beginTime, vTime[k]);
        }

        //
        // Cases on CLrcParser::toString
        //
        string strNewLyr;
        strNewLyr = toLyricsString(rawLyrics, true, false);
        printf("Lrc: %s\n", strNewLyr.c_str());

        RawLyrics rawLyrics3;
        ret = parseLyricsString(strNewLyr, false, rawLyrics3);
        ASSERT_EQ(ret, ERR_OK);

        {
            // Compare toString data.
            auto &props3 = rawLyrics3.properties();
            ASSERT_EQ(props3.artist, props.artist);
            ASSERT_EQ(props3.title, props.title);
            ASSERT_EQ(props3.getOffsetTime(), props.getOffsetTime());

            LyricsLines lyrLines3 = rawLyrics3.toLyricsLinesOnly();
            ASSERT_EQ(lyrLines3.size(), CountOf(vLyrLines));
            for (int k = 0; k < lyrLines3.size(); k++) {
                LyricsLine &line = lyrLines3[k];

                ASSERT_TRUE(line.pieces.size() == 1);
                ASSERT_TRUE(k < CountOf(vLyrLines));
                ASSERT_EQ(line.pieces[0].text, vLyrLines[k]);
                ASSERT_TRUE(line.pieces[0].beginTime == vTime[k]);
            }
        }

        //
        // Case: Convert to text including timestamps
        //
        string strTxtLyr;
        strTxtLyr = toLyricsString(rawLyrics, true, true);
        printf("Txt: %s\n", strTxtLyr.c_str());

        RawLyrics rawLyrics5;
        ret = parseLyricsString(strTxtLyr, false, rawLyrics5);
        ASSERT_EQ(ret, ERR_OK);

        auto &props5 = rawLyrics5.properties();
        ASSERT_EQ(props5.artist, props.artist);
        ASSERT_EQ(props5.title, props.title);
        ASSERT_EQ(props5.getOffsetTime(), 0);

        // Compare line by line
        LyricsLines lyrLines5 = rawLyrics5.toLyricsLinesOnly();
        vector<string> vTxtLyrLines = { "artist_name - title_name" };
        vTxtLyrLines.insert(vTxtLyrLines.end(), vLyrLines, vLyrLines + CountOf(vLyrLines));
        ASSERT_EQ(lyrLines5.size(), vTxtLyrLines.size());
        for (int k = 0; k < lyrLines5.size(); k++) {
            LyricsLine &line = lyrLines5[k];

            ASSERT_TRUE(line.pieces.size() == 1);
            ASSERT_EQ(line.pieces[0].text, vTxtLyrLines[k]);
            ASSERT_TRUE(line.pieces[0].beginTime >= 0);
            ASSERT_TRUE(line.pieces[0].endTime >= 0);
            ASSERT_TRUE(line.beginTime >= 0);
            ASSERT_TRUE(line.endTime >= 0);
        }
    }
}

TEST(LrcParser, CLrcParser_Txt) {
    cstr_t SZ_LYRICS_TEST = "Artist:artist_name\nTitle: title_name\nEncoding:gb2312\nL1中\n\n\nL3\nL4\n\nL6";
    cstr_t valArtist = "artist_name";
    cstr_t valTitle = "title_name";
    cstr_t vLyrLines[] = { "artist_name - title_name", "L1中", "", "", "L3", "L4", "", "L6" };

    string gbStr;
    utf8ToMbcs(SZ_LYRICS_TEST, -1, gbStr, ED_GB2312);

    utf16string wStr;
    utf8ToUCS2(SZ_LYRICS_TEST, -1, wStr);

    utf16string wLyrics;
    wLyrics.append((utf16_t *)SZ_FE_UCS2, 1);
    wLyrics.append(wStr.c_str(), wStr.size());

    utf16string wLyricsBe;
    wLyricsBe.append(wLyrics.c_str(), wLyrics.size());
    ucs2EncodingReverse((utf16_t *)wLyricsBe.data(), (int)wLyricsBe.size());

    string utf8Lyrics;
    utf8Lyrics.append(SZ_FE_UTF8);
    utf8Lyrics.append(SZ_LYRICS_TEST);

    const StringView vLyrics[] = {
        gbStr,
        StringView((uint8_t *)wLyrics.c_str(), wLyrics.size() * sizeof(utf16_t)),
        StringView((uint8_t *)wLyricsBe.c_str(), wLyricsBe.size() * sizeof(utf16_t)),
        utf8Lyrics,
    };

    for (int i = 0; i < CountOf(vLyrics); i++) {
        printf("Round: %d\n", i);

        RawLyrics rawLyrics;
        int ret = parseLyricsBinary(vLyrics[i], false, ED_SYSDEF, rawLyrics);
        ASSERT_EQ(ret, ERR_OK);

        auto &props = rawLyrics.properties();
        ASSERT_EQ(props.lyrContentType, LCT_TXT);

        ASSERT_EQ(props.artist, valArtist);
        ASSERT_EQ(props.title, valTitle);
        ASSERT_EQ(props.getOffsetTime(), 0);
        // ASSERT_EQ(props.getEncodingIndex(), vEncoding[i]);

        LyricsLines lyrLines = rawLyrics.toLyricsLinesOnly();
        ASSERT_EQ(lyrLines.size(), CountOf(vLyrLines));
        for (int k = 0; k < lyrLines.size(); k++) {
            LyricsLine &line = lyrLines[k];

            ASSERT_TRUE(line.pieces.size() == 1);
            ASSERT_TRUE(k < CountOf(vLyrLines));
            ASSERT_EQ(line.pieces[0].text, vLyrLines[k]);
        }

        //
        // Cases on CLrcParser::toString
        //
        string strNewLyr;
        strNewLyr = toLyricsString(rawLyrics, true, false);

        RawLyrics rawLyrics3;
        ret = parseLyricsString(strNewLyr, false, rawLyrics3);
        ASSERT_EQ(ret, ERR_OK);

        {
            // Compare toString data.
            auto &props3 = rawLyrics3.properties();
            ASSERT_EQ(props3.artist, props.artist);
            ASSERT_EQ(props3.title, props.title);
            ASSERT_EQ(props3.getOffsetTime(), props.getOffsetTime());

            LyricsLines lyrLines3 = rawLyrics3.toLyricsLinesOnly();
            ASSERT_EQ(lyrLines3.size(), CountOf(vLyrLines));
            for (int k = 0; k < lyrLines3.size(); k++) {
                LyricsLine &line = lyrLines3[k];

                ASSERT_TRUE(line.pieces.size() == 1);
                ASSERT_TRUE(k < CountOf(vLyrLines));
                ASSERT_EQ(line.pieces[0].text, vLyrLines[k]);
            }
        }
    }
}

TEST(lrcTag, ParseLrcTimeTag) {
    // These are succeeded case.
    cstr_t vTestTag[] = { "[1:30.001]", "[ 10 : 03 . 01 ] abc", "[01:30.1][abc" };
    int vTime[] = { (1 * 60 + 30 ) * 1000 + 1, (10 * 60 + 3 ) * 1000 + 1 * 10, (1 * 60 + 30 ) * 1000 + 1 * 100 };
    cstr_t vNextPosStr[] = { "", " abc", "[abc" };

    for (int i = 0; i < CountOf(vTestTag); i++) {
        int nNextPos = 0, nTime = 0;
        bool bRet = parseLrcTimeTag(vTestTag[i], nNextPos, nTime);
        if (!bRet) {
            FAIL() << (stringPrintf("parseLrcTimeTag: failed, case: %d, %s", i, vTestTag[i]).c_str());
        }

        if (strncmp(vTestTag[i] + nNextPos, vNextPosStr[i], strlen(vNextPosStr[i])) != 0) {
            FAIL() << (stringPrintf("parseLrcTimeTag: Wrong nextPos: %d, case: %d, %s", nNextPos, i, vTestTag[i]).c_str());
        }

        if (nTime != vTime[i]) {
            FAIL() << (stringPrintf("parseLrcTimeTag: Wrong Time: %d, case: %d, %s", nNextPos, i, vTestTag[i]).c_str());
        }
    }

    // Failed case
    cstr_t vTestTagFailed[] = { "[1:30.001 abc", "[ 10a : 03 . 01 ]", "[01:30.1x]" };
    for (int i = 0; i < CountOf(vTestTagFailed); i++) {
        int nNextPos = 0, nTime = 0;
        bool bRet = parseLrcTimeTag(vTestTagFailed[i], nNextPos, nTime);
        if (bRet) {
            FAIL() << (stringPrintf("parseLrcTimeTag(Failure test): failed, case: %d, %s", i, vTestTagFailed[i]).c_str());
        }
    }
}

TEST(lrcTag, parseLrcAllTags) {
    // These are succeeded cases.
    cstr_t vTestTag[] = { "[ ti:value]\n[ar:val2]\n[ti:title]\n[AL:  album]\n[offset:]\n[length: 20]",
        "   [ti:val ue]\n[ti:title]\n[al:ab]\n[offset:1]",
        "[xx]\n  [Ti:value]\n[ar:val\n\n[offset:  33f]" };
    cstr_t vTitles[] = { "value", "val ue", "value" };
    cstr_t vArtists[] = { "val2", "", "" };
    cstr_t vAlbums[] = { "album", "ab", "" };
    int vOffsets[] = { 0, 1, 33 };
    cstr_t vLengths[] = { "20", "", "" };

    for (int i = 0; i < CountOf(vTestTag); i++) {
        LyricsProperties props;
        LyrTagParser tagParser(props);

        tagParser.parseLrcAllTags(vTestTag[i]);

        ASSERT_EQ(props.artist, vArtists[i]);
        ASSERT_EQ(props.title, vTitles[i]);
        ASSERT_EQ(props.album, vAlbums[i]);
        ASSERT_EQ(props.getOffsetTime(), vOffsets[i]);
        ASSERT_EQ(props.mediaLength, vLengths[i]);
    }

    // Failed case
    cstr_t vTestTagFailed[] = {"[t i:value]\n[ar val2]", "  [ti val ue]" };
    for (int i = 0; i < CountOf(vTestTagFailed); i++) {
        LyricsProperties props;
        LyrTagParser tagParser(props);

        tagParser.parseLrcAllTags(vTestTagFailed[i]);

        ASSERT_EQ(props.artist, "");
        ASSERT_EQ(props.title, "");
    }
}

TEST(lrcTag, parseTxtAllTags) {
    // These are succeeded cases.
    cstr_t vTestTag[] = { "  Title:value\nArtist:val2\nTitle:title\nAlbum:  album\n",
        "   Title:val ue\nTitle:title\nAlbum:ab\noffset:a1",
        "xx\n  Title:value\nartist:val\n\noffset:  @33f" };
    cstr_t vTitles[] = { "title", "title", "" };
    cstr_t vArtists[] = { "val2", "", "val" };
    cstr_t vAlbums[] = { "album", "ab", "" };

    for (int i = 0; i < CountOf(vTestTag); i++) {
        LyricsProperties props;
        LyrTagParser tagParser(props);

        printf("Round: %d, %s\n", i, vTestTag[i]);

        tagParser.parseTxtAllTags(vTestTag[i]);

        ASSERT_EQ(props.artist, vArtists[i]);
        ASSERT_EQ(props.title, vTitles[i]);
        ASSERT_EQ(props.album, vAlbums[i]);
        ASSERT_EQ(props.getOffsetTime(), 0);
    }

    // Failed case
    cstr_t vTestTagFailed[] = {"[ti:value]\na rtist:val2", "  Title val ue" };
    for (int i = 0; i < CountOf(vTestTagFailed); i++) {
        LyricsProperties props;
        LyrTagParser tagParser(props);

        tagParser.parseTxtAllTags(vTestTagFailed[i]);

        ASSERT_TRUE(props.artist.empty());
        ASSERT_TRUE(props.title.empty());
    }
}

TEST(lrcTag, CLrcTag_replaceInFile) {
    cstr_t SZ_LYRICS_TEST = "[ti: title中]\r\n\r\n[00:01.20]x\r\n[encoding: gb2312]\r\n[ar: artis中t]";

    string gbStr;
    utf8ToMbcs(SZ_LYRICS_TEST, -1, gbStr);

    utf16string wStr;
    utf8ToUCS2(SZ_LYRICS_TEST, -1, wStr);

    utf16string wLyrics;
    wLyrics.append((utf16_t *)SZ_FE_UCS2, 1);
    wLyrics.append(wStr.c_str(), wStr.size());

    utf16string wLyricsBe;
    wLyricsBe.append(wLyrics.c_str(), wLyrics.size());
    ucs2EncodingReverse((utf16_t *)wLyricsBe.data(), (int)wLyricsBe.size());

    string utf8Lyrics;
    utf8Lyrics.append(SZ_FE_UTF8);
    utf8Lyrics.append(SZ_LYRICS_TEST);

    const StringView vLyrics[] = {
        gbStr,
        StringView((uint8_t *)wLyrics.c_str(), wLyrics.size() * sizeof(utf16_t)),
        StringView((uint8_t *)wLyricsBe.c_str(), wLyricsBe.size() * sizeof(utf16_t)),
        utf8Lyrics,
    };

    auto path = getUnittestTempDir();
    auto fn = dirStringJoin(path.c_str(), "test_tag_replace_in_file.txt");

    for (int i = 0; i < CountOf(vLyrics); i++) {
        auto data = vLyrics[i];
        ASSERT_TRUE(writeFile(fn.c_str(), data));

        LyricsProperties props;
        LyrTagParser tagParser(props);

        props.artist = "newAr中";
        props.title = "newTi中";
        props.id = "newID中";
        props.by = "newBy中";
        props.mediaLength = "newMeidaLen中";

        tagParser.replaceInFile(fn.c_str(), true);

        string newData;
        ASSERT_TRUE(readFile(fn.c_str(), newData));

        newData = convertBinLyricsToUtf8(newData, false, ED_SYSDEF);

        string EXPECTED = "[ar: newAr中]\n[ti: newTi中]\n[by: newBy中]\n[length: newMeidaLen中]\n[id: newID中]\n\n[00:01.20]x";
        strrep(EXPECTED, "\n", SZ_NEW_LINE);
        ASSERT_EQ(newData, EXPECTED);
    }

    deleteFile(fn.c_str());
}

TEST(lrcTag, compressLyrics) {
    cstr_t LYR1 = "[ti: title中]\r\n\r\n[00:01.20]x\n[00:02.30][00:03.40]x\n[00:02.30]y\n\nx\ny\n[01:02.00]x\n";
    cstr_t R_LYR1 = "[ti: title中]\n\n[01:02][00:03.40][00:02.30][00:01.20]x\n[00:02.30]y\n\nx\ny\n";

    string lyrics = LYR1;
    ASSERT_TRUE(compressLyrics(lyrics));
    ASSERT_EQ(lyrics, R_LYR1);

    cstr_t LYR2 = "[ti: title中]\r\n\r\n[00:01.20]x\n[00:02.30]x1\n[00:02.30]y\n\nx\ny\n[01:02.00]x2\n";
    lyrics = LYR2;
    ASSERT_FALSE(compressLyrics(lyrics));
}

#endif
