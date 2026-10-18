#ifndef _MLFONT_INC_
#define _MLFONT_INC_

#pragma once

#include "UnicodeRange.h"
#include "../FontInfoEx.h"


extern uint8_t g_byFontQuality;

template<class _Font>
class CFontCollection {
public:
    typedef    map<uint8_t, _Font*>    MAP_FONT;

    CFontCollection() {
    //: m_logFont(25, 0, 0, 0, FW_REGULAR, false, false, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, g_byFontQuality, DEFAULT_PITCH | FF_ROMAN, "Tahoma")
        LOGFONTW lgfont = {25, 0, 0, 0, FW_REGULAR, false, false, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, g_byFontQuality, DEFAULT_PITCH | FF_ROMAN, "Tahoma"};

        m_logFontLatin9 = lgfont;
        m_logFontOthers = lgfont;
    }

    ~CFontCollection() {
        destroy();
    }

    void setFont(FontInfoEx &font) {
        if (m_logFontLatin9.lfHeight == font.getSize() &&
            m_logFontLatin9.lfWeight == font.getWeight() &&
            m_logFontLatin9.lfItalic == (uint8_t)font.getItalic() &&
            m_logFontLatin9.lfUnderline == (uint8_t)font.isUnderline()) {
            if (strcmp(font.getName(), m_logFontLatin9.lfFaceName) == 0
                && strcmp(font.getNameOthers(), m_logFontOthers.lfFaceName) == 0) {
                return;
            }
        }

        destroy();

        m_logFontOthers.lfHeight = m_logFontLatin9.lfHeight = font.getSize();
        m_logFontOthers.lfWeight = m_logFontLatin9.lfWeight = font.getWeight();
        m_logFontOthers.lfItalic = m_logFontLatin9.lfItalic = font.getItalic();
        m_logFontOthers.lfUnderline = m_logFontLatin9.lfUnderline = font.isUnderline();
        if (isEmptyString(font.getName())) {
            strcpy_safe(m_logFontLatin9.lfFaceName, CountOf(m_logFontLatin9.lfFaceName), "Tahoma");
        } else {
            strcpy_safe(m_logFontLatin9.lfFaceName, CountOf(m_logFontLatin9.lfFaceName), font.getName());
        }

        if (isEmptyString(font.getNameOthers())) {
            // 中文缺省用微软雅黑，避免 GDI 回退到宋体导致小字模糊
            strcpy_safe(m_logFontOthers.lfFaceName, CountOf(m_logFontOthers.lfFaceName), "Microsoft YaHei");
        } else {
            strcpy_safe(m_logFontOthers.lfFaceName, CountOf(m_logFontOthers.lfFaceName), font.getNameOthers());
        }
    }

    void destroy() {
        if (m_mapFonts.empty()) {
            return;
        }

        for (MAP_FONT::iterator it = m_mapFonts.begin(); it != m_mapFonts.end(); it++) {
            delete ((*it).second);
        }
        m_mapFonts.clear();
    }

    int getFontHeight() const { return m_logFontLatin9.lfHeight; }

    template<class _Graphics, class _Worker>
    void resovleText(_Graphics *canvas, cwstr_t szText, int nLen, _Worker &fun) {
        cwstr_t szBeg, szEnd;
        int nClip;
        uint8_t lfCharSetAbove = DEFAULT_CHARSET, lfCharSet = DEFAULT_CHARSET;
        CharEncodingType encoding;

        if (nLen == -1) {
            nLen = strlen(szText);
        }

        szEnd = szText + nLen;

        while (szText < szEnd) {
            szBeg = szText;
            nClip = 0;
            while (szText < szEnd) {
                if (*szText != ' ') {
                    encoding = FindCharEncUnicodeRange(*szText);

                    lfCharSet = GetCharEncodingByID(encoding).fontCharset;
                    if (lfCharSet != lfCharSetAbove && nClip > 0) {
                        break;
                    }
                    lfCharSetAbove = lfCharSet;
                }

                nClip++;
                szText++;
            }
            if (nClip) {
                fun(getFont(canvas, lfCharSetAbove), szBeg, nClip);
            }
        }
    }

    template<class _Graphics>
    _Font *getFont(_Graphics *canvas, uint8_t byCharset) {
        MAP_FONT::iterator it;
        _Font *pFont;
        it = m_mapFonts.find(byCharset);
        if (it == m_mapFonts.end()) {
            pFont = new _Font;
            if (byCharset == DEFAULT_CHARSET) {
                pFont->create(canvas, m_logFontLatin9);
            } else {
                pFont->create(canvas, m_logFontOthers);
            }
            m_mapFonts[byCharset] = pFont;
        } else {
            pFont = (*it).second;
        }

        return pFont;
    }

protected:
    LOGFONTW                    m_logFontLatin9;
    LOGFONTW                    m_logFontOthers;
    MAP_FONT                    m_mapFonts;

};


#endif // _MLFONT_INC_
