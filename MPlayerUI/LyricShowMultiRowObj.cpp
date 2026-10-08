/********************************************************************
    Created  :    2001-12-27 0:02:48
    FileName :    LyricShowMultiRowObj.cpp
    Author   :    xhy

    Purpose  :
*********************************************************************/

#include "MPlayerApp.h"
#include "LyricShowMultiRowObj.h"


// set classname
UIOBJECT_CLASS_NAME_IMP(CLyricShowMultiRowObj, "LyricShowMultiRow")

CLyricShowMultiRowObj::CLyricShowMultiRowObj() {
    m_yDrawingOld = -1;

    m_nCurRowOld = -1;

    m_nPrevLineAlphaOld = m_nextLineAlphaOld = 0;

    m_nDarkenTopArea = float(1.6);
    m_nDarkenBottomArea = float(1.6);
}

CLyricShowMultiRowObj::~CLyricShowMultiRowObj() {
}

void CLyricShowMultiRowObj::invalidate() {
    m_yDrawingOld = -1;
    m_nCurRowOld = -1;
    CLyricShowObj::invalidate();
}

void CLyricShowMultiRowObj::fastDraw(CRawGraph *canvas, CRect *prcUpdate) {
    assert(m_curLyrics);
    if (m_curLyrics == nullptr) {
        return;
    }

    if (!m_curLyrics->hasLyricsOpened() || m_lyrLines.size() == 0) {
        fastDrawMediaInfo(canvas, prcUpdate);
        return;
    }

    // get the current playing line
    int nRowCur = m_curLyrics->getCurPlayLine(m_lyrLines);
    if (nRowCur == -1) {
        // no lyrics
        prcUpdate->setEmpty();
        return;
    }

    int nCurRowCount = m_lyrLines.getCountWithSameTimeStamps(nRowCur);
    assert(nCurRowCount > 0);

    LyricsLine &lineCur = m_lyrLines[nRowCur];
    int nRowHeight = getLineHeight();
    int nDeltaTime = lineCur.endTime - lineCur.beginTime;

    CDispOptRecover dispOptRecover(this);
    if (m_img.isValid()) {
        m_LyricsDisplayOpt = DO_FADEOUT_BG;
    } else if (m_LyricsDisplayOpt == DO_AUTO) {
        if (m_rcObj.height() / nRowHeight >= 5) {
            m_LyricsDisplayOpt = DO_FADEOUT_LOWCOLOR;
        }
    }

    int yCurRow;
    if (nDeltaTime == 0 && m_nCurRowOld == nRowCur) {
        yCurRow = m_yDrawingOld;
    } else {
        yCurRow = getLineVertAlignPos();
        if (m_curLyrics->getPlayElapsedTime() < lineCur.beginTime || nDeltaTime == 0) {
            ;
        } else if (m_curLyrics->getPlayElapsedTime() > lineCur.endTime) {
            yCurRow += -nRowHeight * nCurRowCount;
        } else {
            yCurRow += -nRowHeight * nCurRowCount +
                nRowHeight * nCurRowCount * (lineCur.endTime - m_curLyrics->getPlayElapsedTime()) / nDeltaTime;
        }
    }

    if (prcUpdate) {
        if (yCurRow == m_yDrawingOld && nRowCur == m_nCurRowOld) {
            if (m_LyricsDisplayOpt == DO_NORMAL) {
                fastestDraw_GetUpdateRectOfNormal(canvas, prcUpdate, yCurRow);
            } else {
                fastestDraw_GetUpdateRectOfFadeInOut(canvas, prcUpdate, yCurRow, nRowCur);
            }
        }

        // 当参数 prcUpdate 有效时，不需要去绘制，仅仅计算需要绘制区域
        return;
    } else {
        m_nCurRowOld = nRowCur;
        m_yDrawingOld = yCurRow;
    }

    int yLyrStart, yLyrEnd, y;

    // draw lyrics lines above current line
    int nRow = nRowCur - 1;
    int nRowEnd = 0;
    CRect rcClip = getClipRect();
    for (y = yCurRow - nRowHeight; y + nRowHeight > rcClip.top && nRow >= nRowEnd; y -= nRowHeight) {
        LyricsLine &line = m_lyrLines[nRow];

        if (!drawRow(canvas, line, AUTO_CAL_X, y, LP_ABOVE_CUR_LINE)) {
            break;
        }
        nRow--;
    }
    yLyrStart = y + nRowHeight;

    // draw center row, the row lyric is current
    y = yCurRow;
    for (nRow = nRowCur; nRow < nRowCur + nCurRowCount; nRow++) {
        drawCurrentRow(canvas, m_lyrLines[nRow], AUTO_CAL_X, y);
        y += nRowHeight;
    }

    // draw lyrics lines below current line
    nRowEnd = (int)m_lyrLines.size();
    for (; y < rcClip.bottom && nRow < nRowEnd; y += nRowHeight) {
        LyricsLine &line = m_lyrLines[nRow];
        if (!drawRow(canvas, line, AUTO_CAL_X, y, LP_BELOW_CUR_LINE)) {
            break;
        }
        nRow++;
    }
    yLyrEnd = y;

    fadeOutVertBorder(canvas, yLyrStart, yLyrEnd);

    // if (g_bSave)
    //     {
    //         static int g_n = 0;
    //         g_n++;
    //         stringPrintf    strFile("C:\\a_%03d%d.bmp", g_n, prcUpdate);
    //
    //         saveBmpFileFromRawImageData(canvas->GetRawBuff(), strFile.c_str());
    //         g_bSave = false;
    //     }
}

