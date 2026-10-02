#include "LyrTimestamps.h"


const char *getBase64Set();

namespace LyrTimestamps {

#define HEADER_TIME_STAMP_V0            '@'
#define CHAR_NO_TIME_STAMP              '_'
#define CHAR_MAX_TIME_VALUE             '^'
#define CHAR_NEGATIVE_TIME_VALUE        '-'
#define TIME_STAMP_UNIT                 100
#define MAX_DURATION                    64

bool isTimeStamps(cstr_t timestamps) {
    return timestamps[0] == HEADER_TIME_STAMP_V0;
}

void encodeTimeStamps(int duration, string &timeStamps) {
    const char *charSet = getBase64Set();

    if (duration < 0) {
        timeStamps += CHAR_NEGATIVE_TIME_VALUE;
        duration = -duration;
    }

    // duration can't be too large.
    if (duration > 10 * 60 * 1000) {
        duration = 6 * 1000;
    }

    duration /= TIME_STAMP_UNIT;

    while (duration >= 0) {
        if (duration >= MAX_DURATION) {
            timeStamps += CHAR_MAX_TIME_VALUE;
        } else {
            timeStamps += charSet[duration];
        }

        duration -= MAX_DURATION;
    }
}

// 解码一个时间戳字符。若遇到不在 base64 字符集内的字节（例如字符串被截断读到 '\0'、
// 或混入多字节字符），返回 nullptr 表示失败，由调用方放弃解析，而不是断言崩溃。
cstr_t decodeTimeStamps(cstr_t timestamps, int &duration) {
    duration = 0;

    if (*timestamps == CHAR_NO_TIME_STAMP) {
        // '_' 是“本行无时间戳”的标记，不应出现在这里，视为非法输入。
        return nullptr;
    }

    bool isNegative = (*timestamps == CHAR_NEGATIVE_TIME_VALUE);
    if (isNegative) {
        timestamps++;
    }

    while (*timestamps == CHAR_MAX_TIME_VALUE) {
        duration += MAX_DURATION;
        timestamps++;
    }

    const char *charSet = getBase64Set();
    int i = 0;
    for (i = 0; i < MAX_DURATION; i++) {
        if (charSet[i] == *timestamps) {
            duration += i;
            break;
        }
    }
    if (i >= MAX_DURATION) {
        // 无效字节（不在 base64 字符集内）。
        return nullptr;
    }

    duration *= TIME_STAMP_UNIT;

    if (isNegative) {
        duration = -duration;
    }

    return timestamps + 1;
}

bool parse(cstr_t timestamps, RawLyrics &rawLyrics) {
    // Header
    if (timestamps[0] != HEADER_TIME_STAMP_V0) {
        return false;
    }
    timestamps++;

    // offset time
    int timeOffset = 0;
    timestamps = decodeTimeStamps(timestamps, timeOffset);
    if (timestamps == nullptr) {
        return false;
    }

    // Time of every line
    int timeAboveLine = 0;
    for (auto &line : rawLyrics) {
        // 数据不满足“歌词行且仅一个未定时的 Piece”时，视为非法数据，放弃解析而非断言。
        if (!line.isLyricsLine || line.pieces.empty() || line.pieces[0].beginTime != -1) {
            return false;
        }
        auto &piece = line.pieces[0];

        if (*timestamps == CHAR_NO_TIME_STAMP) {
            timestamps++;
            // Just remove this line from RawLyrics (will be kept in m_arrFileLines).
            line.isLyricsLine = false;
            line.content = piece.text;
            line.pieces.clear();
        } else {
            int duration = 0;
            timestamps = decodeTimeStamps(timestamps, duration);
            if (timestamps == nullptr) {
                return false;
            }
            timeAboveLine = piece.beginTime = line.beginTime = timeAboveLine + duration;
        }
    }

    rawLyrics.properties().setOffsetTime(timeOffset);
    rawLyrics.properties().lyrContentType = LCT_LRC;

    return true;
}

string toString(const RawLyrics &rawLyrics) {
    string strTimeStamps;

    // Header
    strTimeStamps.clear();
    strTimeStamps += HEADER_TIME_STAMP_V0;

    // offset time
    encodeTimeStamps(rawLyrics.properties().getOffsetTime(), strTimeStamps);

    // Time of every line
    int timeAboveLine = 0;
    for (auto &line :rawLyrics) {
        if (line.isLyricsLine) {
            int duration = line.beginTime - timeAboveLine + TIME_STAMP_UNIT / 2;
            encodeTimeStamps(duration, strTimeStamps);
            // Round the above line.
            timeAboveLine += duration / TIME_STAMP_UNIT * TIME_STAMP_UNIT;
        } else {
            strTimeStamps += CHAR_NO_TIME_STAMP;
        }
    }

    return strTimeStamps;
}

} // namespace LyrTimestamps
