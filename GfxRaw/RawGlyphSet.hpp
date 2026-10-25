//
//  RawGlyphSet.hpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/4.
//

#ifndef RawGlyphSet_hpp
#define RawGlyphSet_hpp

#include "RawGraph.h"

#include <unordered_map>


#define _CLEAR_TYPE

//
// Glyph pixel format
//
#ifdef _CLEAR_TYPE
enum {
    G_B                         = PIX_B,
    G_G                         = PIX_G,
    G_R                         = PIX_R,
    G_A                         = PIX_A,
    G_PIX_SIZE                  = 4,
};
#else //  _CLEAR_TYPE
enum {
    G_R                         = 0,
    G_G                         = 0,
    G_B                         = 0,
    G_A                         = 0,
    G_PIX_SIZE                  = 1,
};
#endif //  _CLEAR_TYPE


struct Glyph {
    Glyph();
    ~Glyph();

    int widthOutlined() { return widthBitmap + marginOutlined; }
    int heightOutlined() { return heightBitmap + marginOutlined; }

    int widthBytes() const { return widthBitmap * G_PIX_SIZE; }
    int widthBytesOutlined() const { return (widthBitmap + marginOutlined) * G_PIX_SIZE; }

    string                      ch;
    uint16_t                    nWidth;             // width of char

    uint8_t                     leftOffset, topOffset; // Left and top offset of bitmap.
    uint16_t                    widthBitmap;
    uint8_t                     heightBitmap;       // width of bitmap
    uint8_t                     marginOutlined;
    bool                        freed;

    uint8_t                     *bitmap;
    uint8_t                     *bitmapOutlined;
    int64_t                     nLastUsedTime;
};

// 将烘焙字形压回字号单元格，各字形基线对齐到同一行（见 GfxRaw/GlyphBaselineFit.cpp）
// bottomPad：基线离单元格底边的像素。满高汉字用它把顶边贴住盒子，空位留在下面。
// topPad：格子顶上留白。绘制会再减掉 MARGIN_FONT，所以 1 表示顶上可见 1px，0 表示顶边贴住盒子。
void fitGlyphBitmapToBaseline(Glyph *glyph, int fontHeight, int bakeBaselineY, int refAscent, int bottomPad = 0, int topPad = 1);


#ifdef _WIN32
#include "win32/RawGlyphBuilder.h"
#endif

#ifdef _LINUX_GTK2
#include "gtk2/RawGlyphBuilder.h"
#endif

#ifdef _MAC_OS
#include "mac/RawGlyphBuilder.h"
#endif

class CRawGlyphSet;

class CRawGlyphSetMgr {
public:
    CRawGlyphSetMgr();
    virtual ~CRawGlyphSetMgr();

    CRawGlyphSet *getGlyphSet(const FontInfoEx &font);
    void removeRawGlyphSet(CRawGlyphSet *pRawGlyphSet);

protected:
    typedef list<CRawGlyphSet*>    LIST_SET;

    LIST_SET                    m_listSet;

};

extern CRawGlyphSetMgr g_rawGlyphSetMgr;

class CRawGlyphSet {
    OBJ_REFERENCE_DECL
public:
    CRawGlyphSet();
    virtual ~CRawGlyphSet();

    bool create(const FontInfoEx &font);
    bool isSame(const FontInfoEx &font) const { return m_font.isSame(font); }
    const FontInfoEx &getFont() const { return m_font; }
    int getHeight() const;
    Glyph *getGlyph(string &ch);

    void clearGlyph();

protected:
    friend class CRawGlyphSetMgr;
    typedef unordered_map<string, Glyph*>        MAP_GLYPH;

    FontInfoEx                  m_font;

    MAP_GLYPH                   m_mapGlyph;

    int64_t                     m_timeLastClean;

    CRawGlyphBuilder            m_rawGlyphBuilder;

};


#endif /* RawGlyphSet_hpp */
