#include "SkinTypes.h"
#include "Skin.h"
#include "SkinMenu.h"
#include "SkinEditCtrl.h"


UIOBJECT_CLASS_NAME_IMP(CSkinEditCtrl, "EditCtrl")


#define GLYPH_WIDTH_INVALID 0xFF

#define PASSWORD_CHAR       "*"

inline bool tobool(int n) {
    return n != 0;
}

namespace _SkinEditCtrl {

AutoInvalidate::~AutoInvalidate() {
    assert(m_editor);

    if (m_editor->isInBatchAction()) {
        return;
    }

    if (m_nTopVisibleLineOld != m_editor->m_nTopVisibleLine || m_nScrollPosxOld != m_editor->m_nScrollPosx) {
        m_needUpdate = true;
    } else if (m_nCaretRowOld != m_editor->m_nCaretRow) {
        m_needUpdate = true;
    } else if (!m_needUpdate && m_nBegSelRowOld == -1 && m_editor->m_nBegSelRow == -1) {
        return;
    }

    if (m_needUpdate ||
        m_nBegSelRowOld != m_editor->m_nBegSelRow ||
        m_nEndSelRowOld != m_editor->m_nEndSelRow ||
        m_nBegSelColOld != m_editor->m_nBegSelCol ||
        m_nEndSelColOld != m_editor->m_nEndSelCol) {
        m_editor->updateScrollInfo();
        m_editor->makeCaretInSight();
        m_editor->invalidate();
    }
}

void AutoInvalidate::saveOrg() {
    m_nTopVisibleLineOld = m_editor->m_nTopVisibleLine;
    m_nScrollPosxOld = m_editor->m_nScrollPosx;
    m_nBegSelRowOld = m_editor->m_nBegSelRow;
    m_nEndSelRowOld = m_editor->m_nEndSelRow;
    m_nBegSelColOld = m_editor->m_nBegSelCol;
    m_nEndSelColOld = m_editor->m_nEndSelCol;
    m_nCaretRowOld = m_editor->m_nCaretRow;
}

class StashSelectionCaret {
public:
    StashSelectionCaret(CSkinEditCtrl *editor = nullptr) {
        set(editor);
    }

    void set(CSkinEditCtrl *editor) {
        m_editor = editor;
        if (editor) {
            m_caretRow = m_editor->m_nCaretRow;
            m_caretCol = m_editor->m_nCaretCol;
            m_begSelRow = m_editor->m_nBegSelRow;
            m_begSelCol = m_editor->m_nBegSelCol;
            m_endSelRow = m_editor->m_nEndSelRow;
            m_endSelCol = m_editor->m_nEndSelCol;
        }
    }

    void apply() {
        if (m_editor) {
            m_editor->m_nCaretRow = m_caretRow;
            m_editor->m_nCaretCol = m_caretCol;
            m_editor->m_nBegSelRow = m_begSelRow;
            m_editor->m_nBegSelCol = m_begSelCol;
            m_editor->m_nEndSelRow = m_endSelRow;
            m_editor->m_nEndSelCol = m_endSelCol;
        }
    }

    CSkinEditCtrl *editor() { return m_editor; }

protected:
    friend class EditorDelAction;

    CSkinEditCtrl               *m_editor;
    int                         m_caretRow, m_caretCol;
    int                         m_begSelRow, m_begSelCol, m_endSelRow, m_endSelCol;

};

int countGlyphs(const string &str) {
    int n = (int)str.size();
    if (str.size() > 1) {
        n = 0;
        for (StringIterator it(str.c_str()); !it.isEOS(); ++it) {
            // 统计 UTF8 的字符数量，并且需要去掉 '\r' 的数量
            auto &c = it.curChar();
            if (c == '\r') {
                ++it;
                if (it.isEOS()) {
                    break;
                }
            }
            n++;
        }
    }

    return n;
}

class EditorInsertAction : public CUndoAction {
public:
    EditorInsertAction(CSkinEditCtrl *editor, int row, int col, cstr_t str) : m_editor(editor), m_row(row), m_col(col) {
        if (!editor->isInBatchAction()) {
            m_isInBatchAction = false;
            m_selectionCaretBefore.set(editor);
        }

        m_str.append(str);
    }

    void applyAction(bool isUndo) {
        assert(m_editor->isPosValid(m_row, m_col));
        if (!m_editor->isPosValid(m_row, m_col) || m_str.empty()) {
            return;
        }

        if (isUndo) {
            int n = countGlyphs(m_str);
            m_editor->removeStr(m_row, m_col, n);
            m_editor->m_nCaretRow = m_row;
            m_editor->m_nCaretCol = m_col;
            m_editor->m_nBegSelRow = -1;
        } else {
            m_editor->insertStr(m_row, m_col, m_str.c_str());
        }

        if (!m_isInBatchAction) {
            m_editor->updateScrollInfo();
            m_editor->makeCaretInSight();
            m_editor->invalidate();
        }
    }

    void undo() {
        applyAction(true);

        if (!m_isInBatchAction) {
            m_selectionCaretBefore.apply();
        }
    }

    virtual void redo() {
        applyAction(false);
    }

protected:
    CSkinEditCtrl               *m_editor;
    bool                        m_isInBatchAction = true;
    int                         m_row, m_col;
    string                      m_str;
    StashSelectionCaret         m_selectionCaretBefore;

};


class EditorDelAction : public EditorInsertAction {
public:
    EditorDelAction(CSkinEditCtrl *editor, int nCaretRow, int nCaretCol, cstr_t str) : EditorInsertAction(editor, nCaretRow, nCaretCol, str) {
    }

    virtual void undo() {
        applyAction(false);
        if (!m_isInBatchAction) {
            m_selectionCaretBefore.apply();
        }
    }
    virtual void redo() {
        applyAction(true);
    }

    void setStashSelectionCaretBefore(const StashSelectionCaret &data) {
        m_selectionCaretBefore = data;
    }

};

class EditorBatchAction : public CBatchUndoAction {
public:
    EditorBatchAction(CSkinEditCtrl *editor)  {
        m_selectionCaretBefore.set(editor);
    }

    virtual void undo() {
        CBatchUndoAction::undo();
        m_selectionCaretBefore.apply();

    }

    virtual void redo() {
        CBatchUndoAction::redo();
        m_selectionCaretAfter.apply();
    }

    void end() {
        m_selectionCaretAfter.set(m_selectionCaretBefore.editor());
    }

protected:
    StashSelectionCaret         m_selectionCaretBefore, m_selectionCaretAfter;

};

AutoBatchUndo::AutoBatchUndo(CSkinEditCtrl *editor) : m_update(editor) {
    m_editor = editor;
    m_batchAction = new EditorBatchAction(editor);
    m_editor->m_undoMgr.beginBatchAction(m_batchAction);
}

void AutoBatchUndo::endBatchUndo() {
    m_editor->m_undoMgr.endBatchAction();
    m_batchAction->end();
    m_update.setNeedUpdate();
}

} // namespace _SkinEditCtrl

using namespace _SkinEditCtrl;

CSkinEditCtrl::CSkinEditCtrl() {
    m_bEnableBorder = true;

    m_nEditorStyles = 0;

    m_nCaretMaxXLatest = m_nCaretX = 0;
    m_nCaretY = 0;
    m_nLineSpace = 2;
    m_nTopVisibleLine = 0;

    m_vClrTable.resize(CN_CUSTOMIZED_START);
    m_vClrTable[CN_BG].set(RGB(255, 255, 255));
    m_vClrTable[CN_TEXT].set(RGB(0, 0, 0));
    m_vClrTable[CN_SEL_BG].set(RGB(0x7A, 0x96, 0xDF));
    m_vClrTable[CN_SEL_TEXT].set(RGB(255, 255, 255));

    m_nBegSelCol = m_nEndSelCol = m_nBegSelRow = m_nEndSelRow = -1;
    m_nOneCharDx = 5;
    m_bInMouseSel = false;
    m_nScrollPosx = 0;

    m_isMarkedText = false;
    m_begMarkedRow = -1; m_endMarkedRow = -1;
    m_begMarkedCol = -1; m_endMarkedCol = -1;

    m_pEditSyntaxParser = nullptr;
    m_pEditNotification = nullptr;

    m_undoMgr.setNotification(this);

    m_bPrevSelectedStatus = false;

    // At least one line is in editor
    m_vLines.push_back(newOneLine());

    m_nCaretRow = 0;
    m_nCaretCol = 0;

    m_nIDTimerCaret = 0;
    m_cursor.loadStdCursor(Cursor::C_IBEAM);

    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON | UO_MSG_WANT_RBUTTON | UO_MSG_WANT_KEY;

    m_xMargin = m_yMargin = 0;
}

CSkinEditCtrl::~CSkinEditCtrl() {
    for (int i = 0; i < (int)m_vLines.size(); i++) {
        delete m_vLines[i];
    }
    m_vLines.clear();
}

bool CSkinEditCtrl::CGlyph::iCmp(const char *str) {
    return strncasecmp(str, chGlyph, chGlyph.size()) == 0;
}

bool CSkinEditCtrl::CGlyph::reverseiCmp(const char *str) {
    return strncasecmp(str - chGlyph.size() + 1, chGlyph, chGlyph.size()) == 0;
}

void CSkinEditCtrl::onFontChanged() {
    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    m_nOneCharDx = 0;
    for (char ch = 'a'; ch <= 'z'; ch++) {
        CSize size;
        canvas->getTextExtentPoint32(&ch, 1, &size);
        m_nOneCharDx += size.cx;
    }
    m_nOneCharDx /= 'z' - 'a' + 1;

    for (int i = 0; i < (int)m_vLines.size(); i++) {
        COneLine *pLine = m_vLines[i];

        updateWidthInfoOfLine(canvas, pLine);
    }

    updateScrollInfo();
}

void CSkinEditCtrl::replaceSel(cstr_t text) {
    if (!isSelected()) {
        return;
    }

    if (isInBatchAction()) {
        removeSel();
        m_undoMgr.addAction(new EditorInsertAction(this, m_nCaretRow, m_nCaretCol, text));
        insertStr(m_nCaretRow, m_nCaretCol, text);
    } else {
        AutoBatchUndo batch(this);
        removeSel();
        m_undoMgr.addAction(new EditorInsertAction(this, m_nCaretRow, m_nCaretCol, text));
        insertStr(m_nCaretRow, m_nCaretCol, text);
    }
}

