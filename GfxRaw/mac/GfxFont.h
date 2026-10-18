#ifndef _MLFONT_INC_
#define _MLFONT_INC_

#pragma once

#ifndef DEFAULT_CHARSET
#define DEFAULT_CHARSET     1
#endif

#include "../FontInfoEx.h"


typedef const struct __CTFont * CTFontRef;
typedef const struct __CFString * CFStringRef;
typedef const struct __CFDictionary * CFDictionaryRef;

class GfxFont {
public:
    GfxFont(void);
    virtual ~GfxFont(void);

    bool create(cstr_t faceName, int fontHeight, bool italic, int weight, bool isUnderline);

    // 在基线位置 (x, y) 绘制，返回字符串宽度
    int draw(CGContextRef ctx, CFStringRef attrString, int x, int y);

    int getAscent() const;
    int getDescent() const;

    void destroy();

    // For MAC only.
    CTFontRef getHandle() const { return m_font; }

protected:
    CTFontRef                   m_font;
    CFDictionaryRef             m_attributes;
    int                         m_fontHeight;

};

#endif // _MLFONT_INC_
