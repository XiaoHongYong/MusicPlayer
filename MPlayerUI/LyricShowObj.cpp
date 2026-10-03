/********************************************************************
    Created  :    2002年1月3日 16:21:56
    FileName :    LyricShowObj.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include "MPlayerApp.h"
#include "LyricShowObj.h"
#include "LyricShowAgentObj.h"


bool isMoreLatinLetter(int c) {
    // Used 16 bytes to store whether the char is a letter for unicode 128 to 255.
    static const uint8_t IsLatin[] = {8, 69, 0, 196, 0, 0, 0, 0, 255, 255, 127, 255, 255, 255, 127, 255};
    if (c >= 128 && c < 256) {
        c -= 128;
        return (IsLatin[c / 8] & (1 << (c % 8))) > 0;
    }

    return false;
}

bool isCharAlphaNumericEx(int c) {
    return isAlpha(c) || isDigit(c) || isMoreLatinLetter(c);
}

RawImageDataPtr autoFitImageToRect(const RawImageDataPtr &pImgSrc, int wDst, int hDst, int nBitCount) {
    assert(pImgSrc);

    if (!pImgSrc || pImgSrc->height == 0 || hDst == 0 || wDst == 0) {
        return nullptr;
    }

    int x = 0, y = 0;
    int wSrcToBlt, hSrcToBlt;

    if (float(pImgSrc->width) / pImgSrc->height >= float(wDst) / hDst) {
        wSrcToBlt = (int)(float(wDst) * pImgSrc->height / hDst + 0.5);
        hSrcToBlt = pImgSrc->height;
        x = (pImgSrc->width - wSrcToBlt) / 2;
        y = 0;
    } else {
        wSrcToBlt = pImgSrc->width;
        hSrcToBlt = (int)(float(hDst) * pImgSrc->width / wDst + 0.5);
        x = 0;
        y = (pImgSrc->height - hSrcToBlt) / 2;
    }

    RawImageDataPtr pImgDst = createRawImageData(wDst, hDst, pImgSrc->bitCount);

    CRawImage imgSrc(pImgSrc);
    CRawImage imgDst(pImgDst);

    imgSrc.stretchBlt(&imgDst, 0, 0, wDst, hDst, x, y, wSrcToBlt, hSrcToBlt, BPM_COPY | BPM_BILINEAR);

    if (pImgSrc->bitCount != nBitCount) {
        RawImageDataPtr pImgDstNew = createRawImageData(wDst, hDst, nBitCount);
        imgSrc.attach(pImgDst);
        imgDst.attach(pImgDstNew);

        imgSrc.blt(&imgDst, 0, 0, BPM_COPY);

        pImgDst = pImgDstNew;
    }

    return pImgDst;
}

int getLyricFragDrawWidth(LyricsPiece &piece, CRawGraph* canvas) {
    if (piece.drawWidth == 0) {
        CSize size;
        canvas->getTextExtentPoint32(piece.text, &size);
        piece.drawWidth = (int16_t)size.cx;
    }

    return piece.drawWidth;
}

int getMLRowDrawWidth(const LyricsLine &line, CRawGraph *canvas) {
    int w = 0;

    for (auto &piece : line.pieces) {

        CSize size;
        if (canvas->getTextExtentPoint32(piece.text, &size)) {
            w += size.cx;
        }
    }

    return w;
}


void wrapWords(CRawGraph *canvas, int nWidthMax, const string &text, vector<string> &vStr) {
    if (text.size() <= 10) {
        vStr.push_back(text);
        return;
    }

    string str;

    StringIterator strIterator(text.c_str());
    while (!strIterator.isEOS()) {
        int nWidth = 0;
        int nLineBeg = strIterator.getPos();
        int nLastBreak = -1;
        for (; !strIterator.isEOS(); ++strIterator) {
            CSize size;
            if (canvas->getTextExtentPoint32(strIterator.curChar(), strIterator.curChar().size(), &size)) {
                nWidth += size.cx;
            }
            if (nWidth > nWidthMax && strIterator.getPos() - nLineBeg >= 10) {
                if (nLastBreak > 0) {
                    assert(nLastBreak <= strIterator.getPos());
                    strIterator.setPos(nLastBreak);
                    ++strIterator;
                }
                break;
            }

            unsigned int c = strIterator.curChar()[0];
            if (c >= 128) {
                c = strIterator.curChar()[0];
            }
            if (c >= 256 || c == ' ' || c == '\t') {
                nLastBreak = strIterator.getPos();
            }
        }
        if (strIterator.getPos() == nLineBeg) {
            ++strIterator;
        }
        str.clear();
        str.append(text.c_str() + nLineBeg, strIterator.getPos() - nLineBeg);
        if (str.size()) {
            if (str.data()[str.size() - 1] == '\r') {
                str.resize(str.size() - 1);
            }
        }
        vStr.push_back(str.c_str());

        if (strIterator.curChar()[0] == '\n') {
            ++strIterator;
        }
    }
}

bool wrapDisplayLyrics(LyricsLines &lyrLinesSrc, LyricsLines &lyrLinesOut, CRawGraph *canvas, int nWidthMax, bool bVerticalStyle) {

    lyrLinesOut.clear();

    vector<string> vStr;
    for (auto &lineSrc : lyrLinesSrc) {
        if (lineSrc.pieces.size() > 1 || lineSrc.pieces.size() == 0
            || getMLRowDrawWidth(lineSrc, canvas) <= nWidthMax) {
            // Not support pieces > 1.
            lyrLinesOut.push_back(lineSrc);
        } else {
            // Wrap
            LyricsPiece &pieceSrc = lineSrc.pieces[0];
            vStr.clear();
            wrapWords(canvas, nWidthMax, pieceSrc.text, vStr);

            if (bVerticalStyle) {
                // All lines of lyrics has same time, for vertical scroll styles.
                for (int k = 0; k < (int)vStr.size(); k++) {
                    LyricsLine line(pieceSrc.beginTime, pieceSrc.endTime);
                    line.appendPiece(pieceSrc.beginTime, pieceSrc.endTime, vStr[k]);
                    lyrLinesOut.push_back(line);
                }
            } else {
                int nLen = (int)pieceSrc.text.size();
                int nTimeSpan = lineSrc.endTime - lineSrc.beginTime;

                // assert(vStr.size() > 1);
                int beginTime = lineSrc.beginTime;
                for (int k = 0; k < (int)vStr.size(); k++) {
                    int endTime;
                    if (nLen > 0) {
                        endTime = beginTime + (int)(vStr[k].size() * nTimeSpan) / nLen;
                    } else {
                        endTime = beginTime + nTimeSpan;
                    }

                    LyricsLine line(beginTime, endTime);
                    line.appendPiece(beginTime, endTime, vStr[k]);
                    lyrLinesOut.push_back(line);

                    beginTime = endTime;
                }
            }
        }
    }

    return true;
}

// 每次调整歌词的速度的基本单位
const int SET_SPEED_SPAN = 500;


void alphaBlendColor(CColor &clr1, CColor &clr2, int nAlpha, CColor &clrOut) {
    assert(nAlpha >= 0 && nAlpha <= 255);
    int nPart1 = 255 - nAlpha;

    clrOut.set((clr1.r() * nPart1 + clr2.r() * nAlpha) / 255,
        (clr1.g() * nPart1 + clr2.g() * nAlpha) / 255,
        (clr1.b() * nPart1 + clr2.b() * nAlpha) / 255);
}

void gradientFillImageVert(RawImageData *pImage, CRect &rc, CColor clrGradient[], int nClrCount) {
    assert(pImage->bitCount == 24 || pImage->bitCount == 32);
    assert(nClrCount >= 2);

    int nPixCount = 3;
    if (pImage->bitCount == 24) {
        nPixCount = 3;
    } else if (pImage->bitCount == 32) {
        nPixCount = 4;
    }

    if (rc.right > pImage->width) {
        rc.right = pImage->width;
    }
    if (rc.bottom > pImage->height) {
        rc.bottom = pImage->height;
    }

    float block = (float)rc.height() / (nClrCount - 1);
    int y = rc.top;
    for (int i = 0; i + 2 <= nClrCount; i++) {
        int r0 = clrGradient[i].r(), g0 = clrGradient[i].g(), b0 = clrGradient[i].b();
        int r1 = clrGradient[i + 1].r(), g1 = clrGradient[i + 1].g(), b1 = clrGradient[i + 1].b();

        float dr, dg, db;
        dr = float(r1 - r0) / block;
        dg = float(g1 - g0) / block;
        db = float(b1 - b0) / block;

        float rN, gN, bN;
        rN = (float)(r0 + 0.5); gN = (float)(g0 + 0.5); bN = (float)(b0 + 0.5);
        uint8_t *row = pImage->pixPtr(rc.left, y);
        int yEnd = rc.top + rc.height() * (i + 1) / (nClrCount - 1);
        for (; y < yEnd; y++) {
            uint8_t *pixl = row;
            for (int x = rc.left; x < rc.right; x++) {
                pixl[PixPosition::PIX_B] = (uint8_t)bN;
                pixl[PixPosition::PIX_G] = (uint8_t)gN;
                pixl[PixPosition::PIX_R] = (uint8_t)rN;
                pixl += nPixCount;
            }

            bN += db; gN += dg; rN += dr;
            row += pImage->stride;
        }
    }
}

void gradientFillImageHorz(RawImageData *pImage, CRect &rc, CColor clrGradient[], int nClrCount) {
    assert(pImage->bitCount == 24 || pImage->bitCount == 32);
    assert(nClrCount >= 1);

    if (nClrCount == 1) {
        fillRect(pImage, rc.left, rc.top, rc.right, rc.bottom, clrGradient[0], BPM_COPY, 255);
        return;
    }

    int nPixCount = 3;
    if (pImage->bitCount == 24) {
        nPixCount = 3;
    } else if (pImage->bitCount == 32) {
        nPixCount = 4;
    }

    if (rc.right > pImage->width) {
        rc.right = pImage->width;
    }
    if (rc.bottom > pImage->height) {
        rc.bottom = pImage->height;
    }

    float block = (float)rc.width() / (nClrCount - 1);
    int x = rc.left;
    for (int i = 0; i + 2 <= nClrCount; i++) {
        int r0 = clrGradient[i].r(), g0 = clrGradient[i].g(), b0 = clrGradient[i].b();
        int r1 = clrGradient[i + 1].r(), g1 = clrGradient[i + 1].g(), b1 = clrGradient[i + 1].b();

        float dr, dg, db;
        dr = float(r1 - r0) / block;
        dg = float(g1 - g0) / block;
        db = float(b1 - b0) / block;

        float rN, gN, bN;
        rN = (float)(r0 + 0.5); gN = (float)(g0 + 0.5); bN = (float)(b0 + 0.5);
        uint8_t *col = pImage->pixPtr(x, rc.top);
        int xEnd = rc.left + rc.width() * (i + 1) / (nClrCount - 1);
        for (; x < xEnd; x++) {
            uint8_t *pixl = col;
            for (int y = rc.top; y < rc.bottom; y++) {
                pixl[PixPosition::PIX_B] = (uint8_t)bN;
                pixl[PixPosition::PIX_G] = (uint8_t)gN;
                pixl[PixPosition::PIX_R] = (uint8_t)rN;
                pixl += pImage->stride;
            }

            bN += db; gN += dg; rN += dr;
            col += nPixCount;
        }
    }
}

RawImageDataPtr createGradientFillImage(int nHeight, CColor clrGradient[3]) {
    // Raw font will add the Margin, we need to add it too.
    nHeight += MARGIN_FONT * 2;

    RawImageDataPtr image = createRawImageData(nHeight * 2, nHeight, 24);

    CRect rc(0, 0, image->width, image->height);
    gradientFillImageVert(image.get(), rc, clrGradient, 3);
    return image;
}

IdToString g_idsLyrDisplayOpt[] = {
    { DO_NORMAL,            "normal" },
    { DO_FADEOUT_LOWCOLOR,    "fadein" },
    { DO_FADEOUT_BG,        "fadeout" },
    { DO_AUTO,                "auto" },
};

cstr_t displayOptToStr(DISPLAY_OPTIONS displayOpt) {
    return idToString(g_idsLyrDisplayOpt, displayOpt, g_idsLyrDisplayOpt[DO_AUTO].szId);
}

DISPLAY_OPTIONS displayOptFromStr(cstr_t szDisplayOpt) {
    return (DISPLAY_OPTIONS)stringToID(g_idsLyrDisplayOpt, szDisplayOpt, DO_AUTO);
}

struct LyrShowTOBFieldName {
    cstr_t                      szOBM;
    cstr_t                      szFill;
    cstr_t                      szGradient1, szGradient2, szGradient3;
    cstr_t                      szAutoGradient;
    cstr_t                      szPattern;
    cstr_t                      szBorder;
};

cstr_t        _LyrShowTOBFieldNameHilight[] = {
    "FgColor",
    "HilightGradient1", "HilightGradient2", "HilightGradient3",
    "HilightBorderColor",
    "HilightOBM",
    "HilightPattern",
};

cstr_t        _LyrShowTOBFieldNameLowlight[] = {
    "FgLowColor",
    "LowlightGradient1", "LowlightGradient2", "LowlightGradient3",
    "LowlightBorderColor",
    "LowlightOBM",
    "LowlightPattern",
};

cstr_t getLyrTOBSettingName(bool bHighlight, CLyricShowObj::TOBColorIndex nameIndex) {
    assert(CountOf(_LyrShowTOBFieldNameLowlight) == CountOf(_LyrShowTOBFieldNameHilight));
    assert(nameIndex >= 0 && nameIndex < CountOf(_LyrShowTOBFieldNameLowlight));
    if (bHighlight) {
        return _LyrShowTOBFieldNameHilight[nameIndex];
    } else {
        return _LyrShowTOBFieldNameLowlight[nameIndex];
    }
}

string getPatternDir();

RawImageDataPtr loadRawImageDataPatternFile(cstr_t szFile) {
    string strFile = getPatternDir();
    strFile += szFile;
    if (isFileExist(strFile.c_str())) {
        return loadRawImageDataFromFile(strFile.c_str());
    } else {
        return loadRawImageDataFromFile(szFile);
    }
}

void loadLyrOverlayBlendingSettings(cstr_t szSectName, CLyricShowObj::TextOverlayBlending &tob, int nGradientPatternHeight, bool bHilight) {
    assert(CountOf(_LyrShowTOBFieldNameLowlight) == CountOf(_LyrShowTOBFieldNameHilight));

    cstr_t *fieldName = bHilight ? _LyrShowTOBFieldNameHilight : _LyrShowTOBFieldNameLowlight;

    profileGetColorValue(tob.clr[CLyricShowObj::TCI_BORDER], szSectName, fieldName[CLyricShowObj::TCI_BORDER]);
    profileGetColorValue(tob.clr[CLyricShowObj::TCI_FILL], szSectName, fieldName[CLyricShowObj::TCI_FILL]);

    tob.obm = (OverlayBlendingMode)g_profile.getInt(szSectName, getLyrTOBSettingName(bHilight, CLyricShowObj::TCI_OBM), OBM_COLOR);
    if (tob.obm == OBM_PATTERN) {
        tob.strPatternFile = g_profile.getString(szSectName, bHilight ? "HilightPattern" : "LowlightPattern", "");
        if (!tob.strPatternFile.empty()) {
            RawImageDataPtr image = loadRawImageDataPatternFile(tob.strPatternFile.c_str());
            if (image && image->bitCount != 24) {
                image = convertTo24BppRawImage(image);
            }

            if (image) {
                tob.imgPattern = image;
            }
        }
    } else if (tob.obm == OBM_GRADIENT_COLOR) {
        profileGetColorValue(tob.clr[CLyricShowObj::TCI_GRADIENT1], szSectName, fieldName[CLyricShowObj::TCI_GRADIENT1]);
        profileGetColorValue(tob.clr[CLyricShowObj::TCI_GRADIENT2], szSectName, fieldName[CLyricShowObj::TCI_GRADIENT2]);
        profileGetColorValue(tob.clr[CLyricShowObj::TCI_GRADIENT3], szSectName, fieldName[CLyricShowObj::TCI_GRADIENT3]);

        tob.imgPattern = createGradientFillImage(nGradientPatternHeight, tob.clr + CLyricShowObj::TCI_GRADIENT1);
    } else {
        tob.imgPattern = nullptr;
        tob.obm = OBM_COLOR;
    }

    if (!tob.imgPattern) {
        tob.obm = OBM_COLOR;
    }
}

void createLyrOverlayBlendingPattern(CLyricShowObj::TextOverlayBlending &tob, int nGradientPatternHeight) {
    assert(nGradientPatternHeight > 0 && nGradientPatternHeight < 300);

    tob.imgPattern = nullptr;
    if (tob.obm == OBM_PATTERN) {
        if (!tob.strPatternFile.empty()) {
            RawImageDataPtr image = loadRawImageDataPatternFile(tob.strPatternFile.c_str());
            if (image && image->bitCount != 24) {
                image = convertTo24BppRawImage(image);
            }

            tob.imgPattern = image;
        }
    } else if (tob.obm == OBM_GRADIENT_COLOR) {
        tob.imgPattern = createGradientFillImage(nGradientPatternHeight, tob.clr + CLyricShowObj::TCI_GRADIENT1);
    }

    if (!tob.imgPattern) {
        tob.obm = OBM_COLOR;
    }
}

bool profileGetLyricsFont(cstr_t sectName, FontInfoEx &info) {
    cstr_t defaultFont = "Verdana, 16, bold, 0, 0, Tahoma";
    bool ret = info.parse(g_profile.getString(sectName, "Font", defaultFont));

    // Font height is associated with current skin name.
    auto skinName = MPlayerApp::getInstance()->getSkinFactory()->getSkinName();
    string keyFontHeight = string(sectName) + "FontHeight";
    auto height = g_profile.getInt(skinName, keyFontHeight.c_str(), 0);
    if (height > 0) {
        info.height = height;
    }

    return ret;
}

void profileWriteLyricsFont(EventType etColorTheme, cstr_t sectName, const FontInfoEx &info) {
    // Font height is associated with current skin name.
    auto skinName = MPlayerApp::getInstance()->getSkinFactory()->getSkinName();
    string keyFontHeight = string(sectName) + "FontHeight";
    g_profile.writeInt(skinName, keyFontHeight.c_str(), info.height);

    CMPlayerSettings::setSettings(etColorTheme, sectName, "Font", info.toString().c_str());
}


UIOBJECT_CLASS_NAME_IMP(CLyricShowObj, "LyricShow")

CLyricShowObj::CLyricShowObj() {
    m_nXMargin = 5;
    m_nYMargin = 2;

    m_curLyrics = nullptr;

    m_bCanWrapLines = true;

    m_pBgCache = nullptr;
    m_nBgCacheW = m_nBgCacheH = 0;
    m_bBgCacheDirty = true;

    m_bEnableAutoResize = true;

    m_bUseSkinStyle = false;

    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON | UO_MSG_WANT_KEY | UO_MSG_WANT_MAGNIFY;

    m_etDispSettings = ET_LYRICS_DISPLAY_SETTINGS;
    m_strSectName = SZ_SECT_LYR_DISPLAY;

    m_font.create(FontInfoEx("", "", 13, FW_NORMAL, 0, false), 1.0);

    m_nFontHeight = m_font.getHeight();


    m_nAlignment = LA_CENTER;

    m_LyricsDisplayOpt = DO_AUTO;

    m_hue = 0.0;

    m_clrBg.set(RGB(79, 88, 110));
    m_bSetSkinBg = true;

    m_tobHilight.obm = OBM_COLOR;
    m_tobHilight.clr[TCI_FILL].set(RGB(255, 255, 255));
    m_tobHilight.clr[TCI_BORDER].set(RGB(255, 0, 0));

    m_tobLowlight.obm = OBM_COLOR;
    m_tobLowlight.clr[TCI_FILL].set(RGB(255, 255, 255));
    m_tobLowlight.clr[TCI_BORDER].set(RGB(0, 0, 255));

    m_nLineSpacing = 2;

    m_bOutlineLyrics = false;

    m_bKaraoke = false;

    m_bUseAlbumArtAsBg = false;
    m_bUseBgImg = false;

    m_bDarkenLyrBgOnImg = true;
    m_nDarkenTopArea = float(1.25);
    m_nDarkenBottomArea = float(1.25);
    m_clrDarken.set(RGB(0x18, 0x3C, 0x7B));
    m_clrDarken.setAlpha(128);

    m_nextPicInFolder = 0;

    m_curAdjustVertAlign.loadStdCursor(Cursor::C_HAND);
    m_yBeginDrag = -1;
    m_bEnableAdjustVertAlign = true;
    m_bEnableAdjustVertAlignUser = g_profile.getBool(SZ_SECT_LYR_DISPLAY, "enableAdjustVertAlign", false);
    m_vertLineAlign = VLA_CENTER;
    m_nVertLineOffset = 0;

    m_nLeftAdjust = 0;
    m_nLeftOrg = 0;
}

CLyricShowObj::~CLyricShowObj() {
    g_profile.writeInt("NextPicInFolder", m_nextPicInFolder);
    m_pSkin->unregisterTimerObject(this);

    delete m_pBgCache;
    m_pBgCache = nullptr;

    invalidateRowCache();
}

void CLyricShowObj::onCreate() {
    m_curLyrics = &g_currentLyrics;

    {
        // Is floating lyrics?
        bool isFloatingLyr = CLyricShowAgentObj::isFloatingLyrMode(m_pSkin);
        MPlayerApp::getInstance()->getCurLyrDisplaySettingName(isFloatingLyr, m_strSectName, m_etDispSettings);
    }

    loadAllSettings();

    registerHandler(MPlayerApp::getEventsDispatcher(), m_etDispSettings, ET_LYRICS_CHANGED, ET_LYRICS_DRAW_UPDATE, ET_PLAYER_CUR_MEDIA_CHANGED);
    if (m_bUseBgImg) {
        m_pSkin->registerTimerObject(this, g_profile.getInt("SlideDelayTime", 10) * 1000);
    }

    // !auto resize || !wrap lines
    if (!m_bEnableAutoResize || !g_profile.getBool(SZ_SECT_UI, "AutoAdjustWndWidth", false)
        || !m_bCanWrapLines) {
        m_lyrLines = m_curLyrics->getLyricsLines();
        reverseLyricsForRightToLeftLanguage();
    }
}

void CLyricShowObj::onEvent(const IEvent *pEvent) {
    if (pEvent->eventType == m_etDispSettings) {
        onLyrDisplaySettings(pEvent->name.c_str(), pEvent->strValue.c_str());
    } else if (pEvent->eventType == ET_LYRICS_CHANGED) {
        onLyricsChanged();
    } else if (pEvent->eventType == ET_LYRICS_DRAW_UPDATE) {
        onPlayTimeChangedUpdate();
    } else if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED) {
        updateBgImage();
        invalidate();
    }
}

void CLyricShowObj::draw(CRawGraph *canvas) {
    canvas->setFont(&m_font);

    updateLyricDrawBufferBackground(canvas, m_rcObj);

    CRect rcClip = getClipRect();
    CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
    canvas->setClipBoundBox(rcClip);

    fastDraw(canvas, nullptr);
}

// fastDraw 用于绘制需要更新的区域
// 当 @fastDraw 有效时，不需要去绘制，仅仅计算需要绘制区域
// 否则，需要绘制所有区域
void CLyricShowObj::fastDraw(CRawGraph *canvas, CRect *prcUpdate) {
    if (prcUpdate) {
        prcUpdate->left = prcUpdate->right = m_rcObj.left;
        prcUpdate->top = prcUpdate->bottom = m_rcObj.top;
    }
}

void CLyricShowObj::fastDrawMediaInfo(CRawGraph *canvas, CRect *prcUpdate) {
    if (prcUpdate) {
        prcUpdate->setEmpty();
        return;
    }

    canvas->setFont(&m_font);

    int nLineHeight = (m_etDispSettings == ET_LYRICS_FLOATING_SETTINGS ? m_nFontHeight : getLineHeight());
    int y = (m_rcObj.top + m_rcObj.bottom) / 2 - (int)m_lyrLines.size() * nLineHeight / 2;
    int x;

    if (m_lyrLines.size() > 1) {
        x = m_rcObj.left + m_nXMargin;
    } else {
        x = AUTO_CAL_X;
    }

    if (y < m_rcObj.top + m_nYMargin) {
        y = m_rcObj.top + m_nYMargin;
    }

    for (size_t i = 0; i < m_lyrLines.size(); i++) {
        drawRow(canvas, m_lyrLines[i], x, y, LP_CUR_LINE);
        y += nLineHeight;
    }
}

void CLyricShowObj::setLyricData(CurrentLyrics *pMLData) {
    m_curLyrics = pMLData;
}

// draw lyrics line according to current display style and linePos
//
// set line color according to linePos and display style
//
// 1). Fadeout to low light color
//        a. LP_ABOVE_CUR_LINE :    higlight -> low light
//        b. LP_CUR_LINE :        hilight
//        c. LP_BELOW_CUR_LINE :
//            (i) karaok : Low light
//            (ii)other  : lowlight -> high light
// 2). Fadeout to background
//        a. LP_ABOVE_CUR_LINE :    higlight -> bg
//        b. LP_CUR_LINE :        hilight
//        c. LP_BELOW_CUR_LINE :
//            (i) karaok : bg -> Low light
//            (ii)other  : bg -> high light
// 2). Normal
//        a. LP_ABOVE_CUR_LINE :    lowlight
//        b. LP_CUR_LINE :        hilight
//        c. LP_BELOW_CUR_LINE :    lowlight

enum    LYR_COLOR_TRANSFORM {
    LCT_HIGH_TO_LOW,
    LCT_HIGH_TO_BG,
    LCT_BG_TO_LOW,
    LCT_BG_TO_HIGH,
    LCT_LOW_TO_HIGH,
    LCT_HIGH,
    LCT_LOW,
};

bool CLyricShowObj::drawRow(CRawGraph *canvas, LyricsLine &lyricRow, int x, int y, LinePos linePos) {
    int nSize;
    CColor clrTxt, clrTxtBorder;
    int nAlpha = 255, alphaOld = 255;
    bool bSetOpacityPainting = false;
    LYR_COLOR_TRANSFORM lct;

    // 1). Fadeout to low light color
    //        a. LP_ABOVE_CUR_LINE :    higlight -> low light
    //        b. LP_CUR_LINE :        hilight
    //        c. LP_BELOW_CUR_LINE :
    //            (i) karaok : Low light
    //            (ii)other  : lowlight -> high light
    if (m_LyricsDisplayOpt == DO_FADEOUT_LOWCOLOR) {
        if (linePos == LP_ABOVE_CUR_LINE) {
            lct = LCT_HIGH_TO_LOW;
            nAlpha = getAlpha(lyricRow);
            if (nAlpha == 255) {
                lct = LCT_LOW;
            } else if (nAlpha == 0) {
                lct = LCT_LOW;
            }
        } else if (linePos == LP_BELOW_CUR_LINE) {
            if (isKaraoke()) {
                lct = LCT_LOW;
            } else {
                lct = LCT_LOW_TO_HIGH;
                nAlpha = 255 - getAlpha(lyricRow);
                if (nAlpha == 255) {
                    lct = LCT_LOW;
                } else if (nAlpha == 0) {
                    lct = LCT_LOW;
                }
            }
        } else {
            lct = LCT_HIGH;
        }
    }
    // 2). Fadeout to background
    //        a. LP_ABOVE_CUR_LINE :    higlight -> bg
    //        b. LP_CUR_LINE :        hilight
    //        c. LP_BELOW_CUR_LINE :
    //            (i) karaok : bg -> Low light
    //            (ii)other  : bg -> high light
    else if (m_LyricsDisplayOpt == DO_FADEOUT_BG || m_LyricsDisplayOpt == DO_AUTO) {
        if (linePos == LP_ABOVE_CUR_LINE) {
            lct = LCT_HIGH_TO_BG;
            nAlpha = getAlpha(lyricRow);
            if (nAlpha == 0) {
                return false;
            }
            if (m_pSkin->getEnableTranslucencyLayered() || m_img.isValid()
                || m_tobHilight.obm != OBM_COLOR) {
                bSetOpacityPainting = true;
                lct = LCT_HIGH;
            } else {
                nAlpha = 255 - nAlpha;
            }
        } else if (linePos == LP_BELOW_CUR_LINE) {
            if (isKaraoke()) {
                lct = LCT_BG_TO_LOW;
            } else {
                lct = LCT_BG_TO_HIGH;
            }
            nAlpha = getAlpha(lyricRow);
            if (nAlpha == 0) {
                return false;
            }
            if (m_pSkin->getEnableTranslucencyLayered() || m_img.isValid()
                || m_tobHilight.obm != OBM_COLOR) {
                bSetOpacityPainting = true;
                if (lct == LCT_BG_TO_HIGH) {
                    lct = LCT_HIGH;
                } else {
                    lct = LCT_LOW;
                }
            }
        } else {
            lct = LCT_HIGH;
        }
    }
    // 2). Normal
    //        a. LP_ABOVE_CUR_LINE :    lowlight
    //        b. LP_CUR_LINE :        hilight
    //        c. LP_BELOW_CUR_LINE :    lowlight
    else {
        if (linePos == LP_ABOVE_CUR_LINE) {
            if (isKaraoke()) {
                lct = LCT_HIGH;
            } else {
                lct = LCT_LOW;
            }
        } else if (linePos == LP_BELOW_CUR_LINE) {
            lct = LCT_LOW;
        } else {
            lct = LCT_HIGH;
        }
    }


    switch (lct) {
    case LCT_LOW_TO_HIGH:
        nAlpha = 255 - nAlpha;
    case LCT_HIGH_TO_LOW:
        {
            alphaBlendColor(m_tobLowlight.clr[TCI_BORDER], m_tobHilight.clr[TCI_BORDER], nAlpha, clrTxtBorder);
            if (m_tobHilight.obm == OBM_COLOR) {
                if (m_tobLowlight.obm == OBM_COLOR) {
                    m_font.useColorOverlay();
                    alphaBlendColor(getLowlightColor(), getHighlightColor(), nAlpha, clrTxt);
                } else {
                    m_font.setOverlayPattern(m_tobLowlight.imgPattern, 255 - nAlpha, m_tobHilight.clr[TCI_FILL], nAlpha);
                }
            } else {
                // hilight is image
                if (m_tobLowlight.obm == OBM_COLOR) {
                    m_font.setOverlayPattern(m_tobHilight.imgPattern, nAlpha, m_tobLowlight.clr[TCI_FILL], 255 - nAlpha);
                } else {
                    m_font.setOverlayPattern(m_tobHilight.imgPattern, m_tobLowlight.imgPattern, nAlpha, 255 - nAlpha);
                }
            }
        }
        break;
    case LCT_BG_TO_HIGH:
        nAlpha = 255 - nAlpha;
    case LCT_HIGH_TO_BG:
        {
            alphaBlendColor(m_tobHilight.clr[TCI_BORDER], m_clrBg, nAlpha, clrTxtBorder);
            if (m_tobHilight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
                alphaBlendColor(getHighlightColor(), m_clrBg, nAlpha, clrTxt);
            } else {
                m_font.setOverlayPattern(m_tobHilight.imgPattern, nAlpha, m_clrBg, 255 - nAlpha);
            }
        }
        break;
    case LCT_BG_TO_LOW:
        {
            alphaBlendColor(m_clrBg, m_tobLowlight.clr[TCI_BORDER], nAlpha, clrTxtBorder);
            if (m_tobLowlight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
                alphaBlendColor(m_clrBg, getLowlightColor(), nAlpha, clrTxt);
            } else {
                m_font.setOverlayPattern(m_tobLowlight.imgPattern, nAlpha, m_clrBg, 255 - nAlpha);
            }
        }
        break;
    case LCT_HIGH:
        {
            clrTxtBorder = m_tobHilight.clr[TCI_BORDER];
            if (m_tobHilight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
                clrTxt = getHighlightColor();
            } else {
                m_font.setOverlayPattern(m_tobHilight.imgPattern);
            }
        }
        break;
    case LCT_LOW:
        {
            clrTxtBorder = m_tobLowlight.clr[TCI_BORDER];
            if (m_tobLowlight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
                clrTxt = getLowlightColor();
            } else {
                m_font.setOverlayPattern(m_tobLowlight.imgPattern);
            }
        }
        break;
    }


    if (x == AUTO_CAL_X) {
        x = getLyricRowAlignPos(canvas, lyricRow);
    }

    // 歌词按行缓存：仅对「单一定色」且非描边、非逐帧 setOpacity 的行（LCT_LOW / LCT_HIGH），
    // 把字形栅格化一次进离屏图，每帧只做一次 blt 上屏，避免每帧每字形重跑 drawGlyphRGBA32。
    // 交叉淡出（LCT_HIGH_TO_LOW 等）被排除：两张变体两次 BPM_BLEND 覆盖叠加对不透明字形复算术
    // 不成立（会变「后画覆盖先画」而非混色），必须回退动态路径做单次混色。LCT_BG_* 依赖下层背景也不缓存。
    if (!bSetOpacityPainting && !isOutlineLyrics()) {
        if (lct == LCT_HIGH || lct == LCT_LOW) {
            bool bHilight = (lct == LCT_HIGH);
            int nW = getLyricRowTextWidth(canvas, lyricRow);
            int nH = getFontHeight();
            if (nW > 0 && nH > 0) {
                CLyricRowCell *cell = getRowCacheCell(lyricRow);
                if (cell->nW != nW || cell->nH != nH) {
                    // 行了宽/高变了（换行/字号），重建两张变体
                    delete cell->pImgLow;  cell->pImgLow = nullptr;
                    delete cell->pImgHili; cell->pImgHili = nullptr;
                    cell->nW = nW; cell->nH = nH;
                }

                CRawGraph *&rpImg = bHilight ? cell->pImgHili : cell->pImgLow;
                if (!rpImg) {
                    rpImg = renderRowVariant(lyricRow, bHilight, nW, nH, canvas);
                }

                if (rpImg) {
                    // 单一定色：作透明度 255 的一次混合，与动态 textOut 像素一致。
                    RawImageData *s = rpImg->getRawBuff();
                    canvas->bltImage(canvas->mapAndScaleX(x), canvas->mapAndScaleY(y),
                        s->width, s->height, s, 0, 0, BPM_BLEND);
                    return true;
                }
                // 构建失败：回退下方动态路径
            }
        }
    }

    // 画出此行歌词
    nSize = (int)lyricRow.pieces.size();
    for (LyricsPiece &piece : lyricRow.pieces) {
        if (bSetOpacityPainting) {
            alphaOld = canvas->getOpacityPainting();
            canvas->setOpacityPainting(nAlpha);
        }

        if (isOutlineLyrics()) {
            // draw lyrics text outlined
            canvas->textOutOutlined(x, y + MARGIN_FONT, piece.text, clrTxt, clrTxtBorder);
        } else {
            canvas->setTextColor(clrTxt);
            canvas->textOut(x, y, piece.text);
        }

        if (bSetOpacityPainting) {
            canvas->setOpacityPainting(alphaOld);
        }

        x += getLyricFragDrawWidth(piece, canvas);
    }

    return true;
}

void CLyricShowObj::drawRow(CRawGraph *canvas, LyricsLine &lyricRow, int x, int y, CColor &clrTxt, CColor &clrTxtBorder) {
    int nSize;

    if (x == AUTO_CAL_X) {
        x = getLyricRowAlignPos(canvas, lyricRow);
    }

    // 画出此行歌词
    nSize = (int)lyricRow.pieces.size();
    for (auto &piece : lyricRow.pieces) {

        if (isOutlineLyrics()) {
            // draw lyrics text outlined
            canvas->textOutOutlined(x, y + MARGIN_FONT, piece.text, clrTxt, clrTxtBorder);
        } else {
            canvas->setTextColor(clrTxt);
            canvas->textOut(x, y, piece.text.c_str(), piece.text.size());
        }

        x += getLyricFragDrawWidth(piece, canvas);
    }
}

// COMMENT:
//        逐字显示歌词, like Karaoke
void CLyricShowObj::drawRowKaraoke(CRawGraph *canvas, LyricsLine &lyricRow, int x, int y) {
    assert(m_curLyrics);

    // exp. "Said I love you but I lie"
    //               p^
    // 1. get pCurWord Position
    // 2. draw all words before pCurWord.
    // 3. draw word pCurWord
    // 4. draw words after pCurWords
    // 5. ok

    if (lyricRow.beginTime >= m_curLyrics->getPlayElapsedTime()) {
        drawRow(canvas, lyricRow, x, y, LP_BELOW_CUR_LINE);
        return;
    }

    // 取得该行歌词显示的X座标
    if (x == AUTO_CAL_X) {
        x = getLyricRowAlignPos(canvas, lyricRow);
    }

    // 先将在时间已过的歌词段显示出来
    int nCurPos = 0, nSize = (int)lyricRow.pieces.size();
    for (nCurPos = 0; nCurPos < nSize; nCurPos++) {
        auto &piece = lyricRow.pieces[nCurPos];

        if (piece.endTime >= m_curLyrics->getPlayElapsedTime()) {
            // 时间未过的歌词段
            break;
        } else {
            // 时间已过的歌词段
            //
            // draw lyrics with hilight
            //

            if (m_tobHilight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
            } else {
                m_font.setOverlayPattern(m_tobHilight.imgPattern);
            }

            if (isOutlineLyrics()) {
                // draw lyrics text outlined
                canvas->textOutOutlined(x, y + MARGIN_FONT, piece.text, m_tobHilight.clr[TCI_FILL], m_tobHilight.clr[TCI_BORDER]);
            } else {
                canvas->setTextColor(getHighlightColor());
                canvas->textOut(x, y, piece.text);
            }
            x += getLyricFragDrawWidth(piece, canvas);;
        }
    }

    // 显示当前时间段的歌词
    if (nCurPos < nSize) {
        //    I LOVE YOU        // 1. draw
        //    I lov                // 2. draw ok
        //    I lovE YOU        // 3. result
        int nFragDrawWidth;
        int nKaraokePos = 0;

        auto &piece = lyricRow.pieces[nCurPos];

        // get width of the lyrics fragment
        nFragDrawWidth = getLyricFragDrawWidth(piece, canvas);

        if (piece.beginTime < m_curLyrics->getPlayElapsedTime()) {
            // 如果时间 m_curLyrics->getPlayElapsedTime() 在当前歌词段之间
            //    |1word2|   |1word2|
            //                  ^ piece
            int dt = (piece.endTime - piece.beginTime);
            if (dt != 0) {

                nKaraokePos = nFragDrawWidth * (m_curLyrics->getPlayElapsedTime() - piece.beginTime) / dt;
                // rcText.setLTRB(x, y, x + nKaraokePos + 1, y + getFontHeight() + 1);
            }
        } else {
            //    |1word2|   |1word2|
            //             ^  piece
            nKaraokePos = nFragDrawWidth;
        }

        CRect rcText;

        // draw lowlight part lyrics first
        if (nKaraokePos != nFragDrawWidth) {
            rcText.setLTWH(x, y, nFragDrawWidth + 1000, getFontHeight());

            if (m_tobLowlight.obm == OBM_COLOR) {
                m_font.useColorOverlay();
            } else {
                m_font.setOverlayPattern(m_tobLowlight.imgPattern);
            }

            if (isOutlineLyrics()) {
                canvas->drawTextClipOutlined(piece.text, rcText, m_tobLowlight.clr[TCI_FILL], m_tobLowlight.clr[TCI_BORDER], nKaraokePos);
            } else {
                canvas->setTextColor(getLowlightColor());
                canvas->drawTextClip(piece.text, rcText, nKaraokePos);
            }
        }

        // draw hilight part lyrics
        rcText.setLTWH(x, y, nKaraokePos, getFontHeight());

        if (m_tobHilight.obm == OBM_COLOR) {
            m_font.useColorOverlay();
        } else {
            m_font.setOverlayPattern(m_tobHilight.imgPattern);
        }

        if (isOutlineLyrics()) {
            // draw lyrics text outlined
            canvas->drawTextClipOutlined(piece.text, rcText, m_tobHilight.clr[TCI_FILL], m_tobHilight.clr[TCI_BORDER]);
        } else {
            canvas->setTextColor(getHighlightColor());
            canvas->drawTextClip(piece.text, rcText);
        }

        // 增加 x 的偏移位置
        x += nFragDrawWidth;
        nCurPos++;
    }

    if (m_tobLowlight.obm == OBM_COLOR) {
        m_font.useColorOverlay();
    } else {
        m_font.setOverlayPattern(m_tobLowlight.imgPattern);
    }

    // 显示时间未到的歌词段
    for (; nCurPos < nSize; nCurPos++) {
        auto &piece = lyricRow.pieces[nCurPos];

        // draw text border
        if (isOutlineLyrics()) {
            // draw lyrics text outlined
            canvas->textOutOutlined(x, y + MARGIN_FONT, piece.text, m_tobLowlight.clr[TCI_FILL], m_tobLowlight.clr[TCI_BORDER]);
        } else {
            canvas->setTextColor(getLowlightColor());
            canvas->textOut(x, y, piece.text);
        }
        x += getLyricFragDrawWidth(piece, canvas);
    }
    // 5. ok
}

void CLyricShowObj::drawCurrentRow(CRawGraph *canvas, LyricsLine &lyricRow, int x, int y) {
    assert(m_curLyrics);
    assert(canvas);

    if (isKaraoke()) {
        drawRowKaraoke(canvas, lyricRow, x, y);
    } else {
        drawRow(canvas, lyricRow, x, y, LP_CUR_LINE);
    }
}

void CLyricShowObj::fadeOutVertBorder(CRawGraph *canvas, int yDrawLyrStartPos, int yDrawLyrEndPos) {
    if (m_pSkin->m_nCurTranslucencyAlpha > 0) {
        if (m_bSetSkinBg && m_pContainer->isDrawBgImage()) {
            return;
        }

        if (m_img.isValid()) {
            return;
        }
    }

    CColor clrBg;
    if (m_bSetSkinBg) {
        clrBg = m_pContainer->getBgColor();
    } else {
        clrBg = m_clrBg;
    }

    CRect rc;
    rc.setLTRB(m_rcObj.left, m_rcObj.top, m_rcObj.right, m_rcObj.top + m_nFontHeight);
    if (rc.bottom > yDrawLyrStartPos) {
        if (m_pSkin->getEnableTranslucencyLayered() && m_pSkin->m_nCurTranslucencyAlpha <= 0) {
            canvas->vertAlphaFadeOut(rc, true);
        } else {
            canvas->vertFadeOut(rc, clrBg, true);
        }
    }

    rc.bottom = m_rcObj.bottom;
    rc.top = m_rcObj.bottom - m_nFontHeight;
    if (rc.top < yDrawLyrEndPos) {
        if (m_pSkin->getEnableTranslucencyLayered() && m_pSkin->m_nCurTranslucencyAlpha <= 0) {
            canvas->vertAlphaFadeOut(rc, false);
        } else {
            canvas->vertFadeOut(rc, clrBg, false);
        }
    }
}

// COMMENT:
//        根据当前的 Graphics 环境，取得歌词 lyricRow 行的宽度
int CLyricShowObj::getLyricRowTextWidth(CRawGraph *canvas, LyricsLine &lyricRow) {
    assert(m_curLyrics);

    int nTxtWidth = 0;
    for (auto &piece : lyricRow.pieces) {
        nTxtWidth += getLyricFragDrawWidth(piece, canvas);
    }

    return nTxtWidth;
}

void CLyricShowObj::updateLyricDrawBufferBackground(CRawGraph *canvas, CRect &rc) {
    if (m_img.isValid()) {
        if (m_bBgCacheDirty) {
            // 背景失效（换图/改尺寸等）时重建离屏缓存；失败则走原动态路径兜底。
            if (!rebuildBgCache(canvas)) {
                m_bBgCacheDirty = false;
            }
        }

        if (m_pBgCache) {
            drawBgCache(canvas);
        } else {
            m_img.tileBltEx(canvas, m_rcObj.left, m_rcObj.top, rc.left, rc.top, rc.width(), rc.height());
            // m_img.blt(canvas, m_rcObj.left, m_rcObj.top);
        }

        if (m_bDarkenLyrBgOnImg) {
            darkenLyricsBg(canvas, rc);
        }
    } else if (m_bSetSkinBg) {
        // The background has been drawed by parent skin container
    } else {
        canvas->fillRect(rc, m_clrBg);
    }
}

// 将 m_img 平铺进一个透明离屏缓冲，平铺相位与窗口坐标对齐，避免滚动时每帧重算 tile。
// 透明缓冲 + 混合上屏：因为 premultiplied alpha 混合可结合，结果与动态路径像素一致。
bool CLyricShowObj::rebuildBgCache(CRawGraph *canvas) {
    int w = m_rcObj.width(), h = m_rcObj.height();
    if (w <= 0 || h <= 0 || !m_img.isValid()) {
        m_bBgCacheDirty = false;
        return false;
    }

    // create() 接收逻辑尺寸并内部按 scale 放大；getRawBuff() 返回物理尺寸。
    if (!m_pBgCache || m_nBgCacheW != int(w * canvas->getScaleFactor())
        || m_nBgCacheH != int(h * canvas->getScaleFactor())) {
        delete m_pBgCache;

        CRawGraph *pCache = new CRawGraph(canvas->getScaleFactor());
        // 离屏缓存不绘制到窗口，windowHandle 传 0 即可（内存缓冲不依赖窗口句柄）。
        if (!pCache->create(w, h, (WindowHandle)0, 32)) {
            delete pCache;
            m_pBgCache = nullptr;
            m_bBgCacheDirty = false;
            return false;
        }

        RawImageData *rd = pCache->getRawBuff();
        m_nBgCacheW = rd->width;
        m_nBgCacheH = rd->height;
        m_pBgCache = pCache;
    }

    // 清为透明
    m_pBgCache->fillRect(CRect(0, 0, w, h), CColor(0, 0), BPM_COPY);

    // 用与动态路径相同的相位（xOrg/yOrg = m_rcObj 左上角）把图贴进缓存。
    m_img.tileBltEx(m_pBgCache, m_rcObj.left, m_rcObj.top, 0, 0, w, h);

    m_bBgCacheDirty = false;

    return true;
}

void CLyricShowObj::drawBgCache(CRawGraph *canvas) {
    if (!m_pBgCache) {
        return;
    }

    RawImageData *src = m_pBgCache->getRawBuff();

    // bltImage 的坐标是 scale 后（物理）坐标；cache 与 canvas 同 scale，像素 1:1。
    canvas->bltImage(canvas->mapAndScaleX(m_rcObj.left), canvas->mapAndScaleY(m_rcObj.top),
        src->width, src->height, src, 0, 0, BPM_BLEND);
}

#define LYRIC_ROW_CACHE_CAP        24          // 可见行缓存上限（LRU 淘汰）

// 由歌词行取（或新建）行缓存条目。按 LyricsLine* 索引；播放中 m_lyrLines 不增删，指针稳定。
CLyricShowObj::CLyricRowCell *CLyricShowObj::getRowCacheCell(LyricsLine &lyricRow) {
    for (auto &cell : m_vRowCache) {
        if (cell.pLine == &lyricRow) {
            cell.nLastUse = ++m_nRowCacheUseTick;
            return &cell;
        }
    }

    if ((int)m_vRowCache.size() >= LYRIC_ROW_CACHE_CAP) {
        // LRU 淘汰最久未用的一行
        auto itMin = m_vRowCache.begin();
        for (auto it = m_vRowCache.begin(); it != m_vRowCache.end(); ++it) {
            if (it->nLastUse < itMin->nLastUse) {
                itMin = it;
            }
        }
        delete itMin->pImgLow;
        delete itMin->pImgHili;
        m_vRowCache.erase(itMin);
    }

    CLyricRowCell cell;
    cell.pLine = &lyricRow;
    cell.nStamp = m_nRowCacheStamp;
    cell.nLastUse = ++m_nRowCacheUseTick;
    m_vRowCache.push_back(cell);
    return &m_vRowCache.back();
}

// 把一行的字形栅格化一次进离屏图（透明底）。同一行的 high/low 变体复用本函数、仅换颜色/图案，
// 保证两张图覆盖形状逐像素一致，交叉叠加才不重影。仅用于非描边文本。
CRawGraph *CLyricShowObj::renderRowVariant(LyricsLine &lyricRow, bool bHilight, int nW, int nH, CRawGraph *canvas) {
    CRawGraph *pImg = new CRawGraph(canvas->getScaleFactor());
    if (!pImg->create(nW, nH, (WindowHandle)0, 32)) {
        delete pImg;
        return nullptr;
    }

    pImg->fillRect(CRect(0, 0, nW, nH), CColor(0, 0), BPM_COPY);
    pImg->setFont(&m_font);

    const TextOverlayBlending &tob = bHilight ? m_tobHilight : m_tobLowlight;
    CColor clrFill = bHilight ? getHighlightColor() : getLowlightColor();

    if (tob.obm == OBM_COLOR) {
        m_font.useColorOverlay();
    } else {
        m_font.setOverlayPattern(tob.imgPattern);
        clrFill = tob.clr[TCI_FILL];
    }

    // textOut 的 y 是行顶（字形向下占 getFontHeight），baseline=0 渲染并与 blit 位置一致。
    int x = 0;
    for (auto &piece : lyricRow.pieces) {
        pImg->setTextColor(clrFill);
        pImg->textOut((float)x, 0, piece.text);
        x += getLyricFragDrawWidth(piece, pImg);
    }

    return pImg;
}

void CLyricShowObj::invalidateRowCache() {
    for (auto &cell : m_vRowCache) {
        delete cell.pImgLow;
        delete cell.pImgHili;
    }
    m_vRowCache.clear();
    m_nRowCacheStamp++;
}

void CLyricShowObj::darkenLyricsBg(CRawGraph *canvas, CRect &rc) {
    int nLineHeight = getLineHeight();

    auto nDarkenTop = getLineVertAlignPos() - int(nLineHeight * m_nDarkenTopArea);
    auto nDarkenBottom = getLineVertAlignPos() + int(nLineHeight * m_nDarkenBottomArea);
    auto nFillBottom = min((int)rc.bottom, nDarkenBottom);
    auto nFillTop = max((int)rc.top, nDarkenTop);
    if (nFillBottom > nFillTop) {
        canvas->fillRect(rc.left, nFillTop, rc.width(), nFillBottom - nFillTop, m_clrDarken, BPM_BLEND);
    }
}

void CLyricShowObj::setAntialise(bool bAntialis) {
    onLyrDrawContextChanged();
}

// COMMENT:
//        取得该行歌词显示的X座标
// INPUT:
//        canvas        -    显示歌词行的设备
//        lyricRow -    歌词行
// OUTPUT:
//        输出此行歌词显示的 X 座标位置
int CLyricShowObj::getLyricRowAlignPos(CRawGraph *canvas, LyricsLine &lyricRow) {
    assert(m_curLyrics);

    int x;
    switch (m_nAlignment) {
    case LA_LEFT:
        // 居左
        // 计算左边显示的位置
        x = m_rcObj.left + m_nXMargin;
        break;
    case LA_CENTER:
    default:
        // 居中
        {
            int nTxtWidth;
            // 取得此行歌词的显示宽度
            nTxtWidth = getLyricRowTextWidth(canvas, lyricRow);

            // 计算左边显示的位置
            x = m_rcObj.left + (m_rcObj.width() - nTxtWidth) / 2;
        }
        break;
    case LA_RIGHT:
        // 居右
        {
            int nTxtWidth;
            // 取得此行歌词的显示宽度
            nTxtWidth = getLyricRowTextWidth(canvas, lyricRow);

            // 计算左边显示的位置
            x = m_rcObj.right - nTxtWidth - m_nXMargin;
        }
        break;
    }
    return x;
}

bool CLyricShowObj::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    assert(m_curLyrics);

    if (nFlags != 0) {
        return false;
    }

    switch (nChar) {
    case VK_UP:
        m_curLyrics->setOffsetTime(m_curLyrics->getOffsetTime() - SET_SPEED_SPAN);
        MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);
        break;
    case VK_DOWN:
        m_curLyrics->setOffsetTime(m_curLyrics->getOffsetTime() + SET_SPEED_SPAN);
        MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);
        break;
    case 'D':
        m_pSkin->postShortcutKeyCmd(ID_OPEN_LRC);
        break;
    case 'Z':
        g_player.prev();
        break;
    case 'X':
        g_player.play();
        break;
    case 'C':
        g_player.playPause();
        break;
    case 'V':
        g_player.stop();
        break;
    case 'B':
        g_player.next();
        break;
    case VK_RIGHT:
        // 前进5秒
        {
            uint32_t dwCurPos;
            uint32_t dwLength;

            dwCurPos = g_player.getPlayPos();
            dwCurPos += 5000;
            dwLength = g_player.getMediaLength();
            if (dwCurPos > dwLength) {
                dwCurPos = dwLength;
            }
            g_player.seekTo(dwCurPos);
        }
        break;
    case VK_LEFT:
        // 后退5秒
        {
            int dwCurPos;

            dwCurPos = g_player.getPlayPos();
            dwCurPos -= 5000;
            if (dwCurPos < 0) {
                dwCurPos = 0;
            }
            g_player.seekTo(dwCurPos);
        }
        break;
    default:
        return false;
    }
    return true;
}

bool CLyricShowObj::onMouseDrag(CPoint point) {
    if (m_yBeginDrag != -1) {
        int nPosStart;
        int nHeight = m_rcObj.height();

        if (point.y < m_rcObj.top + nHeight / 4) {
            m_vertLineAlign = VLA_TOP;
        } else if (point.y > m_rcObj.bottom - nHeight / 4) {
            m_vertLineAlign = VLA_BOTTOM;
        } else {
            m_vertLineAlign = VLA_CENTER;
        }

        if (m_vertLineAlign == VLA_TOP) {
            nPosStart = m_rcObj.top;
        } else if (m_vertLineAlign == VLA_BOTTOM) {
            nPosStart = m_rcObj.bottom;
        } else {
            nPosStart = m_rcObj.top + nHeight / 2;
        }

        m_nVertLineOffset = point.y - nPosStart;
        // assert(getLineVertAlignPos() == point.y);

        setCursor(m_curAdjustVertAlign);

        invalidate();
        return true;
    } else if (m_bEnableAdjustVertAlign && m_bEnableAdjustVertAlignUser) {
        int nPos;

        nPos = getLineVertAlignPos();
        if (point.y >= nPos - m_nFontHeight && point.y <= nPos + m_nFontHeight
            && m_rcObj.height() >= m_nFontHeight * 3) {
            setCursor(m_curAdjustVertAlign);
            return true;
        }
    }

    return false;
}

bool CLyricShowObj::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (m_yBeginDrag != -1) {
        m_yBeginDrag = -1;
        setCursor(m_curAdjustVertAlign);
        m_pSkin->releaseCaptureMouse(this);

        g_profile.writeInt(m_strSectName.c_str(), "VertAlign", m_vertLineAlign);
        g_profile.writeInt(m_strSectName.c_str(), "VertAlignOffset", m_nVertLineOffset);
        return true;
    }

    return false;
}

bool CLyricShowObj::onLButtonDown(uint32_t nFlags, CPoint point) {
    if (m_bEnableAdjustVertAlign && m_bEnableAdjustVertAlignUser) {
        int nPos;

        nPos = getLineVertAlignPos();
        if (point.y >= nPos - m_nFontHeight && point.y <= nPos + m_nFontHeight
            && m_rcObj.height() >= m_nFontHeight * 3) {
            setCursor(m_curAdjustVertAlign);
            m_pSkin->setCaptureMouse(this);
            m_yBeginDrag = point.y;
            return true;
        }
    }

    return false;
}

void CLyricShowObj::onMagnify(float magnification) {
    m_pSkin->postShortcutKeyCmd(magnification >= 0 ? ID_FONT_SIZE_INC : ID_FONT_SIZE_DEC);
}

void CLyricShowObj::onLyricsChanged() {
    if ((m_bEnableAutoResize && g_profile.getBool(SZ_SECT_UI, "AutoAdjustWndWidth", false))
        || !m_bCanWrapLines) {
        // auto resize || !wrap lines
        m_lyrLines = m_curLyrics->getLyricsLines();
        reverseLyricsForRightToLeftLanguage();
    } else {
        wrapLyricsLines();
    }

    onLyrDrawContextChanged();

    invalidate();
}

void CLyricShowObj::onLyrDrawContextChanged() {
    m_bBgCacheDirty = true;
    invalidateRowCache();       // 歌词/字体/颜色等变化：行缓存全部失效

    m_lyrLines.clearDrawContextWidth();

    if (m_bCanWrapLines) {
        if (!autoWidthSkinAccordLyrics()) {
            wrapLyricsLines();
        }
    }

    // Auto height.
    autoHeightSkinAccordLyrics(getAutoHeightLines());
}

void adjustColorHue(CColor &clr, float hueOffset) {
    COLORREF c = clr.get();
    adjustColorHue(c, hueOffset);
    clr.set(c);
}

void CLyricShowObj::onAdjustHue(float hue, float saturation, float luminance) {
    if (m_hue != 0) {
        // load original settings.
        setBgColor(g_profile.getString(m_strSectName.c_str(), "BgColor", ""));

        loadLyrOverlaySettings();
    }

    m_hue = hue;

    if (hue == 0) {
        return;
    }

    // Adjust hue
    adjustColorHue(m_clrBg, hue);

    if (m_pSkin && m_bSetSkinBg) {
        m_pSkin->setProperty("BgColor", colorToStr(m_clrBg).c_str());
    }

    for (TOBColorIndex i = TCI_START; i < TCI_COUNT; i = (TOBColorIndex)(i + 1)) {
        adjustColorHue(m_tobHilight.clr[i], hue);
        adjustColorHue(m_tobLowlight.clr[i], hue);
    }

    if (m_tobHilight.imgPattern) {
        adjustImageHue(m_tobHilight.imgPattern.get(), hue);
    }

    if (m_tobLowlight.imgPattern) {
        adjustImageHue(m_tobLowlight.imgPattern.get(), hue);
    }
}

void CLyricShowObj::wrapLyricsLines() {
    CRawGraph *canvas;

    canvas = m_pContainer->getMemGraph();

    canvas->setFont(&m_font);

    bool bVerticalStyle = (isKindOf(CLyricShowMultiRowObj::className()) || isKindOf(CLyricShowTxtObj::className()));

    int width = m_rcObj.width() - m_nXMargin * 2 - getOutlineMargin();
    wrapDisplayLyrics(g_currentLyrics.getLyricsLines(), m_lyrLines, canvas, width, bVerticalStyle);

    reverseLyricsForRightToLeftLanguage();
}

// COMMENT:
// 自动调整迷窗口高度
void CLyricShowObj::autoHeightSkinAccordLyrics(int nLines) {
    if (!m_bEnableAutoResize || nLines <= 0) {
        return;
    }

    if (m_pSkin->m_wndResizer.isFixedHeight()) {
        return;
    }

    if (!g_profile.getBool(SZ_SECT_UI, "AutoAdjustWndHeight", true)) {
        return;
    }

    CRect rc;
    int nHeightOthers = 0;
    int nBottomOld;

    m_pSkin->getWindowRect(&rc);
    nHeightOthers = rc.height() - m_rcObj.height();
    nBottomOld = rc.bottom;

    rc.bottom = rc.top + nHeightOthers + m_nYMargin * 2 + abs(getLineHeight() * nLines) + m_nLineSpacing * (nLines - 1);
    if (nBottomOld != rc.bottom) {
        m_pSkin->moveWindow(rc);
    }
}

// COMMENT:
//        根据歌词的长度，自动调整窗口宽度可以匹配显示歌词
bool CLyricShowObj::autoWidthSkinAccordLyrics() {
    CRect rc, rcOrg;
    int nWidthMax = 200;

    if (!m_curLyrics || !m_bEnableAutoResize || m_pSkin->m_wndResizer.isFixedWidth()) {
        return false;
    }

    if (m_lyrLines.empty()) {
        return false;
    }

    if (!g_profile.getBool(SZ_SECT_UI, "AutoAdjustWndWidth", false)) {
        return false;
    }

    CRawGraph *canvas;

    canvas = m_pContainer->getMemGraph();

    canvas->setFont(&m_font);

    //
    // 找到最宽的一行歌词
    m_pSkin->getWindowRect(&rc);
    rcOrg = rc;
    for (int i = 0; i < (int)m_lyrLines.size(); i++) {
        int w = getLyricRowTextWidth(canvas, m_lyrLines[i]);
        if (w > nWidthMax) {
            nWidthMax = w;
        }
    }

    nWidthMax += m_nXMargin * 2 + 8;

    //
    // 调整宽带到可以显示最宽的一行歌词
    //    if (rc.right != rc.left + rc.width() - m_rcObj.width() + nWidthMax + m_nXMargin * 2 + 40)
    {
        CRect rcRestrict;

        getMonitorRestrictRect(rcOrg, rcRestrict);

        if (m_nLeftAdjust != rc.left) {
            m_nLeftOrg = rc.left;
        } else {
            rc.right -= rc.left - m_nLeftOrg;
            rc.left = m_nLeftOrg;
        }

        rc.right = rc.left + rc.width() - m_rcObj.width() + nWidthMax;

        if (rc.width() < m_pSkin->m_wndResizer.getMinCx()) {
            rc.right = rc.left + m_pSkin->m_wndResizer.getMinCx();
        }

        // Max width is the width of screen
        if (rc.width() > rcRestrict.width()) {
            rc.right = rc.left + rcRestrict.width();
        }

        // floating lyrics and translucency? align the skin window to center.
        if (m_etDispSettings == ET_LYRICS_FLOATING_SETTINGS
            && m_nAlignment == LA_CENTER
            // rcOrg is in center of rcRestrict?
            && abs((rcRestrict.right + rcRestrict.left) / 2 - (rcOrg.right + rcOrg.left) / 2) < 200) {
            int nOffset;

            nOffset = (rcRestrict.right + rcRestrict.left) / 2 - (rc.right + rc.left) / 2;
            rc.left += nOffset;
            rc.right += nOffset;
        }

        // Out of right border of screen?
        if (rc.right > rcRestrict.right) {
            rc.left -= rc.right - rcRestrict.right;
            rc.right -= rc.right - rcRestrict.right;
        }

        // Out of left border of screen?
        if (rc.left < rcRestrict.left) {
            rc.right += rcRestrict.left - rc.left;
            rc.left += rcRestrict.left - rc.left;
        }

        m_nLeftAdjust = rc.left;

        // NOT changed
        if (rc.width() <= rcOrg.width() + 4 && rc.width() >= rcOrg.width() - 50
            && rcOrg.left >= rcRestrict.left && rcOrg.right <= rcRestrict.right) {
            return true;
        }

        m_pSkin->moveWindow(rc.left, rc.top, rc.width(), rc.height(), true);
    }
    return true;
}

void CLyricShowObj::onPlayTimeChangedUpdate() {
    if (m_pSkin->isIconic() || !isVisible() || !isParentVisible()) {
        return;
    }

    CRawGraph *canvas = m_pContainer->getMemGraph();
    canvas->setFont(&m_font);

    // 设置绘制区域为空，不进行实际的绘制
    const CRect empty;
    canvas->setClipBoundBox(empty);

    // 缺省需要更新的区域为 m_rcObject 去掉 margin
    CRect rcUpdate = getClipRect();

    // 调用 fastDraw 获取需要更新的区域
    fastDraw(canvas, &rcUpdate);

    if (!rcUpdate.empty()) {
        m_pSkin->invalidateRect(&rcUpdate);
    }
}

int CLyricShowObj::getAlpha(LyricsLine &line) {
    int nTime;
    const int FADE_DURATION = 1500;

    if (line.beginTime > m_curLyrics->getPlayElapsedTime()) {
        nTime = line.beginTime - m_curLyrics->getPlayElapsedTime();
        if (nTime <= FADE_DURATION && nTime >= -FADE_DURATION) {
            return (FADE_DURATION - nTime) * 255 / FADE_DURATION;
        } else {
            return 0;
        }
    } else {
        nTime = m_curLyrics->getPlayElapsedTime() - line.endTime;
        if (nTime >= FADE_DURATION) {
            return 0;
        } else if (nTime > 0) {
            return (FADE_DURATION - nTime) * 255 / FADE_DURATION;
        } else {
            return 255;
        }
    }
}

void CLyricShowObj::loadBgImageFolder() {
    m_vPicFiles.clear();
    m_nextPicInFolder = 0;

    if (m_strBgPicFolder.empty()) {
        return;
    }

    dirStringAddSep(m_strBgPicFolder);

    FileFind finder;
    static cstr_t szSupportedPicExt[] = { ".jpg", ".gif", ".bmp", ".tif", ".png" };

    if (!finder.openDir(m_strBgPicFolder.c_str())) {
        return;
    }

    while (finder.findNext()) {
        for (int i = 0; i < CountOf(szSupportedPicExt); i++) {
            if (fileIsExtSame(finder.getCurName(), szSupportedPicExt[i])) {
                m_vPicFiles.push_back(m_strBgPicFolder + finder.getCurName());
            }
        }
    }
}

void CLyricShowObj::loadNextBgImage() {
    if (!m_bUseAlbumArtAsBg && !m_bUseBgImg) {
        m_img.detach();
        return;
    }

    RawImageDataPtr image;
    if (m_bUseAlbumArtAsBg) {
        image = m_curAlbumArt.loadNext();
    }

    if (!image && m_bUseBgImg && !m_vPicFiles.empty()) {
        int index = m_nextPicInFolder % m_vPicFiles.size();
        image = loadRawImageDataFromFile(m_vPicFiles[index].c_str());

        m_nextPicInFolder = index + 1;
        if (m_nextPicInFolder == m_vPicFiles.size()) {
            // 重新循环展示
            m_curAlbumArt.restartLoop();
            m_nextPicInFolder = 0;
        }
    }

    if (image) {
        if (m_img.isValid()) {
            m_img.detach();
        }

        image = autoFitImageToRect(image, m_rcObj.width(), m_rcObj.height(), image->bitCount);
        if (image) {
            m_img.attach(image);
        }
    }
}


void CLyricShowObj::onTimer(int nId) {
    loadNextBgImage();
    invalidate();
}

void CLyricShowObj::onSize() {
    CUIObject::onSize();

    m_bBgCacheDirty = true;
    invalidateRowCache();       // 尺寸/scale 变化：字形尺寸可能变，行缓存全部失效

    if (m_bUseBgImg || m_bUseAlbumArtAsBg) {
        loadNextBgImage();
    }

    if (m_bCanWrapLines) {
        wrapLyricsLines();
    }
}

int CLyricShowObj::getLineVertAlignPos() {
    int nPos;

    if (m_vertLineAlign == VLA_TOP) {
        nPos = m_rcObj.top;
    } else if (m_vertLineAlign == VLA_BOTTOM) {
        nPos = m_rcObj.bottom;
    } else {
        nPos = m_rcObj.top + m_rcObj.height() / 2;
    }

    nPos += m_nVertLineOffset;

    if (nPos <= m_rcObj.top + m_nYMargin + 5 || nPos >= m_rcObj.bottom - m_nYMargin - getFontHeight()) {
        nPos = m_rcObj.top + m_rcObj.height() / 2;
    }

    return nPos;
}

bool CLyricShowObj::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (isPropertyName(szProperty, "UseSkinStyle")) {
        m_bUseSkinStyle = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "LyrAlign") == 0) {
        setLyrAlign(szValue);
    } else if (strcasecmp(szProperty, "LyrDrawOpt") == 0) {
        m_LyricsDisplayOpt = displayOptFromStr(szValue);
    } else if (strcasecmp(szProperty, "BgColor") == 0) {
        setBgColor(szValue);
    } else if (strcasecmp(szProperty, "SetSkinBg") == 0) {
        m_bSetSkinBg = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "UseBgImage") == 0) {
        m_bUseBgImg = isTRUE(szValue);
        m_pSkin->unregisterTimerObject(this);
        if (m_bUseBgImg) {
            loadBgImageFolder();
            loadNextBgImage();
            m_pSkin->registerTimerObject(this, g_profile.getInt(m_strSectName.c_str(), "SlideDelayTime", 10) * 1000);
        } else if (!m_bUseAlbumArtAsBg) {
            m_img.detach();
            m_vPicFiles.clear();
        }
    } else if (strcasecmp(szProperty, "BgPicFolder") == 0) {
        if (strcmp(szValue, m_strBgPicFolder.c_str()) != 0) {
            m_strBgPicFolder = szValue;
            updateBgImage();
        }
    } else if (isPropertyName(szProperty, "UseAlbumArtAsBg")) {
        m_bUseAlbumArtAsBg = isTRUE(szValue);
        updateBgImage();
    } else if (isPropertyName(szProperty, "DarkenLyrBg")) {
        m_bDarkenLyrBgOnImg = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "SlideDelayTime")) {
        if (m_bUseBgImg) {
            m_pSkin->unregisterTimerObject(this);
            m_pSkin->registerTimerObject(this, atoi(szValue) * 1000);
        }
    } else if (strcasecmp(szProperty, "LineSpacing") == 0) {
        m_nLineSpacing = atoi(szValue);
    } else if (strcasecmp(szProperty, "Font") == 0) {
        FontInfoEx info;
        if (info.parse(szValue)) {
            m_font.create(info, m_pSkin->getScaleFactor());
            m_nFontHeight = m_font.getHeight();

            if (m_tobHilight.imgPattern &&
                    m_tobHilight.imgPattern->height != getPatternFontHeight()) {
                if (m_hue == 0) {
                    loadLyrOverlaySettings();
                } else {
                    onAdjustHue(m_hue, 0, 0);
                }
            }

            onLyrDrawContextChanged();
        }
    } else if (isPropertyName(szProperty, "OutlineLyrText")) {
        m_bOutlineLyrics = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "Karaoke")) {
        m_bKaraoke = isTRUE(szValue);
    } else if (strcasecmp(szProperty, "EnableAutoResize") == 0) {
        m_bEnableAutoResize = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "enableAdjustVertAlign")) {
        m_bEnableAdjustVertAlign = isTRUE(szValue);
    } else if (isPropertyName(szProperty, "XMargin")) {
        m_nXMargin = atoi(szValue);
    } else {
        TOBColorIndex i;

        for (i = TCI_START; i < TCI_COUNT; i = (TOBColorIndex)(i + 1)) {
            if (isPropertyName(szProperty, _LyrShowTOBFieldNameHilight[i])) {
                getColorValue(m_tobHilight.clr[i], szValue);
                break;
            } else if (isPropertyName(szProperty, _LyrShowTOBFieldNameLowlight[i])) {
                getColorValue(m_tobLowlight.clr[i], szValue);
                break;
            }
        }

        // Not processed?
        if (i == TCI_COUNT) {
            return false;
        }
    }

    return true;
}

bool CLyricShowObj::onLyrDisplaySettings(cstr_t szProperty, cstr_t szValue) {
    if (m_bUseSkinStyle) {
        return true;
    }

    if (isPropertyName(szProperty, "BgColor")) {
        setBgColor(szValue);
        m_pSkin->invalidateRect();
        return true;
    } else if (isPropertyName(szProperty, "OutlineLyrText")) {
        m_bOutlineLyrics = isTRUE(szValue);
    }
    //
    // High light of lyrics overlay blending settings
    //
    else if (isPropertyName(szProperty, "HilightOBM")) {
        loadLOBHilightSettings();
    } else if (isPropertyName(szProperty, "HilightPattern")) {
        if (m_tobHilight.obm == OBM_PATTERN) {
            RawImageDataPtr image = loadRawImageDataPatternFile(szValue);
            if (image && image->bitCount != 24) {
                image = convertTo24BppRawImage(image);
            }

            m_tobHilight.imgPattern = image;
        }
    }
    //
    // Low light of lyrics overlay blending settings
    //
    else if (isPropertyName(szProperty, "LowlightOBM")) {
        loadLOBLowlightSettings();
    } else if (isPropertyName(szProperty, "LowlightPattern")) {
        if (m_tobLowlight.obm == OBM_PATTERN) {
            RawImageDataPtr image = loadRawImageDataPatternFile(szValue);
            if (image && image->bitCount != 24) {
                image = convertTo24BppRawImage(image);
            }

            m_tobLowlight.imgPattern = image;
        }
    } else if (isPropertyName(szProperty, "enableAdjustVertAlign")) {
        m_bEnableAdjustVertAlignUser = isTRUE(szValue);
    } else {
        TOBColorIndex i;

        //
        // Highlight color settings.
        //
        for (i = TCI_START; i < TCI_COUNT; i = (TOBColorIndex)(i + 1)) {
            if (isPropertyName(szProperty, _LyrShowTOBFieldNameHilight[i])) {
                getColorValue(m_tobHilight.clr[i], szValue);
                break;
            }
        }
        if ((i == TCI_GRADIENT1 || i == TCI_GRADIENT2 || i == TCI_GRADIENT3)) {
            createLyrOverlayBlendingPattern(m_tobHilight, getPatternFontHeight());
        } else if (i == TCI_COUNT) {
            //
            // Lowlight color settings.
            //
            for (i = TCI_START; i < TCI_COUNT; i = (TOBColorIndex)(i + 1)) {
                if (isPropertyName(szProperty, _LyrShowTOBFieldNameLowlight[i])) {
                    getColorValue(m_tobLowlight.clr[i], szValue);
                    break;
                }
            }
            if ((i == TCI_GRADIENT1 || i == TCI_GRADIENT2 || i == TCI_GRADIENT3)) {
                createLyrOverlayBlendingPattern(m_tobLowlight, getPatternFontHeight());
            } else if (i == TCI_COUNT) {
                //
                // Got default processing
                //
                if (!setProperty(szProperty, szValue)) {
                    return false;
                }
            }
        }
    }

    invalidate();

    return true;
}

void CLyricShowObj::updateBgImage() {
    m_bBgCacheDirty = true;

    if (m_img.isValid()) {
        m_img.detach();
    }

    m_curAlbumArt.reset();

    if (m_bUseBgImg || m_bUseAlbumArtAsBg) {
        loadNextBgImage();
    }
}

int CLyricShowObj::getPatternFontHeight() const {
    return (m_nFontHeight + MARGIN_FONT * 2) * m_pSkin->getScaleFactor();
}

void CLyricShowObj::setLyrAlign(cstr_t szValue) {
    static IdToString    _id2strLyrAlign[] = {
        { LA_LEFT, "left" },
        { LA_CENTER, "center" },
        { LA_RIGHT, "right" },
        { 0, nullptr },
    };
    m_nAlignment = (LyrAlignment)stringToID(_id2strLyrAlign, szValue, LA_CENTER);
}

void CLyricShowObj::setBgColor(cstr_t szValue) {
    if (isEmptyString(szValue)) {
        return;
    }

    getColorValue(m_clrBg, szValue);

    if (m_pSkin && m_bSetSkinBg) {
        m_pSkin->setProperty("BgColor", szValue);
    }
}

void CLyricShowObj::loadLyrOverlaySettings() {
    m_bOutlineLyrics = g_profile.getBool(m_strSectName.c_str(), "OutlineLyrText", m_bOutlineLyrics);

    profileGetColorValue(m_tobHilight.clr[TCI_BORDER], m_strSectName.c_str(), "HilightBorderColor");
    profileGetColorValue(m_tobLowlight.clr[TCI_BORDER], m_strSectName.c_str(), "LowlightBorderColor");

    loadLOBHilightSettings();
    loadLOBLowlightSettings();
}

void CLyricShowObj::loadLOBHilightSettings() {
    loadLyrOverlayBlendingSettings(m_strSectName.c_str(), m_tobHilight, getPatternFontHeight(), true);
}

void CLyricShowObj::loadLOBLowlightSettings() {
    loadLyrOverlayBlendingSettings(m_strSectName.c_str(), m_tobLowlight, getPatternFontHeight(), false);
}

void CLyricShowObj::loadAllSettings() {
    if (m_bUseSkinStyle) {
        return;
    }

    {
        FontInfoEx info;

        profileGetLyricsFont(m_strSectName.c_str(), info);
        m_font.create(info, 1.0);
        m_nFontHeight = m_font.getHeight();
    }

    setBgColor(g_profile.getString(m_strSectName.c_str(), "BgColor", ""));

    loadLyrOverlaySettings();

    setLyrAlign(g_profile.getString(m_strSectName.c_str(), "LyrAlign", ""));

    m_LyricsDisplayOpt = displayOptFromStr(g_profile.getString(m_strSectName.c_str(), "LyrDrawOpt", ""));

    m_nLineSpacing = CMPlayerSettings::getSettings(m_strSectName.c_str(), "LineSpacing", 2);
    m_bKaraoke = g_profile.getBool(m_strSectName.c_str(), "Karaoke", m_bKaraoke);

    m_vertLineAlign = (VertLineAlign)g_profile.getInt(m_strSectName.c_str(), "VertAlign", VLA_CENTER);
    if (m_vertLineAlign > VLA_BOTTOM || m_vertLineAlign < VLA_TOP) {
        m_vertLineAlign = VLA_CENTER;
    }
    m_nVertLineOffset = g_profile.getInt(m_strSectName.c_str(), "VertAlignOffset", 0);

    //
    // load Background image settings.
    //        Floating lyrics don't use background....
    if (m_etDispSettings != ET_LYRICS_FLOATING_SETTINGS) {
        m_bUseAlbumArtAsBg = g_profile.getBool(m_strSectName.c_str(), "UseAlbumArtAsBg", false);
        m_bUseBgImg = g_profile.getBool(m_strSectName.c_str(), "UseBgImage", false);
        m_strBgPicFolder = CMLProfile::getDir(m_strSectName.c_str(), "BgPicFolder", "");

        m_bDarkenLyrBgOnImg = g_profile.getBool(m_strSectName.c_str(), "DarkenLyrBg", true);

        if (m_bUseBgImg) {
            loadBgImageFolder();
            m_nextPicInFolder = g_profile.getInt(m_strSectName.c_str(), "NextPicInFolder", 0);
        }

        loadLyrOverlaySettings();
    }
}

bool inline isArabic(WCHAR c);

bool inline isArabic(WCHAR c) {
    return (c >= 0x590 && c <= 0x6ff);// || (c >= 0x0E00 && c <= 0x0E7F);
}

bool inline isStrArabic(cstr_t str) {
    while (*str) {
        if (isArabic(*str)) {
            return true;
        }
        str++;
    }

    return false;
}

void strreverse(char * str, int len) {
    for (int i = len / 2; i >= 0; i--) {
        char c = str[len - 1 - i];
        str[len - 1 - i] = str[i];
        str[i] = c;
    }
}

inline bool isNoneAlpha(char c);

inline bool isNoneAlpha(char c) {
    return (uint32_t)c < 0x7F && !isAlpha(c) && !isDigit(c);
}

void reverseStringForRightToLeftLanguage(char *szText) {
    cstr_t p = szText;
    cstr_t szStart = szText;
    cstr_t szNoneAlpha = nullptr;

    string str;

    while (*p) {
        szNoneAlpha = nullptr;
        while (*p && !isArabic(*p)) {
            if (isNoneAlpha(*p)) {
                if (szNoneAlpha == nullptr) {
                    szNoneAlpha = p;
                }
            } else {
                szNoneAlpha = nullptr;
            }
            p++;
        }

        if (*p && szNoneAlpha != nullptr) {
            // Reverse none alpha char
            strreverse((char *)szNoneAlpha, int(p - szNoneAlpha));

            // insert alpha str
            str.insert(0, szStart, int(szNoneAlpha - szStart));

            // insert none alpha
            str.insert(0, szNoneAlpha, int(p - szNoneAlpha));
        } else {
            str.insert(0, szStart, int(p - szStart));
        }
        szStart = p;

        if (!*p) {
            break;
        }

        while (isArabic(*p)) {
            p++;
        }

        str.insert(0, szStart, int(p - szStart));
        szStart = p;

        while (*p && isNoneAlpha(*p) && !isArabic(*p)) {
            p++;
        }

        if (p != szStart) {
            // Reverse none alpha char
            strreverse((char *)szStart, int(p - szStart));

            // insert none alpha
            str.insert(0, szStart, int(p - szStart));
            szStart = p;
        }
    }

    assert(str.size() == strlen(szText));
    strcpy(szText, str.c_str());
}

void CLyricShowObj::reverseLyricsForRightToLeftLanguage() {
    for (int i = 0; i < (int)m_lyrLines.size(); i++) {
        LyricsLine &line = m_lyrLines[i];
        for (int k = 0; k < (int)line.pieces.size(); k++) {
            LyricsPiece &piece = line.pieces[k];

            if (isStrArabic(piece.text.c_str())) {
                reverseStringForRightToLeftLanguage((char *)piece.text.c_str());
            }
        }
    }
}