void CSkinEditCtrl::removeSel() {
    if (!isSelected()) {
        return;
    }

    int nBegSelRow, nBegSelCol;
    string strToDel;
    StashSelectionCaret selectionCaretBefore(this);

    selectionToText(strToDel);
    removeSelected(nBegSelRow, nBegSelCol);
    auto undoAction = new EditorDelAction(this, nBegSelRow, nBegSelCol, strToDel.c_str());
    undoAction->setStashSelectionCaretBefore(selectionCaretBefore);
    m_undoMgr.addAction(undoAction);

    if (!isInBatchAction()) {
        updateScrollInfo();
        makeCaretInSight();
        invalidate();
    }
}

void CSkinEditCtrl::setSel(int nBeg, int nEnd) {
    int n = 0;

    m_nBegSelRow = -1;
    m_nEndSelRow = -1;

    if (nBeg == -1) {
        onNotifySelChanged();
        return;
    }

    for (int i = 0; i < (int)m_vLines.size(); i++) {
        COneLine *pLine = m_vLines[i];

        for (int k = 0; k < (int)pLine->size(); k++) {
            if (n == nBeg) {
                m_nBegSelRow = i;
                m_nBegSelCol = k;
            } else if (n == nEnd) {
                m_nEndSelRow = i;
                m_nEndSelCol = k;
            }
            n++;
        }
        if (pLine->newLineType == NLT_RN) {
            n += 2;
        } else if (pLine->newLineType == NLT_N) {
            n++;
        } else if (pLine->newLineType == NLT_R) {
            n++;
        }
        if (n >= nBeg && m_nBegSelRow == -1) {
            m_nBegSelRow = i;
            m_nBegSelCol = (int)pLine->size();
        } else if (n >= nEnd && m_nEndSelRow == -1) {
            m_nEndSelRow = i;
            m_nEndSelCol = (int)pLine->size();
        }
    }

    onNotifySelChanged();
}

void CSkinEditCtrl::setSel(int nBegLine, int nBegCol, int nEndLine, int nEndCol) {
    if ((nBegLine >= 0 && nBegLine < (int)m_vLines.size()) &&
        (nBegCol >= 0 && nBegCol <= (int)m_vLines[nBegLine]->size()) &&
        (nEndLine >= 0 && nEndLine < (int)m_vLines.size()) &&
        (nEndCol >= 0 && nEndCol <= (int)m_vLines[nEndLine]->size())) {
        m_nBegSelRow = nBegLine;
        m_nBegSelCol = nBegCol;
        m_nEndSelRow = nEndLine;
        m_nEndSelCol = nEndCol;

        onNotifySelChanged();
    }
}

void CSkinEditCtrl::setSelOfLine(int nLine, int nCol, int nNumOfSel) {
    if (nLine >= 0 && nLine < (int)m_vLines.size()) {
        if (nCol >= 0 && nCol <= (int)m_vLines[nLine]->size()) {
            m_nBegSelRow = nLine;
            m_nBegSelCol = nCol;

            m_nEndSelRow = nLine;
            assert(nNumOfSel + nCol <= (int)m_vLines[nLine]->size());
            if (nNumOfSel + nCol <= (int)m_vLines[nLine]->size()) {
                m_nEndSelCol = nCol + nNumOfSel;
            } else {
                m_nEndSelCol = (int)m_vLines[nLine]->size();
            }

            onNotifySelChanged();
            return;
        }
    }
}

void CSkinEditCtrl::setCaret(int nRow, int nCol) {
    if (nRow == -1 || nRow >= (int)m_vLines.size()) {
        m_nCaretRow = (int)m_vLines.size() - 1;
        m_nCaretCol = getEndColPosOfLine(m_nCaretRow);
        return;
    }

    m_nCaretRow = nRow;
    if (nCol > getEndColPosOfLine(m_nCaretRow)) {
        m_nCaretCol = getEndColPosOfLine(m_nCaretRow);
    } else {
        m_nCaretCol = nCol;
    }

    makeCaretInSight();
    invalidate();
}

void CSkinEditCtrl::setText(cstr_t szText) {
    m_undoMgr.clear();
    doSetText(szText);
    showCaret();
    updateScrollInfo();
}

int CSkinEditCtrl::getText(string &str) {
    for (int i = 0; i < (int)m_vLines.size(); i++) {
        COneLine *pLine = m_vLines[i];
        for (int k = 0; k < (int)pLine->size(); k++) {
            CGlyph &glyph = (*pLine)[k];
            str += glyph.chGlyph;
        }
        if (pLine->newLineType == NLT_RN) {
            str += "\r\n";
        } else if (pLine->newLineType == NLT_N) {
            str += "\n";
        } else if (pLine->newLineType == NLT_R) {
            str += "\r";
        }
    }

    return (int)str.size();
}

void CSkinEditCtrl::selectionToText(string &str) {
    int nBegSelRow = 0, nEndSelRow = 0;
    int nBegSelCol = 0, nEndSelCol = 0;
    bool bSelectedEmpty = true;

    if (m_nBegSelRow != -1) {
        bSelectedEmpty = false;
        sortSelectPos(nBegSelRow, nBegSelCol, nEndSelRow, nEndSelCol, bSelectedEmpty);
    }

    if (bSelectedEmpty) {
        return;
    }

    // to text
    for (int i = nBegSelRow; i <= nEndSelRow; i++) {
        COneLine *pLine = m_vLines[i];

        for (int k = 0; k < (int)pLine->size(); k++) {
            CGlyph &glyph = (*pLine)[k];

            bool bSel = true;
            if (i == nBegSelRow) {
                if (k < nBegSelCol) {
                    // don't fill
                    bSel = false;
                }
            }
            if (i == nEndSelRow) {
                if (k >= nEndSelCol) {
                    // don't fill
                    bSel = false;
                }
            }
            if (bSel) {
                str += glyph.chGlyph;
            }
        }
        if (i < nEndSelRow) {
            str += pLine->getReturnStr(pLine->newLineType);
        }
    }
}

bool CSkinEditCtrl::getTextOfLine(int nLine, string &strLine) {
    strLine = "";
    if (nLine < 0 || nLine >= (int)m_vLines.size()) {
        return false;
    }

    COneLine *pLine = m_vLines[nLine];

    for (int k = 0; k < (int)pLine->size(); k++) {
        CGlyph &glyph = (*pLine)[k];

        strLine += glyph.chGlyph;
    }

    return true;
}

bool CSkinEditCtrl::setTextOfLine(int nLine, cstr_t szLine) {
    if (nLine >= 0 && nLine < (int)m_vLines.size()) {
        m_nBegSelRow = nLine;
        m_nBegSelCol = 0;
        m_nEndSelRow = nLine;
        m_nEndSelCol = (int)m_vLines[nLine]->size();

        assert(strchr(szLine, '\n') == nullptr);
        replaceSel(szLine);

        return true;
    } else {
        return false;
    }
}

int CSkinEditCtrl::getLength() {
    int n = 0;

    for (int i = 0; i < (int)m_vLines.size(); i++) {
        COneLine *pLine = m_vLines[i];

        for (int k = 0; k < (int)pLine->size(); k++) {
            n++;
        }
        if (pLine->newLineType == NLT_RN) {
            n += 2;
        } else if (pLine->newLineType == NLT_N) {
            n++;
        } else if (pLine->newLineType == NLT_R) {
            n++;
        }
    }
    return n;
}

bool CSkinEditCtrl::canUndo() {
    return m_undoMgr.canUndo();
}

bool CSkinEditCtrl::canRedo() {
    return m_undoMgr.canRedo();
}

void CSkinEditCtrl::onCmdCopy() {
    if (isSelected()) {
        string str;
        selectionToText(str);
        if (str.size()) {
            copyTextToClipboard(str.c_str());
        }
    }
}

// cut selected
void CSkinEditCtrl::onCmdCut() {
    if (isReadOnly()) {
        return;
    }

    if (!isSelected()) {
        return;
    }

    string strToDel;

    selectionToText(strToDel);
    copyTextToClipboard(strToDel.c_str());

    removeSel();
}

void CSkinEditCtrl::onCmdPaste() {
    if (isReadOnly()) {
        return;
    }

    string str;

    getClipBoardText(str);

    if (!str.empty()) {
        insertStr(str.c_str());
    }
}

void CSkinEditCtrl::onCmdUndo() {
    if (isReadOnly()) {
        return;
    }

    if (m_undoMgr.canUndo()) {
        m_undoMgr.undo();
    }
}

void CSkinEditCtrl::onCmdRedo() {
    if (isReadOnly()) {
        return;
    }

    if (m_undoMgr.canRedo()) {
        m_undoMgr.redo();
    }
}

void CSkinEditCtrl::onCmdDelete() {
    if (isReadOnly()) {
        return;
    }

    if (isSelected()) {
        removeSel();
    } else {
        string strToDel;
        AutoInvalidate updater(this);

        getStrAtPos(m_nCaretRow, m_nCaretCol, strToDel);
        m_undoMgr.addAction(new EditorDelAction(this, m_nCaretRow, m_nCaretCol, strToDel.c_str()));
        removeChar(m_nCaretRow, m_nCaretCol);

        updater.setNeedUpdate();
    }
}

void CSkinEditCtrl::getStrAtPos(int nRow, int nCol, string &str) {
    str.resize(0);

    assert(isPosValid(nRow, nCol));
    if (!isPosValid(nRow, nCol)) {
        return;
    }

    COneLine *pLine = m_vLines[nRow];

    if (nCol == pLine->size()) {
        // remove the "\n".
        if (nRow == m_vLines.size() - 1) {
            return;
        }

        str = pLine->getReturnStr(pLine->newLineType);
    } else {
        str = pLine->at(nCol).chGlyph;
    }
}

bool CSkinEditCtrl::isPosValid(int nRow, int nCol) {
    if (nRow >= 0 && nRow < (int)m_vLines.size()) {
        COneLine *pLine = m_vLines[nRow];

        return (nCol >= 0 && nCol <= (int)pLine->size());
    }

    return false;
}

void CSkinEditCtrl::sortSelectPos(int &nBegSelRow, int &nBegSelCol, int &nEndSelRow, int &nEndSelCol, bool &bSelectedEmpty) {
    assert(m_nBegSelRow != -1);
    bSelectedEmpty = false;
    if (m_nBegSelRow <= m_nEndSelRow) {
        nBegSelRow = m_nBegSelRow;
        nEndSelRow = m_nEndSelRow;
        if (m_nBegSelRow == m_nEndSelRow && m_nEndSelCol == m_nBegSelCol) {
            bSelectedEmpty = true;
        }
        if (m_nBegSelRow == m_nEndSelRow && m_nEndSelCol < m_nBegSelCol) {
            nBegSelCol = m_nEndSelCol;
            nEndSelCol = m_nBegSelCol;
        } else {
            nBegSelCol = m_nBegSelCol;
            nEndSelCol = m_nEndSelCol;
        }
    } else {
        nBegSelRow = m_nEndSelRow;
        nEndSelRow = m_nBegSelRow;
        nBegSelCol = m_nEndSelCol;
        nEndSelCol = m_nBegSelCol;
    }
}

