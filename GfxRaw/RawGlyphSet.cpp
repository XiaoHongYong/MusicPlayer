//
//  RawGlyphSet.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/4.
//

#include "RawGlyphSet.hpp"


#define MEM_CLEAN_DURATION      (1000 * 60 * 2)
#define MAX_GLYPH_CAP           512

CRawGlyphSetMgr::CRawGlyphSetMgr() {
}

CRawGlyphSetMgr::~CRawGlyphSetMgr() {
    m_listSet.clear();
}

CRawGlyphSet *CRawGlyphSetMgr::getGlyphSet(const FontInfoEx &font) {
    for (CRawGlyphSet *pSet : m_listSet) {
        if (pSet->isSame(font)) {
            pSet->addRef();
            return pSet;
        }
    }

    CRawGlyphSet *pSet = new CRawGlyphSet;

    pSet->create(font);
    pSet->addRef();
    m_listSet.push_back(pSet);

    return pSet;
}

void CRawGlyphSetMgr::removeRawGlyphSet(CRawGlyphSet *pRawGlyphSet) {
    for (LIST_SET::iterator it = m_listSet.begin(); it != m_listSet.end(); ++it) {
        CRawGlyphSet *pSet = *it;
        if (pSet == pRawGlyphSet) {
            m_listSet.erase(it);
            return;
        }
    }
}

CRawGlyphSetMgr g_rawGlyphSetMgr;


Glyph::Glyph() {
    leftOffset = topOffset = heightBitmap = marginOutlined = 0;
    widthBitmap = nWidth = 0;
    bitmap = nullptr;
    bitmapOutlined = nullptr;
    nLastUsedTime = getTickCount();
    freed = false;
}

Glyph::~Glyph() {
    if (bitmapOutlined) {
        delete[] bitmapOutlined;
    }
    if (bitmap) {
        delete[] bitmap;
    }
}

CRawGlyphSet::CRawGlyphSet() {
    OBJ_REFERENCE_INIT

    m_timeLastClean = getTickCount();
    m_mapGlyph.reserve(MAX_GLYPH_CAP);
}

CRawGlyphSet::~CRawGlyphSet() {
    clearGlyph();

    g_rawGlyphSetMgr.removeRawGlyphSet(this);
}

bool CRawGlyphSet::create(const FontInfoEx &font) {
    m_font = font;

    m_rawGlyphBuilder.init(font);

    return true;
}

int CRawGlyphSet::getHeight() const {
    return m_rawGlyphBuilder.getHeight();
}

Glyph *CRawGlyphSet::getGlyph(string &ch) {
    Glyph *glyph = nullptr;
    auto now = getTickCount();

    // 热路径：缓存命中只做一次哈希查找，不做任何清理扫描。
    auto it = m_mapGlyph.find(ch);
    if (it != m_mapGlyph.end()) {
        glyph = (*it).second;
        if (glyph->freed && glyph->bitmap == nullptr) {
            // It must has been freed, renew one.
            Glyph *temp = m_rawGlyphBuilder.buildGlyph(ch);
            assert(temp);
            if (!temp) {
                return nullptr;
            }
            glyph->bitmap = temp->bitmap;
            glyph->freed = false;
            temp->bitmap = nullptr;
            delete temp;
        }

        glyph->nLastUsedTime = now;
        return glyph;
    }

    // 只在分配新字形（较少发生）时才考虑清理，避免每次访问都扫全表。
    if (m_mapGlyph.size() > MAX_GLYPH_CAP && now - m_timeLastClean >= MEM_CLEAN_DURATION) {
        // Clean unused glyph
        m_timeLastClean = now;
        for (auto itClean = m_mapGlyph.begin(); itClean != m_mapGlyph.end(); ++itClean) {
            auto glyphClean = (*itClean).second;
            if (now - glyphClean->nLastUsedTime >= MEM_CLEAN_DURATION) {
                if (glyphClean->bitmap) {
                    delete[] glyphClean->bitmap;
                    glyphClean->bitmap = nullptr;
                    glyphClean->freed = true;
                    if (glyphClean->bitmapOutlined) {
                        delete[] glyphClean->bitmapOutlined;
                        glyphClean->bitmapOutlined = nullptr;
                    }
                }
            }
        }
    }

    glyph = m_rawGlyphBuilder.buildGlyph(ch);
    assert(glyph);
    if (!glyph) {
        return nullptr;
    }
    m_mapGlyph[ch] = glyph;

    glyph->nLastUsedTime = now;

    return glyph;
}

void CRawGlyphSet::clearGlyph() {
    for (MAP_GLYPH::iterator it = m_mapGlyph.begin(); it != m_mapGlyph.end(); ++it) {
        Glyph *p = (*it).second;
        delete p;
    }
    m_mapGlyph.clear();
}
