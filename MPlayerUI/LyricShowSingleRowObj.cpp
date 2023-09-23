

#include "MPlayerApp.h"
#include "LyricShowSingleRowObj.h"


// set classname
cstr_t CLyricShowSingleRowObj::ms_szClassName = "LyricShowSingleRow";

CLyricShowSingleRowObj::CLyricShowSingleRowObj() {
    m_nDarkenTopArea = float(0.65);
    m_nDarkenBottomArea = float(0.65);

    m_nXMargin = 5;
    m_nYMargin = 0;
}

CLyricShowSingleRowObj::~CLyricShowSingleRowObj() {

}

#define  FIO_TIME           500

void alphaBlendColor(CColor &clr1, CColor &clr2, int nAlpha, CColor &clrOut);

void CLyricShowSingleRowObj::drawCurLineFadeInNextLine(CRawGraph *canvas, LyricsLine &lineCur, LyricsLine *lineNext, int x, int y) {
    int nTime;
    int nAlphaCurLine, nAlphaNextLine;

    nTime = lineCur.endTime - m_curLyrics->getPlayElapsedTime();
    if (nTime <= FIO_TIME && nTime >= 0) {
        nAlphaCurLine = (FIO_TIME - nTime) * 255 / FIO_TIME;
    } else {
        nAlphaCurLine = 255;
    }

    if (lineNext) {
        nTime = lineNext->beginTime - m_curLyrics->getPlayElapsedTime();
        if (nTime <= FIO_TIME && nTime >= 0) {
            nAlphaNextLine = nTime * 255 / FIO_TIME;
        } else {
            nAlphaNextLine = 255;
        }
    } else {
        nAlphaNextLine = 255 - nAlphaCurLine;
    }


    if (m_pSkin->getEnableTranslucencyLayered() || m_img.isValid()) {
        // transparent window
        uint8_t alphaOld = canvas->getOpacityPainting();
        canvas->setOpacityPainting(nAlphaNextLine);
        drawRow(canvas, lineCur, AUTO_CAL_X, y, getHighlightColor(), m_tobHilight.clr[TCI_BORDER]);

        if (lineNext) {
            canvas->setOpacityPainting(nAlphaCurLine);

            drawRow(canvas, *lineNext, AUTO_CAL_X, y, getHighlightColor(), m_tobHilight.clr[TCI_BORDER]);
        }
        canvas->setOpacityPainting(alphaOld);
    } else {
        CColor clrTxt, clrTxtBorder;

        alphaBlendColor(m_tobHilight.clr[TCI_BORDER], m_clrBg, nAlphaCurLine, clrTxtBorder);
        alphaBlendColor(getHighlightColor(), m_clrBg, nAlphaCurLine, clrTxt);

        drawRow(canvas, lineCur, AUTO_CAL_X, y, clrTxt, clrTxtBorder);

        if (lineNext) {
            alphaBlendColor(m_tobHilight.clr[TCI_BORDER], m_clrBg, nAlphaNextLine, clrTxtBorder);
            alphaBlendColor(getHighlightColor(), m_clrBg, nAlphaNextLine, clrTxt);

            drawRow(canvas, *lineNext, AUTO_CAL_X, y, clrTxt, clrTxtBorder);
        }
    }
}

// OUTPUT:
//        rcUpdate    -    更新的矩形区域
void CLyricShowSingleRowObj::fastDraw(CRawGraph *canvas, CRect *prcUpdate) {
    canvas->setFont(&m_font);

    if (prcUpdate) {
        *prcUpdate = m_rcObj;
    }

    // get the current playing row
    int nRowCur = m_curLyrics->getCurPlayLine(m_lyrLines);
    if (nRowCur == -1) {
        // no lyrics, redraw background
        updateLyricDrawBufferBackground(canvas, m_rcObj);
        return;
    }

    auto &lyricRow = m_lyrLines[nRowCur];

    // clear back buffer
    updateLyricDrawBufferBackground(canvas, m_rcObj);

    int nPlayPos = m_curLyrics->getPlayElapsedTime();
    int y = getLineVertAlignPos() - getFontHeight() / 2 + getOutlineMargin();

    // 在当前行结束前0.5秒渐变切换到下一行
    if (nPlayPos + FIO_TIME >= lyricRow.endTime) {
        // 计算渐变显示的颜色
        LyricsLine *lineNext = nullptr;

        CRect rcClip;

        rcClip.setLTRB(m_rcObj.left + m_nXMargin,
            m_rcObj.top + m_nYMargin,
            m_rcObj.right - m_nXMargin,
            m_rcObj.bottom - m_nYMargin);
        // 设置剪裁区域为: 左边边框-->nSeperatePos
        CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
        canvas->setClipBoundBox(rcClip);

        if (nRowCur + 1 < (int)m_lyrLines.size()) {
            lineNext = &m_lyrLines[nRowCur + 1];
        }

        drawCurLineFadeInNextLine(canvas, lyricRow, lineNext, -1, y);
    } else {

        CRect rcClip;

        rcClip.setLTRB(m_rcObj.left + m_nXMargin,
            m_rcObj.top + m_nYMargin,
            m_rcObj.right - m_nXMargin,
            m_rcObj.bottom - m_nYMargin);
        // 设置剪裁区域为: 左边边框-->右边歌词的开始显示部分
        CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
        canvas->setClipBoundBox(rcClip);

        drawRow(canvas, lyricRow, AUTO_CAL_X, y, LP_CUR_LINE);
    }
}

cstr_t CLyricShowSingleRowObj::getClassName() {
    return ms_szClassName;
}

bool CLyricShowSingleRowObj::isKindOf(cstr_t szClassName) {
    if (CLyricShowObj::isKindOf(szClassName)) {
        return true;
    }

    return strcasecmp(szClassName, ms_szClassName) == 0;
}