void CSkinEditCtrl::onLanguageChanged() {
    CSkinScrollFrameCtrlBase::onLanguageChanged();
    if (!m_placeHolderEnglish.empty()) {
        m_placeHolder = _TL(m_placeHolderEnglish.c_str());
    }
}

void CSkinEditCtrl::onCreate() {
    if (isFlagSet(m_nEditorStyles, S_MULTILINE)) {
        m_bHorzScrollBar = m_bVertScrollBar = true;
        m_msgNeed |= UO_MSG_WANT_MOUSEWHEEL;
    }

    CSkinScrollFrameCtrlBase::onCreate();

    m_nCaretX = m_xMargin;
    m_nCaretY = m_yMargin;

    m_font.setParent(m_pSkin);

    updateWidthOfAllLines();
}

void CSkinEditCtrl::fillSelectedLineBg(CRawGraph *canvas, int x, int y, COneLine *pLine, int nBegSelCol, int nEndSelCol) {
    CRect rc;

    assert(nBegSelCol <= (int)pLine->size() && nEndSelCol <= (int)pLine->size());

    for (int k = 0; k < nBegSelCol; k++) {
        CGlyph &glyph = (*pLine)[k];
        x += glyph.width;
    }
    rc.left = x;
    rc.top = y;
    rc.right = rc.left;
    rc.bottom = rc.top + getLineDy();

    for (int k = nBegSelCol; k < nEndSelCol; k++) {
        CGlyph &glyph = (*pLine)[k];
        rc.right += glyph.width;
    }

    fillGraph(canvas, rc, CN_SEL_BG);
}