void CLyricShowMultiRowObj::fastestDraw_GetUpdateRectOfFadeInOut(CRawGraph *canvas, CRect *prcUpdate, int y, int nCurLine) {
    int nRowHeight = getLineHeight();
    int nAlpha;

    assert(nCurLine >= 0 && nCurLine < (int)m_lyrLines.size());

    prcUpdate->setLTRB(m_rcObj.left + m_nXMargin, 0, m_rcObj.right - m_nXMargin, 0);
    prcUpdate->top = prcUpdate->bottom = y;

    // get current line update rect
    if (isKaraoke()) {
        prcUpdate->bottom = y + nRowHeight;
    }

    // get previous line update rect
    if (nCurLine > 0) {
        nAlpha = getAlpha(m_lyrLines[nCurLine - 1]);
        if (m_nPrevLineAlphaOld != nAlpha) {
            m_nPrevLineAlphaOld = nAlpha;

            // Redraw the whole line.
            prcUpdate->top = y - nRowHeight;
        }
    }

    // get next line update rect
    if (nCurLine < (int)m_lyrLines.size() - 1) {
        nAlpha = getAlpha(m_lyrLines[nCurLine + 1]);
        if (m_nextLineAlphaOld != nAlpha) {
            m_nextLineAlphaOld = nAlpha;

            // Redraw the whole line.
            if (prcUpdate->top == prcUpdate->bottom) {
                prcUpdate->top = y + nRowHeight;
            }
            prcUpdate->bottom = y + nRowHeight * 2;
        }
    }

    if (prcUpdate->top != prcUpdate->bottom && isOutlineLyrics()) {
        // 如果打开了 Outline, 则还需要增加 outline 的位置.
        prcUpdate->top -= getOutlineMargin();
        prcUpdate->bottom += getOutlineMargin();
    }

    if (prcUpdate->top < m_rcObj.top + m_nYMargin) {
        prcUpdate->top = m_rcObj.top + m_nYMargin;
    }
    if (prcUpdate->bottom > m_rcObj.bottom - m_nYMargin) {
        prcUpdate->bottom = m_rcObj.bottom - m_nYMargin;
    }
}

void CLyricShowMultiRowObj::fastestDraw_GetUpdateRectOfNormal(CRawGraph *canvas, CRect *prcUpdate, int y) {
    if (!isKaraoke()) {
        prcUpdate->setEmpty();
        return;
    }

    prcUpdate->top = y;
    prcUpdate->bottom = y + getLineHeight();
    if (prcUpdate->top < m_rcObj.top + m_nYMargin) {
        prcUpdate->top = m_rcObj.top + m_nYMargin;
    }
    if (prcUpdate->bottom > m_rcObj.bottom - m_nYMargin) {
        prcUpdate->bottom = m_rcObj.bottom - m_nYMargin;
    }
}