void CSkinEditCtrl::draw(CRawGraph *canvas) {
    auto oldBgType = m_bgType;
    m_bgType = BG_NONE;
    CSkinScrollFrameCtrlBase::draw(canvas);
    m_bgType = oldBgType;

    canvas->setFont(m_font.getFont());

    fillGraph(canvas, m_rcContent, CN_BG);

    CRect rc = m_rcContent;
    rc.deflate(m_xMargin, m_yMargin);

    int nBegSelRow = 0, nEndSelRow = 0;
    int nBegSelCol = 0, nEndSelCol = 0;
    bool bSelectedEmpty = true;

    if (m_nBegSelRow != -1) {
        bSelectedEmpty = false;
        sortSelectPos(nBegSelRow, nBegSelCol, nEndSelRow, nEndSelCol, bSelectedEmpty);
    }

    CRect rcClip = rc;

    CRawGraph::CClipBoxAutoRecovery autoCBR(canvas);
    canvas->setClipBoundBox(rcClip);

    if (m_vLines.empty() || (m_vLines.size() == 1 && m_vLines[0]->size() == 0)) {
        if (!isOnFocus()) {
            // Draw place holder
            canvas->setTextColor(m_clrPlaceHolder);
            canvas->drawText(m_placeHolder, rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            return;
        }

        m_caret.draw(canvas);
        return;
    }

    int xStart = rcClip.left;
    int y = rcClip.top;
    int xMax = rcClip.right;
    int yMax = rcClip.bottom;
    assert(m_nTopVisibleLine >= 0 && m_nTopVisibleLine < (int)m_vLines.size());

    if (!isFlagSet(m_nEditorStyles, S_MULTILINE)) {
        y = rc.top + (rc.height() - getFontHeight()) / 2;
    }

    if (bSelectedEmpty) {
        // If no lines are selected, draw same
        for (int i = m_nTopVisibleLine; i < (int)m_vLines.size(); i++) {
            if (y + getLineDy() > yMax && i > m_nTopVisibleLine) {
                break;
            }

            drawLine(canvas, m_vLines[i], xStart, xMax, y);

            y += getLineDy();
        }
        m_caret.draw(canvas);
        return;
    }

    // draw lines before selection
    for (int i = m_nTopVisibleLine; i < nBegSelRow; i++) {
        if (y + getLineDy() > yMax && i > m_nTopVisibleLine) {
            break;
        }

        drawLine(canvas, m_vLines[i], xStart, xMax, y);

        y += getLineDy();
    }

    // draw lines at selection
    for (int i = nBegSelRow; i <= nEndSelRow; i++) {
        if (i < m_nTopVisibleLine) {
            continue;
        }
        if (y + getLineDy() > yMax && i > m_nTopVisibleLine) {
            break;
        }

        COneLine *pLine = m_vLines[i];
        int nBegSelColCurLine, nEndSelColCurLine;

        if (i == nBegSelRow) {
            nBegSelColCurLine = nBegSelCol;
        } else {
            nBegSelColCurLine = 0;
        }

        if (i == nEndSelRow) {
            nEndSelColCurLine = nEndSelCol;
        } else {
            nEndSelColCurLine = (int)pLine->size();
        }

        int x = xStart - m_nScrollPosx;

        fillSelectedLineBg(canvas, x, y, pLine, nBegSelColCurLine, nEndSelColCurLine);

        for (int k = 0; k < (int)pLine->size(); k++) {
            CGlyph &glyph = (*pLine)[k];
            assert(glyph.width != GLYPH_WIDTH_INVALID);
            if (x + glyph.width < m_xMargin) {
                x += glyph.width;
                continue;
            }
            if (x + glyph.width > xMax) {
                break;
            }

            if (k >= nBegSelColCurLine && k < nEndSelColCurLine) {
                CRect rcLine;
                rcLine.setLTRB(x, y, x + glyph.width, y + getLineDy());
                canvas->setTextColor(getColor(CN_SEL_TEXT));
            } else {
                if (glyph.clrIndex >= m_vClrTable.size()) {
                    canvas->setTextColor(getColor(CN_TEXT));
                } else {
                    canvas->setTextColor(m_vClrTable[glyph.clrIndex]);
                }
            }
            if (glyph.chGlyph != '\t') {
                if (isPassword()) {
                    canvas->textOut(x, y, PASSWORD_CHAR, 1);
                } else {
                    canvas->textOut(x, y, glyph.chGlyph, glyph.chGlyph.size());
                }
            }
            x += glyph.width;
        }

        y += getLineDy();
    }

    //    canvas->setTextColor(m_clrText);
    for (int i = nEndSelRow + 1; i < (int)m_vLines.size(); i++) {
        if (i < m_nTopVisibleLine) {
            continue;
        }
        if (y + getLineDy() > yMax && i > m_nTopVisibleLine) {
            break;
        }

        drawLine(canvas, m_vLines[i], xStart, xMax, y);

        y += getLineDy();
    }

    m_caret.draw(canvas);
}

CColor & CSkinEditCtrl::getColor(int nColorName) {
    assert(m_vClrTable.size() >= CN_CUSTOMIZED_START);
    if (nColorName < 0 || nColorName >= (int)m_vClrTable.size()) {
        return m_vClrTable[CN_TEXT];
    } else {
        return m_vClrTable[nColorName];
    }
}

void CSkinEditCtrl::setColor(int nColorName, const CColor & clr) {
    assert(m_vClrTable.size() >= CN_CUSTOMIZED_START);
    if (nColorName >= (int)m_vClrTable.size()) {
        if (nColorName > CN_MAX) {
            return;
        }
        m_vClrTable.resize(nColorName + 1);
    }

    m_vClrTable[nColorName] = clr;
}

uint8_t CSkinEditCtrl::getBgAlpha() {
#ifdef _WIN32
    if (m_translucencyWithSkin) {
        return int(m_pSkin->m_nCurTranslucencyAlpha);
    }
#endif
    return 255;
}

void CSkinEditCtrl::updateRectToScreen(const CRect *rc) {
    if (!isVisible() || !isParentVisible()) {
        return;
    }

    m_pContainer->updateMemGraphicsToScreen(rc, this);
}

void CSkinEditCtrl::fillGraph(CRawGraph *canvas, const CRect &rc, int nColorName) {
    int alpha = getBgAlpha();
    CColor clr = getColor(nColorName);
    clr.setAlpha(alpha);

    canvas->fillRect(rc, clr);
}

void CSkinEditCtrl::drawLine(CRawGraph *canvas, COneLine *pLine, int x, int xMax, int y) {
    uint8_t byClrLast = 0;

    x -= m_nScrollPosx;

    canvas->setTextColor(getColor(CN_TEXT));
    for (int k = 0; k < (int)pLine->size(); k++) {
        CGlyph &glyph = (*pLine)[k];
        assert(glyph.width != GLYPH_WIDTH_INVALID);

        if (glyph.clrIndex != byClrLast) {
            if (glyph.clrIndex >= m_vClrTable.size()) {
                byClrLast = 0;
            } else {
                byClrLast = glyph.clrIndex;
            }
            canvas->setTextColor(m_vClrTable[byClrLast]);
        }
        if (x + glyph.width < m_xMargin) {
            x += glyph.width;
            continue;
        }
        if (x + glyph.width > xMax) {
            break;
        }

        if (glyph.chGlyph != '\t') {
            if (isPassword()) {
                canvas->textOut(x, y, PASSWORD_CHAR, 1);
            } else {
                canvas->textOut(x, y, glyph.chGlyph, glyph.chGlyph.size());
            }
        }
        x += glyph.width;
    }
}

void CSkinEditCtrl::updateWidthInfoOfLine(CRawGraph *canvas, COneLine *pLine) {
    int x = 0;
    for (int k = 0; k < (int)pLine->size(); k++) {
        CGlyph &glyph = (*pLine)[k];
        if (glyph.chGlyph == '\t') {
            // tab
            glyph.width = m_nOneCharDx * 4 - x % (m_nOneCharDx * 4);
        } if (glyph.chGlyph == ' ') {
            // space
            glyph.width = m_nOneCharDx;
        } else {
            CSize size;
            if (isPassword()) {
                canvas->getTextExtentPoint32(PASSWORD_CHAR, 1, &size);
            } else {
                canvas->getTextExtentPoint32(glyph.chGlyph, glyph.chGlyph.size(), &size);
            }
            glyph.width = (uint8_t)size.cx;
        }
        x += glyph.width;
    }
}

void CSkinEditCtrl::updateWidthOfAllLines() {
    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    for (int i = 0; i < (int)m_vLines.size(); i++) {
        updateWidthInfoOfLine(canvas, m_vLines[i]);
    }
}

bool CSkinEditCtrl::doSetText(cstr_t szText) {
    COneLine *pLine = nullptr;

    // delete old text
    clear();

    selectNone();

    m_nCaretRow = 0;
    m_nCaretCol = 0;

    pLine = m_vLines[0];
    while (*szText) {
        CGlyph glyph;

        glyph.chGlyph = szText;
        glyph.clrIndex = CN_TEXT;
        if (*szText == '\r') {
            //             glyph.width = 0;
            //             pLine->push_back(glyph);
            szText++;
            if (*szText == '\n') {
                pLine->newLineType = NLT_RN;
                onNotifyParseLine((int)m_vLines.size() - 1);
                pLine = newOneLine();
                m_vLines.push_back(pLine);
            } else {
                pLine->newLineType = NLT_R;
                onNotifyParseLine((int)m_vLines.size() - 1);
                pLine = newOneLine();
                m_vLines.push_back(pLine);
            }
        } else if (*szText == '\n') {
            pLine->newLineType = NLT_N;
            onNotifyParseLine((int)m_vLines.size() - 1);
            pLine = newOneLine();
            m_vLines.push_back(pLine);
        } else {
            glyph.width = GLYPH_WIDTH_INVALID;
            pLine->push_back(glyph);
        }

        assert(glyph.chGlyph.size() > 0);
        szText += glyph.chGlyph.size();
    }

    onNotifyParseLine((int)m_vLines.size() - 1);


    //
    // update line width
    //
    if (m_bCreated) {
        updateWidthOfAllLines();
    }

    return true;
}

void CSkinEditCtrl::clear() {
    m_nCaretMaxXLatest = m_nCaretX = m_xMargin;
    m_nCaretY = m_yMargin;
    m_nTopVisibleLine = 0;
    m_nBegSelCol = m_nEndSelCol = m_nBegSelRow = m_nEndSelRow = -1;
    m_nScrollPosx = 0;

    m_nCaretRow = m_nCaretCol = 0;

    for (int i = 0; i < (int)m_vLines.size(); i++) {
        delete m_vLines[i];
    }
    m_vLines.clear();

    m_vLines.push_back(newOneLine());
}

void CSkinEditCtrl::clearWidth() {
    for (int i = 0; i < (int)m_vLines.size(); i++) {
        COneLine *pLine = m_vLines[i];

        for (int k = 0; k < (int)pLine->size(); k++) {
            CGlyph &glyph = (*pLine)[k];
            if (glyph.width != 0) {
                glyph.width = GLYPH_WIDTH_INVALID;
            }
        }
    }
}

void CSkinEditCtrl::showCaret(int x, int y, int nWidth, int nHeight) {
    CRawGraph *canvas;

    canvas = m_pContainer->getMemGraph();
    assert(canvas);

    if (!m_caret.isOn()) {
        m_nIDTimerCaret = m_pSkin->registerTimerObject(this, TIMER_DURATION_CARET);
    }

    if (m_caret.isOn() && m_caret.isDrawed()) {
        m_caret.drawFlash(canvas);
        updateRectToScreen(&m_caret.getUpdateRect());
    }

    if (m_pSkin->getFocusUIObj() == (CUIObject *)this) {
        m_caret.showCaret(x, y, nWidth, nHeight);
        m_pSkin->caretPositionChanged(CPoint(m_rcObj.left + x + 10, m_rcObj.top + y + 10 + nHeight));
    }
}

void CSkinEditCtrl::hideCaret() {
    if (m_nIDTimerCaret) {
        m_pSkin->unregisterTimerObject(this, m_nIDTimerCaret);
        m_nIDTimerCaret = 0;
    }
    m_caret.hideCaret();
}

void CSkinEditCtrl::showCaret() {
    if (isFlagSet(m_nEditorStyles, S_MULTILINE)) {
        m_nCaretY = m_yMargin + (m_nCaretRow - m_nTopVisibleLine) * getLineDy();
    } else {
        m_nCaretY = (m_rcContent.height() - getFontHeight()) / 2;
    }
    showCaret(m_nCaretX, m_nCaretY, 1, getFontHeight());
}

void CSkinEditCtrl::makeCaretInSight() {
    CRect rc = m_rcContent;

    if (m_nCaretRow < 0 || m_nCaretRow >= (int)m_vLines.size()) {
        m_nCaretRow = 0;
        m_nCaretCol = 0;
    }

    // rc.deflate(m_xMargin, m_yMargin);
    //     if (m_nCaretRow < m_nTopVisibleLine)
    //         m_nTopVisibleLine = m_nCaretRow;
    //     else
    //     {
    //         m_nCaretY = m_yMargin + (m_nCaretRow - m_nTopVisibleLine) * getLineDy();
    //         if (m_nCaretY >= rc.bottom - m_yMargin)
    //         {
    //             m_nTopVisibleLine = m_nCaretRow;
    //         }
    //     }

    if (m_pVertScrollBar && m_pVertScrollBar->isEnabled()) {
        if (m_nCaretRow - m_nTopVisibleLine == m_pVertScrollBar->getPage()) {
            m_nTopVisibleLine++;
        } else if (m_nTopVisibleLine - m_nCaretRow == 1) {
            m_nTopVisibleLine--;
        } else if (m_nCaretRow - m_nTopVisibleLine > m_pVertScrollBar->getPage()) {
            m_nTopVisibleLine += (m_nCaretRow - m_nTopVisibleLine) - (m_nCaretRow - m_nTopVisibleLine) % m_pVertScrollBar->getPage();
        } else if (m_nCaretRow < m_nTopVisibleLine) {
            m_nTopVisibleLine -= (m_nTopVisibleLine - m_nCaretRow) - (m_nTopVisibleLine - m_nCaretRow) % m_pVertScrollBar->getPage();
            if (m_nCaretRow < m_nTopVisibleLine) {
                m_nTopVisibleLine -= m_pVertScrollBar->getPage();
            }
        }

        if (m_nTopVisibleLine > m_pVertScrollBar->getMax()) {
            m_nTopVisibleLine = m_pVertScrollBar->getMax();
        }
    } else {
        m_nTopVisibleLine = 0;
    }

    if (m_nTopVisibleLine < 0) {
        m_nTopVisibleLine = 0;
    }

    m_nCaretY = m_yMargin + (m_nCaretRow - m_nTopVisibleLine) * getLineDy();

    if (m_vLines.empty()) {
        m_nCaretX = m_xMargin;
    } else {
        COneLine *pLine;
        int nNextCaretCharX;

        pLine = m_vLines[m_nCaretRow];
        nNextCaretCharX = m_nCaretX = m_xMargin;
        assert(m_nCaretCol <= (int)pLine->size());
        for (int i = 0; i < (int)pLine->size() && i <= m_nCaretCol; i++) {
            CGlyph &glyph = (*pLine)[i];

            assert(glyph.width != GLYPH_WIDTH_INVALID);
            m_nCaretX = nNextCaretCharX;
            nNextCaretCharX += glyph.width;
        }
        if (m_nCaretCol == pLine->size()) {
            m_nCaretX = nNextCaretCharX;
        }

        if (nNextCaretCharX - m_nScrollPosx >= rc.right - m_xMargin) {
            // horz scroll +
            m_nScrollPosx = nNextCaretCharX - (rc.right - m_xMargin - 16 * m_nOneCharDx);
            if (nNextCaretCharX - m_nScrollPosx <= 0) {
                m_nScrollPosx = nNextCaretCharX - rc.width() / 2 - m_xMargin;
            }
            if (m_pHorzScrollBar && m_nScrollPosx > m_pHorzScrollBar->getMax()) {
                m_nScrollPosx = m_pHorzScrollBar->getMax();
            }
        }
        if (nNextCaretCharX < m_nScrollPosx + m_xMargin) {
            // horz scroll -
            m_nScrollPosx = nNextCaretCharX - m_xMargin - 16 * m_nOneCharDx;
            if (m_nScrollPosx < 0) {
                m_nScrollPosx = 0;
            }
        }
    }

    m_nCaretX -= m_nScrollPosx;
    m_nCaretMaxXLatest = m_nCaretX;

    showCaret();

    if (m_pVertScrollBar) {
        m_pVertScrollBar->setScrollPos(m_nTopVisibleLine);
    }
    if (m_pHorzScrollBar) {
        m_pHorzScrollBar->setScrollPos(m_nScrollPosx);
    }
}

void CSkinEditCtrl::setCaretToPos(int x, int y) {
    COneLine *pLine;
    int i;

    x -= m_rcContent.left;
    y -= m_rcContent.top;

    m_nCaretRow = m_nTopVisibleLine + (y - m_yMargin)/ getLineDy();
    assert(!m_vLines.empty());
    if (m_nCaretRow >= (int)m_vLines.size()) {
        m_nCaretRow = (int)m_vLines.size() - 1;
    } else if (m_nCaretRow < 0) {
        m_nCaretRow = 0;
    }

    if (m_pVertScrollBar && m_pVertScrollBar->isEnabled() && m_pVertScrollBar->getPage() <= m_pVertScrollBar->getMax()) {
        if (m_nCaretRow - m_nTopVisibleLine == m_pVertScrollBar->getPage()) {
            m_nTopVisibleLine++;
        } else if (m_nTopVisibleLine - m_nCaretRow == 1) {
            m_nTopVisibleLine--;
        } else if (m_nCaretRow - m_nTopVisibleLine > m_pVertScrollBar->getPage()) {
            m_nTopVisibleLine += (m_nCaretRow - m_nTopVisibleLine) - (m_nCaretRow - m_nTopVisibleLine) % m_pVertScrollBar->getPage();
        } else if (m_nCaretRow < m_nTopVisibleLine) {
            m_nTopVisibleLine += (m_nCaretRow - m_nTopVisibleLine) - (m_nCaretRow - m_nTopVisibleLine) % m_pVertScrollBar->getPage();
        }
    }

    if (m_nTopVisibleLine < 0) {
        m_nTopVisibleLine = 0;
    }

    m_nCaretY = m_yMargin + (m_nCaretRow - m_nTopVisibleLine) * getLineDy();

    x += m_nScrollPosx;

    pLine = m_vLines[m_nCaretRow];
    m_nCaretX = m_xMargin;
    for (i = 0; i < (int)pLine->size(); i++) {
        CGlyph &glyph = (*pLine)[i];

        assert(glyph.width != GLYPH_WIDTH_INVALID);
        if (x <= m_nCaretX + glyph.width / 2) {
            break;
        }
        m_nCaretX += glyph.width;
    }
    m_nCaretMaxXLatest = m_nCaretX;
    m_nCaretCol = i;
}

int CSkinEditCtrl::getEndColPosOfLine(int nLine) {
    assert(nLine >=0 && nLine < (int)m_vLines.size());

    COneLine *pLine;

    pLine = m_vLines[nLine];
    return (int)pLine->size();
}

void CSkinEditCtrl::nextPos(int &row, int &nCol) {
    if (nCol >= getEndColPosOfLine(row)) {
        if (row < (int)m_vLines.size() - 1) {
            row++;
            nCol = 0;
        } else {
            nCol = getEndColPosOfLine(row);
        }
    } else {
        nCol++;
    }
}

void CSkinEditCtrl::prevPos(int &row, int &nCol) {
    if (nCol > 0) {
        nCol--;
    } else {
        if (row > 0) {
            row--;
            nCol = getEndColPosOfLine(row);
        }
    }
}

void CSkinEditCtrl::nextWord(int &row, int &nCol) {
    bool bAlpha, bBegAlpha;
    COneLine *pLine;
    bool bMoved = false;

    pLine = m_vLines[row];

    if (nCol == pLine->size()) {
        bBegAlpha = false;
    } else {
        bBegAlpha = tobool(isalnum((*pLine)[nCol].chGlyph[0])) || (*pLine)[nCol].chGlyph == '_';
    }

    while (1) {
        pLine = m_vLines[row];

        if (nCol == pLine->size()) {
            bAlpha = false;
        } else {
            bAlpha = tobool(isalnum((*pLine)[nCol].chGlyph[0])) || (*pLine)[nCol].chGlyph == '_';
        }

        if (bAlpha != bBegAlpha) {
            if (bBegAlpha) {
                // the space followed by text, should belong to that word.
                while (nCol < (int)pLine->size() && (*pLine)[nCol].chGlyph == ' ') {
                    nCol++;
                }
            }
            break;
        }

        if (nCol >= (int)pLine->size()) {
            if (bMoved) {
                break; // End at the end of line.
            }

            if (row < (int)m_vLines.size() - 1) {
                // End at next line
                row++;
                nCol = 0;
                return;
            } else {
                // end of text.
                return;
            }
        } else {
            nCol++;
            bMoved = true;
        }
    }
}

void CSkinEditCtrl::prevWord(int &row, int &nCol) {
    bool bAlpha, bBegAlpha;
    COneLine *pLine;
    bool bBegSpace;

    prevPos(row, nCol);
    if (row < 0 || row >= m_vLines.size()) {
        return;
    }

    pLine = m_vLines[row];

    if (nCol == pLine->size()) {
        bBegAlpha = false;
        bBegSpace = true;
    } else {
        bBegAlpha = tobool(isalnum((*pLine)[nCol].chGlyph[0])) || (*pLine)[nCol].chGlyph == '_';
        bBegSpace = (*pLine)[nCol].chGlyph == ' ';
    }

    // word ends with space is a whole word.
    while (bBegSpace && nCol > 0) {
        nCol--;
        if ((*pLine)[nCol].chGlyph[0] == ' ') {
            continue;
        } else if (tobool(isalnum((*pLine)[nCol].chGlyph[0])) || (*pLine)[nCol].chGlyph == '_') {
            bBegAlpha = true;
            break;
        } else {
            break;
        }
    }

    while (1) {
        pLine = m_vLines[row];

        if (nCol == pLine->size()) {
            bAlpha = false;
        } else {
            bAlpha = tobool(isalnum((*pLine)[nCol].chGlyph[0])) || (*pLine)[nCol].chGlyph == '_';
        }

        if (bAlpha != bBegAlpha) {
            nextWord(row, nCol);
            break;
        }

        if (nCol > 0) {
            nCol--;
        } else {
            break;
            //             if (row > 0)
            //             {
            //                 row--;
            //                 nCol = m_vLines[row]->size();
            //                 break;
            //             }
            //             else
            //             {
            //                 // begin of text
            //                 return;
            //             }
        }
    }
}

void CSkinEditCtrl::beginOfLine(int row, int &col) {
    assert(row >= 0 && row < m_vLines.size());
    COneLine *line = m_vLines[row];
    for (int i = 0; i < (int)line->size(); i++) {
        auto &glyph = line->at(i);
        if (!glyph.isEqual(' ') && !glyph.isEqual('\t')) {
            if (col > i) {
                col = i;
            } else {
                col = 0;
            }
            return;
        }
    }

    col = 0;
}

bool CSkinEditCtrl::isSelected() {
    return m_nBegSelRow != -1;
}

void CSkinEditCtrl::selectNone() {
    m_nBegSelRow = -1;
    onNotifySelChanged();
}

void CSkinEditCtrl::removeSelected(int &nBegSelRow, int &nBegSelCol) {
    int nEndSelRow = 0, nEndSelCol = 0;
    bool bSelectedEmpty = true;

    if (m_nBegSelRow != -1) {
        bSelectedEmpty = false;
        sortSelectPos(nBegSelRow, nBegSelCol, nEndSelRow, nEndSelCol, bSelectedEmpty);
    }

    if (bSelectedEmpty) {
        selectNone();
        return;
    }

    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    // remove selection
    for (int i = nEndSelRow; i >= nBegSelRow; i--) {
        COneLine *pLine = m_vLines[i];

        if (i == nBegSelRow) {
            if (i == nEndSelRow) {
                pLine->erase(pLine->begin() + nBegSelCol, pLine->begin() + nEndSelCol);
            } else {
                pLine->erase(pLine->begin() + nBegSelCol, pLine->end());

                COneLine *pNextLine = m_vLines[i + 1];
                m_vLines.erase(m_vLines.begin() + i + 1);
                pLine->insert(pLine->end(), pNextLine->begin(), pNextLine->end());
                delete pNextLine;
            }
            updateWidthInfoOfLine(canvas, pLine);
        } else if (i == nEndSelRow) {
            pLine->erase(pLine->begin(), pLine->begin() + nEndSelCol);
            updateWidthInfoOfLine(canvas, pLine);
        } else {
            m_vLines.erase(m_vLines.begin() + i);
            delete pLine;
        }
    }

    onNotifyParseLine(nBegSelRow);

    m_nCaretRow = nBegSelRow;
    m_nCaretCol = nBegSelCol;

    selectNone();
}

void CSkinEditCtrl::removeStr(int nRow, int nCol, int nSize) {
    assert(nRow >= 0 && nRow < (int)m_vLines.size());

    COneLine *pLine = nullptr;

    while (nRow < (int)m_vLines.size() && nSize > 0) {
        pLine = m_vLines[nRow];
        assert(nCol >= 0 && nCol <= (int)pLine->size());

        if (nCol < (int)pLine->size()) {
            if (nCol + nSize >= (int)pLine->size()) {
                nSize -= pLine->size() - nCol;
                pLine->erase(pLine->begin() + nCol, pLine->end());
            } else {
                pLine->erase(pLine->begin() + nCol, pLine->begin() + (nCol + nSize));
                nSize = 0;
            }
        }
        if (nSize > 0 && nCol == pLine->size()) {
            // remove the "\n".
            if (nRow == m_vLines.size() - 1) {
                return;
            }

            COneLine *pNextLine = m_vLines[nRow + 1];

            // nSize -= pLine->getReturnSize();
            nSize--;
            assert(nSize >= 0);

            pLine->insert(pLine->end(), pNextLine->begin(), pNextLine->end());

            m_vLines.erase(m_vLines.begin() + nRow + 1);
            delete pNextLine;
        }
    }

    onNotifyParseLine(nRow);

    if (pLine) {
        CRawGraph *canvas = getMemGraphics();
        canvas->setFont(m_font.getFont());
        updateWidthInfoOfLine(canvas, pLine);
    }
}

void CSkinEditCtrl::removeChar(int nRow, int nCol) {
    assert(nRow >= 0 && nRow < (int)m_vLines.size());

    COneLine *pLine = m_vLines[nRow];
    assert(nCol >= 0 && nCol <= (int)pLine->size());

    if (nCol == pLine->size()) {
        // remove the "\n".
        if (nRow == m_vLines.size() - 1) {
            return;
        }

        COneLine *pNextLine = m_vLines[nRow + 1];
        m_vLines.erase(m_vLines.begin() + nRow + 1);
        pLine->insert(pLine->end(), pNextLine->begin(), pNextLine->end());
        delete pNextLine;
    } else {
        pLine->erase(pLine->begin() + nCol);
    }

    onNotifyParseLine(nRow);

    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    updateWidthInfoOfLine(canvas, pLine);
}

void CSkinEditCtrl::insertStr(cstr_t text, bool isMarkedText) {
    if (isSelected()) {
        replaceSel(text);
        return;
    }

    int begInsertRow = m_nCaretRow, begInsertCol = m_nCaretCol;

    if (m_isMarkedText) {
        m_nBegSelRow = m_begMarkedRow;
        m_nBegSelCol = m_begMarkedCol;
        m_nEndSelRow = m_endMarkedRow;
        m_nEndSelCol = m_endMarkedCol;
        m_isMarkedText = false;

        // 删除 mark 的 text.
        removeSelected(begInsertRow, begInsertCol);
    }

    if (isMarkedText) {
        m_isMarkedText = true;

        m_begMarkedRow = begInsertRow;
        m_begMarkedCol = begInsertCol;
    } else {
        m_undoMgr.addAction(new EditorInsertAction(this, m_nCaretRow, m_nCaretCol, text));
    }

    insertStr(begInsertRow, begInsertCol, text);

    if (isMarkedText) {
        m_endMarkedRow = m_nCaretRow;
        m_endMarkedCol = m_nCaretCol;
    }

    AutoInvalidate updater(this);
    updater.setNeedUpdate();
}

void CSkinEditCtrl::insertChar(int nRow, int nCol, WCHAR chInsert) {
    assert(nRow >= 0 && nRow < (int)m_vLines.size());

    COneLine *pLine = m_vLines[nRow];
    assert(nCol >= 0 && nCol <= (int)pLine->size());

    CGlyph glyph;

    glyph.chGlyph = chInsert;
    glyph.clrIndex = CN_TEXT;

    if (nCol == pLine->size()) {
        pLine->insert(pLine->end(), glyph);
    } else {
        pLine->insert(pLine->begin() + nCol, glyph);
    }

    onNotifyParseLine(nRow);

    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    updateWidthInfoOfLine(canvas, pLine);
}

void CSkinEditCtrl::insertStr(int nRow, int nCol, cstr_t szText) {
    string strSingleLine;

    // insert only first line, if it's single line
    if (isSingleLine()) {
        cstr_t p = szText;
        while (*p && *p != '\r' && *p != '\n') {
            p++;
        }
        strSingleLine.append(szText, p);
        szText = strSingleLine.c_str();
    }

    CRawGraph *canvas = getMemGraphics();
    canvas->setFont(m_font.getFont());

    assert(nRow >= 0 && nRow < (int)m_vLines.size());
    COneLine leftGlyphOfLine;
    COneLine *pLine = m_vLines[nRow];
    assert(nCol >= 0 && nCol <= (int)pLine->size());

    leftGlyphOfLine.insert(leftGlyphOfLine.begin(), pLine->begin() + nCol, pLine->end());
    pLine->erase(pLine->begin() + nCol, pLine->end());

    NewLineType nltInsertLine = pLine->newLineType;

    m_nCaretRow = nRow;
    while (*szText) {
        CGlyph glyph;

        glyph.chGlyph = szText;
        glyph.clrIndex = CN_TEXT;
        if (*szText == '\r') {
            szText++;
            updateWidthInfoOfLine(canvas, pLine);
            if (*szText == '\n') {
                pLine->newLineType = NLT_RN;
            } else {
                pLine->newLineType = NLT_R;
            }
            onNotifyParseLine(m_nCaretRow);
            pLine = newOneLine();
            m_nCaretRow++;
            m_vLines.insert(m_vLines.begin() + m_nCaretRow, pLine);
        } else if (*szText == '\n') {
            updateWidthInfoOfLine(canvas, pLine);
            pLine->newLineType = NLT_N;
            onNotifyParseLine(m_nCaretRow);
            pLine = newOneLine();
            m_nCaretRow++;
            m_vLines.insert(m_vLines.begin() + m_nCaretRow, pLine);
        } else {
            glyph.width = GLYPH_WIDTH_INVALID;
            pLine->push_back(glyph);
        }

        assert(glyph.chGlyph.size() > 0);
        szText += glyph.chGlyph.size();
    }

    pLine->newLineType = nltInsertLine;

    m_nCaretCol = (int)pLine->size();

    pLine->insert(pLine->end(), leftGlyphOfLine.begin(), leftGlyphOfLine.end());

    onNotifyParseLine(m_nCaretRow);

    updateWidthInfoOfLine(canvas, pLine);
}

inline void reduceCol(int &col, int size) {
    col -= size;
    if (col < 0) {
        col = 0;
    }
}

int CSkinEditCtrl::unindentLine(int lineNo) {
    COneLine *line = m_vLines[lineNo];

    int size = 0;

    for (; size < m_indentSize; size++) {
        auto &glyph = line->at(size);
        if (!glyph.isEqual(' ')) {
            break;
        }
    }

    if (size == 0) {
        return 0;
    }

    string text(size, ' ');
    m_undoMgr.addAction(new EditorDelAction(this, lineNo, 0, text.c_str()));

    line->erase(line->begin(), line->begin() + size);

    if (m_nCaretRow == lineNo) {
        // 光标位置需要偏移
        reduceCol(m_nCaretCol, size);
    }

    if (lineNo == m_nBegSelRow) {
        reduceCol(m_nBegSelCol, size);
    }

    if (lineNo == m_nEndSelRow) {
        reduceCol(m_nEndSelCol, size);
    }

    return size;
}

void CSkinEditCtrl::indentLine(int lineNo) {
    COneLine *line = m_vLines[lineNo];

    CGlyph glyph;
    glyph.chGlyph = ' ';
    glyph.clrIndex = CN_TEXT;
    glyph.width = m_nOneCharDx;
    line->insert(line->begin(), m_indentSize, glyph);

    string text(m_indentSize, ' ');
    m_undoMgr.addAction(new EditorInsertAction(this, lineNo, 0, text.c_str()));

    if (m_nCaretRow == lineNo) {
        // 光标位置需要偏移
        m_nCaretCol += m_indentSize;
    }

    if (lineNo == m_nBegSelRow) {
        m_nBegSelCol += m_indentSize;
    }

    if (lineNo == m_nEndSelRow) {
        m_nEndSelCol += m_indentSize;
    }
}

void CSkinEditCtrl::updateScrollInfo(bool bHorz, bool bVert) {
    CRect rc = m_rcContent;

    rc.deflate(m_xMargin, m_yMargin);

    if (bHorz) {
        int wMax = 0, width;
        for (int i = 0; i < (int)m_vLines.size(); i++) {
            COneLine *pLine = m_vLines[i];
            width = 0;

            for (int k = 0; k < (int)pLine->size(); k++) {
                CGlyph &glyph = (*pLine)[k];
                width += glyph.width;
            }
            if (width > wMax) {
                wMax = width;
            }
        }

        int nPage;

        nPage = rc.width() - m_xMargin * 2;
        if (nPage <= 0) {
            nPage = 1;
        }

        if (m_pHorzScrollBar) {
            if (wMax > nPage) {
                m_pHorzScrollBar->setScrollInfo(0, wMax, nPage, m_pHorzScrollBar->getScrollPos(), m_nOneCharDx);
            } else {
                m_nScrollPosx = 0;
                m_pHorzScrollBar->disableScrollBar();
            }
        }
    }

    if (bVert) {
        int nPage;

        nPage = rc.height() / getLineDy();
        if (nPage <= 0) {
            nPage = 2;
        }

        if (m_pVertScrollBar) {
            if ((int)m_vLines.size() > nPage) {
                m_pVertScrollBar->setScrollInfo(0, (int)m_vLines.size(), nPage, m_nTopVisibleLine);
            } else {
                m_pVertScrollBar->disableScrollBar();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
// CUIObject messages to draw or handle user input, etc.
//
//////////////////////////////////////////////////////////////////////////

void CSkinEditCtrl::onSize() {
    CSkinScrollFrameCtrlBase::onSize();

    m_caret.setOffset(m_rcContent.left, m_rcContent.top);

    updateScrollInfo();

    CRect rc = m_rcContent;

    rc.deflate(m_xMargin, m_yMargin);

    if (!rc.ptInRect(CPoint(m_nCaretX + rc.left, m_nCaretY + rc.top))) {
        hideCaret();
    }
}

void CSkinEditCtrl::onSetFocus() {
    m_pSkin->startTextInput();

    CRect rc = m_rcContent;

    rc.deflate(m_xMargin, m_yMargin);

    if (rc.ptInRect(CPoint(m_nCaretX + rc.left, m_nCaretY + rc.top))) {
        showCaret();
    }

    invalidate();
}

void CSkinEditCtrl::onKillFocus() {
    m_pSkin->endTextInput();

    m_bInMouseSel = false;

    hideCaret();

    if (m_pEditNotification) {
        m_pEditNotification->onEditorKillFocus();
    }

    invalidate();
}

bool CSkinEditCtrl::onLButtonDown(uint32_t nFlags, CPoint point) {
    if (CSkinScrollFrameCtrlBase::onLButtonDown(nFlags, point)) {
        return true;
    }

    setCursor(m_cursor);

    bool control = isModifierKeyPressed(MK_CONTROL, nFlags);

    m_bInMouseSel = false;

    setFocus();

    if (control) {
        // Select the word of cursor
        setCaretToPos(point.x, point.y);

        m_nBegSelRow = m_nCaretRow;
        m_nBegSelCol = m_nCaretCol;
        prevWord(m_nBegSelRow, m_nBegSelCol);

        m_nEndSelRow = m_nBegSelRow;
        m_nEndSelCol = m_nBegSelCol;
        nextWord(m_nEndSelRow, m_nEndSelCol);

        m_nCaretRow = m_nEndSelRow;
        m_nCaretCol = m_nEndSelCol;

        makeCaretInSight();
    } else {
        bool shift = isModifierKeyPressed(MK_SHIFT, nFlags);

        if (shift && !isSelected()) {
            // Select text from the current caret.
            m_nBegSelRow = m_nCaretRow;
            m_nBegSelCol = m_nCaretCol;
        }

        setCaretToPos(point.x, point.y);
        makeCaretInSight();

        if (shift) {
            // Select text to the current caret now.
            m_nEndSelRow = m_nCaretRow;
            m_nEndSelCol = m_nCaretCol;
        }

        if (!shift && isSelected()) {
            selectNone();
        }
    }

    invalidate();
    return true;
}

bool CSkinEditCtrl::onLButtonDblClk(uint32_t nFlags, CPoint point) {
    if (CSkinScrollFrameCtrlBase::onLButtonDblClk(nFlags, point)) {
        return true;
    }
    if (!m_rcContent.ptInRect(point)) {
        return false;
    }

    setCursor(m_cursor);

    m_bInMouseSel = true;

    setFocus();

    // Select the word of cursor
    setCaretToPos(point.x, point.y);

    m_nEndSelRow = m_nCaretRow;
    m_nEndSelCol = m_nCaretCol;
    nextWord(m_nEndSelRow, m_nEndSelCol);

    m_nBegSelRow = m_nEndSelRow;
    m_nBegSelCol = m_nEndSelCol;
    prevWord(m_nBegSelRow, m_nBegSelCol);

    m_nCaretRow = m_nEndSelRow;
    m_nCaretCol = m_nEndSelCol;

    makeCaretInSight();
    invalidate();

    return true;
}

bool CSkinEditCtrl::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (CSkinScrollFrameCtrlBase::onLButtonUp(nFlags, point)) {
        return true;
    }

    setCursor(m_cursor);

    m_pSkin->releaseCapture();

    m_bInMouseSel = false;
    if (m_nBegSelRow == m_nEndSelRow && m_nBegSelCol == m_nEndSelCol) {
        m_nBegSelRow = -1;
    }

    onNotifySelChanged();

    return true;
}

bool CSkinEditCtrl::onMouseDrag(CPoint point) {
    if (CSkinScrollFrameCtrlBase::onMouseDrag(point)) {
        return true;
    }

    setCursor(m_cursor);

    bool shift = isModifierKeyPressed(MK_SHIFT, 0);

    if (shift) {
        if (!isSelected()) {
            // Select text from the current caret.
            m_nBegSelRow = m_nCaretRow;
            m_nBegSelCol = m_nCaretCol;
        }

        setCaretToPos(point.x, point.y);

        makeCaretInSight();

        showCaret();

        // Select text to the current caret now.
        m_nEndSelRow = m_nCaretRow;
        m_nEndSelCol = m_nCaretCol;
        return true;
    }

    // mouse select
    if (!m_bInMouseSel) {
        m_bInMouseSel = true;

        m_nBegSelRow = m_nCaretRow;
        m_nBegSelCol = m_nCaretCol;

        setCaretToPos(point.x, point.y);

        m_nEndSelRow = m_nCaretRow;
        m_nEndSelCol = m_nCaretCol;
    } else {
        if (!isSelected()) {
            // Select text from the current caret.
            m_nBegSelRow = m_nCaretRow;
            m_nBegSelCol = m_nCaretCol;
        }

        setCaretToPos(point.x, point.y);

        m_nEndSelRow = m_nCaretRow;
        m_nEndSelCol = m_nCaretCol;
    }

    makeCaretInSight();
    showCaret();
    invalidate();

    return true;
}

bool CSkinEditCtrl::onMouseMove(CPoint point) {
    if (CSkinScrollFrameCtrlBase::onMouseMove(point)) {
        return true;
    }

    setCursor(m_cursor);

    return true;
}

bool CSkinEditCtrl::onRButtonUp(uint32_t nFlags, CPoint point) {
    if (CSkinScrollFrameCtrlBase::onRButtonUp(nFlags, point)) {
        return true;
    }

    setCursor(m_cursor);

    m_pSkin->clientToScreen(point);
    onContexMenu(point.x, point.y);

    return true;
}

void CSkinEditCtrl::onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) {
    if (isSingleLine() && m_pEditNotification && !isFlagSet(nMkeys, MK_SHIFT)) {
        m_pEditNotification->onEditorMouseWheel(nWheelDistance, nMkeys, pt);
        return;
    }

    if (!m_pVertScrollBar || !m_pHorzScrollBar) {
        return;
    }

    int nOffset = nWheelDistance;
    if (isFlagSet(nMkeys, MK_SHIFT)) {
        // horz scroll bar
        int pos = m_pHorzScrollBar->getScrollPos() + nOffset * m_nOneCharDx;
        if (pos < 0) {
            pos = 0;
        } else if (pos > m_pHorzScrollBar->getMax()) {
            pos = m_pHorzScrollBar->getMax();
        }
        if (pos == m_pHorzScrollBar->getScrollPos()) {
            return;
        }

        m_pHorzScrollBar->setScrollPos(pos, true);

        if (m_nScrollPosx != m_pHorzScrollBar->getScrollPos()) {
            m_nCaretX += m_nScrollPosx - m_pHorzScrollBar->getScrollPos();
            m_nScrollPosx = m_pHorzScrollBar->getScrollPos();

            if (m_nCaretX - m_nScrollPosx - m_xMargin >= m_pHorzScrollBar->getPage()
                || m_nCaretX < m_xMargin) {
                hideCaret();
            } else {
                showCaret();
            }

            invalidate();
        }
    } else {
        if (m_pVertScrollBar->getScrollPos() + nOffset < 0) {
            return;
        }
        if (m_pVertScrollBar->getScrollPos() + nOffset > m_pVertScrollBar->getMax()) {
            return;
        }
        m_pVertScrollBar->setScrollPos(m_pVertScrollBar->getScrollPos() + nOffset, true);

        if (m_nTopVisibleLine != m_pVertScrollBar->getScrollPos()) {
            m_nTopVisibleLine = m_pVertScrollBar->getScrollPos();
            if (m_nTopVisibleLine >= (int)m_vLines.size()) {
                m_nTopVisibleLine = (int)m_vLines.size() - 1;
            }
            if (m_nTopVisibleLine < 0) {
                m_nTopVisibleLine = 0;
            }

            if (m_nCaretRow - m_nTopVisibleLine >= m_pVertScrollBar->getPage()
                || m_nTopVisibleLine > m_nCaretRow) {
                hideCaret();
            } else {
                showCaret();
            }

            invalidate();
        }
    }
}

bool CSkinEditCtrl::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    bool alt = isModifierKeyPressed(MK_ALT, nFlags);
    bool shift = isModifierKeyPressed(MK_SHIFT, nFlags);
#ifdef _MAC_OS
    bool ctrl = isModifierKeyPressed(MK_COMMAND, nFlags);
#else
    bool ctrl = isModifierKeyPressed(MK_CONTROL, nFlags);
#endif

    bool bSelKey = false;
    int nBegSelRow = m_nCaretRow, nBegSelCol = m_nCaretCol;
    AutoInvalidate updater(this);
    int nCaretMaxXLatestOrg = m_nCaretMaxXLatest;
    bool bRecoverCaretMaxXLatestOrg = false;

    switch (nChar) {
    case VK_A:
        {
            // select all
            if (ctrl) {
                m_nBegSelRow = 0;
                m_nBegSelCol = 0;
                m_nEndSelRow = (int)m_vLines.size() - 1;
                assert(m_vLines.size());
                m_nEndSelCol = (int)m_vLines[m_vLines.size() - 1]->size();
            }
            break;
        }
        break;
    case VK_Z:
        {
            // undo
            if (ctrl) {
                if (shift) {
                    if (m_undoMgr.canRedo()) {
                        m_undoMgr.redo();
                    }
                } else {
                    if (m_undoMgr.canUndo()) {
                        m_undoMgr.undo();
                    }
                }
            }
            break;
        }
    case VK_Y:
        if (ctrl) {
            if (m_undoMgr.canRedo()) {
                m_undoMgr.redo();
                updater.setNeedUpdate();
            }
        }
        break;
    case VK_X:
        {
            // cut
            if (ctrl) {
                onCmdCut();
            }
            // just return, the OnCmdXXX will update the view.
            return true;
        }
    // case VK_INSERT:
    //     {
    //         // paste
    //         if (shift) {
    //             onCmdPaste();
    //         }
    //         // just return, the OnCmdXXX will update the view.
    //         return;
    //     }
    case VK_V:
        {
            // paste
            if (ctrl) {
                onCmdPaste();
            }
            // just return, the OnCmdXXX will update the view.
            return true;
        }
    case VK_C:
        {
            // copy
            if (ctrl) {
                onCmdCopy();
            }
            // just return, the OnCmdXXX will update the view.
            return true;
        }
    case VK_END:
        {
            bSelKey = true;
            if (ctrl) {
                // end of text
                assert(m_vLines.size());
                m_nCaretRow = (int)m_vLines.size() - 1;
                m_nCaretCol = getEndColPosOfLine(m_nCaretRow);
            } else {
                // end of line
                m_nCaretCol = getEndColPosOfLine(m_nCaretRow);
            }
            break;
        }
    case VK_HOME:
        {
            bSelKey = true;
            if (ctrl) {
                // begin of text
                m_nCaretRow = 0;
                m_nCaretCol = 0;
            } else {
                // begin of line
                m_nCaretCol = getCaretLineHomePos();
            }
            break;
        }
    case VK_LEFT: // left arrow
        {
            bSelKey = true;
            if (ctrl) {
                // to begin of line
                beginOfLine(m_nCaretRow, m_nCaretCol);
            } else if (alt) {
                // to prev word
                prevWord(m_nCaretRow, m_nCaretCol);
            } else {
                prevPos(m_nCaretRow, m_nCaretCol);
            }
            break;
        }
    case VK_RIGHT: // right arrow
        {
            bSelKey = true;
            if (ctrl) {
                // to end of line
                m_nCaretCol = (int)m_vLines[m_nCaretRow]->size();
            } else if (alt) {
                // to next word
                nextWord(m_nCaretRow, m_nCaretCol);
            } else {
                if (isSelected()) {
                    m_nCaretRow = m_nEndSelRow;
                    m_nCaretCol = m_nEndSelCol;
                }
                nextPos(m_nCaretRow, m_nCaretCol);
            }
            break;
        }
    case VK_UP: // up arrow
        {
            if (isSingleLine() && m_pEditNotification) {
                return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
            }

            bSelKey = true;

            if (ctrl) {
                m_nCaretRow = 0;
                m_nCaretCol = 0;
            } else if (m_nCaretRow > 0) {
                bRecoverCaretMaxXLatestOrg = true;
                setCaretToPos(m_rcContent.left + m_nCaretMaxXLatest, m_rcContent.top + m_nCaretY - getLineDy());
            }
            break;
        }
    case VK_DOWN: // down arrow
        {
            if (isSingleLine() && m_pEditNotification) {
                return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
            }

            bSelKey = true;
            if (ctrl) {
                m_nCaretRow = (int)m_vLines.size() - 1;
                m_nCaretCol = (int)m_vLines.back()->size();
            } else if (m_nCaretRow < (int)m_vLines.size() - 1) {
                bRecoverCaretMaxXLatestOrg = true;
                setCaretToPos(m_rcContent.left + m_nCaretMaxXLatest, m_rcContent.top + m_nCaretY + getLineDy());
            }
            break;
        }
    case VK_PAGE_UP: // page up
        {
            if (isSingleLine() && m_pEditNotification) {
                return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
            }

            bSelKey = true;
            if (m_nCaretRow > 0 && m_pVertScrollBar&& m_pVertScrollBar->isEnabled()) {
                bRecoverCaretMaxXLatestOrg = true;
                setCaretToPos(m_rcContent.left + m_nCaretMaxXLatest, m_rcContent.top + m_nCaretY - getLineDy() * (m_pVertScrollBar->getPage() - 1));
                m_nTopVisibleLine -= m_pVertScrollBar->getPage() - 1;
                if (m_nTopVisibleLine < 0) {
                    m_nTopVisibleLine = 0;
                }
            }
            break;
        }
    case VK_PAGE_DOWN: // page down
        {
            if (isSingleLine() && m_pEditNotification) {
                return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
            }

            bSelKey = true;
            if (m_nCaretRow < (int)m_vLines.size() && m_pVertScrollBar && m_pVertScrollBar->isEnabled()) {
                bRecoverCaretMaxXLatestOrg = true;
                setCaretToPos(m_rcContent.left + m_nCaretMaxXLatest, m_rcContent.top + m_nCaretY + getLineDy() * (m_pVertScrollBar->getPage() - 1));
                m_nTopVisibleLine += m_pVertScrollBar->getPage() - 1;
                if (m_nTopVisibleLine > m_pVertScrollBar->getMax()) {
                    m_nTopVisibleLine = m_pVertScrollBar->getMax();
                }
            }
            break;
        }
    case VK_TAB:
        if (!ctrl) {
            if (isSingleLine()) {
                // 单行编辑，不处理 tab key
                return false;
            } else if (m_nBegSelRow == -1) {
                // 未选中
                if (shift) {
                    AutoBatchUndo batchUndo(this);
                    unindentLine(m_nCaretRow);
                } else {
                    int count = m_indentSize - m_nCaretCol % m_indentSize;
                    string text(count, ' ');
                    insertStr(text.c_str());
                }
            } else if (m_nBegSelRow == m_nEndSelRow && !shift) {
                // 单行选中，替换当前选中的内容
                int count = m_indentSize - m_nBegSelCol % m_indentSize;
                string text(count, ' ');
                replaceSel(text.c_str());
                setCaret(m_nCaretRow, m_nBegSelCol + count);
            } else {
                // 多行选中，选中的所有行都需要偏移
                AutoBatchUndo batchUndo(this);

                int start = min(m_nBegSelRow, m_nEndSelRow);
                int end = max(m_nBegSelRow, m_nEndSelRow);
                if (shift) {
                    // 缩进
                    for (int i = start; i <= end; i++) {
                        unindentLine(i);
                    }
                } else {
                    // 添加
                    for (int i = start; i <= end; i++) {
                        indentLine(i);
                    }
                }
            }
        }
        break;
    case VK_BACK:
        {
            if (!isReadOnly()) {
                if (isSelected()) {
                    removeSel();
                } else if (m_nCaretRow != 0 || m_nCaretCol != 0) {
                    string strToDel;
                    StashSelectionCaret selectionCaretBefore(this);

                    if (ctrl) {
                        m_nBegSelRow = m_nCaretRow;
                        m_nBegSelCol = m_nCaretCol;
                        m_nEndSelRow = m_nCaretRow;
                        m_nEndSelCol = m_nCaretCol;
                        prevWord(m_nEndSelRow, m_nEndSelCol);

                        selectionToText(strToDel);
                        removeSelected(nBegSelRow, nBegSelCol);
                        m_nBegSelRow = -1;
                    } else {
                        prevPos(m_nCaretRow, m_nCaretCol);
                        getStrAtPos(m_nCaretRow, m_nCaretCol, strToDel);
                        removeChar(m_nCaretRow, m_nCaretCol);
                    }
                    auto action = new EditorDelAction(this, m_nCaretRow, m_nCaretCol, strToDel.c_str());
                    action->setStashSelectionCaretBefore(selectionCaretBefore);
                    m_undoMgr.addAction(action);
                }
                updater.setNeedUpdate();
            }
            break;
        }
    // case VK_DELETE:
    //     {
    //         if (!isReadOnly()) {
    //             if (shift) {
    //                 // shift + VK_DELETE = cut
    //                 onCmdCut();
    //                 // just return, the OnCmdXXX will update the view.
    //                 return;
    //             } else {
    //                 onCmdDelete();
    //             }
    //         }
    //         break;
    //     }
    case VK_RETURN:
        {
            if (isSingleLine()) {
                if (m_pEditNotification) {
                    if (ctrl) {
                        return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
                    } else if (shift) {
                        return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
                    } else {
                        return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
                    }
                }

                if (m_cmdSubmit != ID_INVALID) {
                    m_pSkin->postCustomCommandMsg(m_cmdSubmit);
                }
                break;
            }

            if (!isReadOnly()) {
                if (isSelected()) {
                    replaceSel("\r\n");
                } else {
                    insertStr("\r\n");
                }

                updater.setNeedUpdate();
            }

            break;
        }
    default:
        if (m_pEditNotification) {
            return m_pEditNotification->onEditorKeyDown(nChar, nFlags);
        }
        return false;
    }

    if (bSelKey) {
        if (shift) {
            if (!isSelected()) {
                m_nBegSelRow = nBegSelRow;
                m_nBegSelCol = nBegSelCol;
            }

            // Select text to the current caret now.
            m_nEndSelRow = m_nCaretRow;
            m_nEndSelCol = m_nCaretCol;

            onNotifySelChanged();
        } else {
            selectNone();
        }
    }

    // Where is Caret? Make it in sight
    makeCaretInSight();

    if (bRecoverCaretMaxXLatestOrg) {
        m_nCaretMaxXLatest = nCaretMaxXLatestOrg;
    }

    return true;
}

void CSkinEditCtrl::onChar(uint32_t nChar) {
    if (nChar <= 127 && nChar != '\t' && !isprint(nChar)) {
        return;
    }

    string text;
    WCHAR wchar = nChar;
    ucs2ToUtf8(&wchar, 1, text);

    if (isSelected()) {
        replaceSel(text.c_str());
    } else {
        insertStr(text.c_str());
    }
}

void addMenuItem(rapidjson::Document &doc, const char *name, const char *idName) {
    rapidjson::Value item;
    item.SetArray();
    if (name == nullptr) {
        item.PushBack(rapidjson::Value("separator", doc.GetAllocator()), doc.GetAllocator());
    } else {
        item.PushBack(rapidjson::Value(name, doc.GetAllocator()), doc.GetAllocator());
        item.PushBack(rapidjson::Value(idName, doc.GetAllocator()), doc.GetAllocator());
    }
    doc.PushBack(item, doc.GetAllocator());
}

void CSkinEditCtrl::onContexMenu(int xPos, int yPos) {
    CSkinMenu menu;

    rapidjson::Document doc;
    doc.SetArray();

    addMenuItem(doc, _TL("&Undo"), "ID_EDIT_UNDO");
    addMenuItem(doc, _TL("&Redo"), "ID_EDIT_REDO");
    addMenuItem(doc, nullptr, nullptr);
    addMenuItem(doc, _TL("Cu&t"), "ID_EDIT_CUT");
    addMenuItem(doc, _TL("&Copy"), "ID_EDIT_COPY");
    addMenuItem(doc, _TL("&Paste"), "ID_EDIT_PASTE");
    addMenuItem(doc, _TL("&Delete"), "ID_EDIT_DELETE");

    menu.loadMenu(doc.GetArray());

    bool bEnable = isSelected();
    menu.enableItem(ID_EDIT_CUT, bEnable);
    menu.enableItem(ID_EDIT_COPY, bEnable);

    bEnable = m_undoMgr.canUndo();
    menu.enableItem(ID_EDIT_UNDO, bEnable);

    bEnable = m_undoMgr.canRedo();
    menu.enableItem(ID_EDIT_REDO, bEnable);

    m_pSkin->registerHandleContextMenuCmd(this);

    menu.trackPopupMenu(xPos, yPos, m_pSkin);
}

void CSkinEditCtrl::onTimer(int nId) {
    if (nId == m_nIDTimerCaret) {
        if (!isVisible() || !isParentVisible()) {
            m_pSkin->unregisterTimerObject(this, m_nIDTimerCaret);
            m_caret.hideCaret();
            return;
        }

        // Redraw flashing caret
        CRawGraph *canvas;

        canvas = m_pContainer->getMemGraph();
        assert(canvas);
        m_caret.drawFlash(canvas);

        updateRectToScreen(&m_caret.getUpdateRect());
    } else {
        CSkinScrollFrameCtrlBase::onTimer(nId);
    }
}

bool CSkinEditCtrl::onCommand(uint32_t nId) {
    switch (nId) {
    case ID_EDIT_UNDO:
        onCmdUndo();
        break;
    case ID_EDIT_REDO:
        onCmdRedo();
        break;
    case ID_EDIT_CUT:
        onCmdCut();
        break;
    case ID_EDIT_COPY:
        onCmdCopy();
        break;
    case ID_EDIT_PASTE:
        onCmdPaste();
        break;
    case ID_EDIT_DELETE:
        onCmdDelete();
        break;
    default:
        return false;
    }

    return CSkinScrollFrameCtrlBase::onCommand(nId);
}

void CSkinEditCtrl::onAdjustHue(float hue, float saturation, float luminance) {
    CSkinScrollFrameCtrlBase::onAdjustHue(hue, saturation, luminance);

    m_font.onAdjustHue(m_pSkin, hue, saturation, luminance);
}

void CSkinEditCtrl::onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) {
    if (!m_pVertScrollBar) {
        return;
    }

    // changes...
    m_nTopVisibleLine = m_pVertScrollBar->getScrollPos();
    if (m_nTopVisibleLine < 0) {
        m_nTopVisibleLine = 0;
    }

    if (m_nCaretRow - m_nTopVisibleLine >= m_pVertScrollBar->getPage()
        || m_nTopVisibleLine > m_nCaretRow) {
        hideCaret();
    } else {
        showCaret();
    }

    invalidate();
}

void CSkinEditCtrl::onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) {
    if (!m_pHorzScrollBar) {
        return;
    }

    // changes...
    m_nCaretX += m_nScrollPosx - m_pHorzScrollBar->getScrollPos();
    m_nScrollPosx = m_pHorzScrollBar->getScrollPos();

    if (m_nCaretX - m_nScrollPosx - m_xMargin >= m_pHorzScrollBar->getPage()
        || m_nCaretX < m_xMargin) {
        hideCaret();
    } else {
        showCaret();
    }

    invalidate();
}

void CSkinEditCtrl::onInputText(cstr_t text) {
    if (isReadOnly()) {
        return;
    }

    insertStr(text);
}

void CSkinEditCtrl::onInputMarkedText(cstr_t text) {
    // MarkedText 是输入法输入的临时字符串，会被后期的 InputText 替换掉.
    if (isReadOnly()) {
        return;
    }

    insertStr(text, true);
}

//
// IUndoMgrNotify interface
//
void CSkinEditCtrl::onStatusChanged(Status status, bool bVal) {
    if (!m_pEditNotification) {
        return;
    }

    IEditNotification::Status s;

    switch (status) {
    case S_CAN_UNDO:
        s = IEditNotification::S_CAN_UNDO;
        break;
    case S_CAN_REDO:
        s = IEditNotification::S_CAN_REDO;
        break;
    default:
        return;
    }

    m_pEditNotification->onEditorTextChanged(s, bVal);
}


void CSkinEditCtrl::onAction(Action action) {
    if (!m_pEditNotification) {
        return;
    }

    m_pEditNotification->onEditorTextChanged();
}

IdToString __EditStyleText[] = {
    { CSkinEditCtrl::S_MULTILINE, "MULTILINE" },
    { CSkinEditCtrl::S_PASSWORD, "PASSWORD" },
    { CSkinEditCtrl::S_READ_ONLY, "READ_ONLY" },
    { 0, nullptr },
};

bool CSkinEditCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CSkinScrollFrameCtrlBase::setProperty(szProperty, szValue)) {
        return true;
    }

    if (m_font.setProperty(szProperty, szValue)) {
        return true;
    } else if (isPropertyName(szProperty, "XMargin")) {
        m_xMargin = atoi(szValue);
    } else if (isPropertyName(szProperty, "YMargin")) {
        m_yMargin = atoi(szValue);
    } else if (isPropertyName(szProperty, "Style")) {
        m_nEditorStyles = getCombinationValue(__EditStyleText, szValue);
    } else if (isPropertyName(szProperty, "PlaceHolder")) {
        m_placeHolderEnglish = szValue;
        m_placeHolder = _TL(szValue);
    } else if (isPropertyName(szProperty, "PlaceHolderColor")) {
        m_clrPlaceHolder = parseColorString(szValue);
    } else if (isPropertyName(szProperty, "SubmitCmd")) {
        if (isTRUE(szValue)) {
            m_cmdSubmit = ID_OK;
        } else if (strIsSame(szValue, SZ_FALSE)) {
            m_cmdSubmit = ID_INVALID;
        } else {
            m_cmdSubmit = getIDByName(szValue);
        }
    } else {
        return false;
    }

    return true;
}

string &CSkinEditCtrl::getText() {
    m_strText.resize(0);
    CSkinEditCtrl::getText(m_strText);

    return m_strText;
}

#ifdef _SKIN_EDITOR_
void CSkinEditCtrl::enumProperties(CUIObjProperties &listProperties) {
    CSkinScrollFrameCtrlBase::enumProperties(listProperties);

    m_font.enumProperties(listProperties);
}
#endif // _SKIN_EDITOR_
