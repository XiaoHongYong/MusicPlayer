#pragma once

#include "SkinScrollFrameCtrlBase.h"
#include "UndoMgr.h"
#include "../Utils/StringIterator.h"


class CSkinEditCtrl;

namespace _SkinEditCtrl {

class StashSelectionCaret;
class AutoBatchUndo;
class EditorInsertAction;
class EditorBatchAction;

class AutoInvalidate {
public:
    AutoInvalidate(CSkinEditCtrl *editor) : m_editor(editor) { saveOrg(); }
    ~AutoInvalidate();

    void setNeedUpdate() { m_needUpdate = true; }

protected:
    void saveOrg();

protected:
    CSkinEditCtrl               *m_editor;
    bool                        m_needUpdate = false;
    int                         m_nTopVisibleLineOld;
    int                         m_nScrollPosxOld;
    int                         m_nBegSelRowOld, m_nEndSelRowOld;
    int                         m_nBegSelColOld, m_nEndSelColOld;
    int                         m_nCaretRowOld;

};

class AutoBatchUndo {
public:
    AutoBatchUndo(CSkinEditCtrl *editor);
    ~AutoBatchUndo() { endBatchUndo(); }

    void endBatchUndo();

protected:
    CSkinEditCtrl               *m_editor;
    EditorBatchAction           *m_batchAction;
    AutoInvalidate              m_update;

};

} // namespace _SkinEditCtrl

//
// Editor syntax (color) parser
//
class IEditSyntaxParser {
public:
    virtual ~IEditSyntaxParser() { }
    virtual void init(CSkinEditCtrl *pEdit) = 0;
    virtual void onNotifyParseLine(int nLine) = 0;

};


class IEditNotification {
public:
    enum Status {
        S_CAN_UNDO,
        S_CAN_REDO,
        S_SEL,
    };

    virtual void onEditorTextChanged(Status status, bool bVal) { }
    virtual void onEditorTextChanged() { }

    virtual bool onEditorKeyDown(uint32_t code, uint32_t flags) { return false; }
    virtual void onEditorMouseWheel(int wheelDistance, int mkeys, CPoint pt) { }
    virtual void onEditorKillFocus() { }

};

#define CARET_EXTRA_EXPAND  1

class CSkinCaret {
public:
    CSkinCaret() {
        m_bOldDrawed = false;
        m_rcCaret.setEmpty();
        m_bOn = false;
        m_clrCaret.set(RGB(255, 255, 255));
        m_xOffset = 0;
        m_yOffset = 0;
    }

    virtual ~CSkinCaret() { }

    bool isOn() { return m_bOn; }

    bool isDrawed() { return m_bOldDrawed; }

    virtual void drawFlash(CRawGraph *canvas) {
        if (!m_bOn) {
            return;
        }

        canvas->fillRectXOR(m_rcCaret, m_clrCaret);
        m_bOldDrawed = !m_bOldDrawed;
    }

    virtual void draw(CRawGraph *canvas) {
        if (!m_bOn) {
            return;
        }

        if (m_bOldDrawed) {
            canvas->fillRectXOR(m_rcCaret, m_clrCaret);
        }
    }

    virtual void showCaret(int x, int y, int nWidth, int nHeight) {
        m_bOn = true;
        x += m_xOffset;
        y += m_yOffset;
        m_rcCaret.setLTRB(x, y, x + nWidth, y + nHeight + CARET_EXTRA_EXPAND);
    }

    void hideCaret() {
        m_bOn = false;
        m_bOldDrawed = false;
    }

    void setOffset(int xOffset, int yOffset) {
        if (xOffset != m_xOffset || m_yOffset != yOffset) {
            m_rcCaret.offsetRect(xOffset - m_xOffset, yOffset - m_yOffset);
            m_xOffset = xOffset;
            m_yOffset = yOffset;
        }
    }

    const CRect & getUpdateRect() { return m_rcCaret; }

protected:
    bool                        m_bOn;
    CColor                      m_clrCaret;

    CRect                       m_rcCaret;

    bool                        m_bOldDrawed;
    int                         m_xOffset, m_yOffset;

};


class CSkinEditCtrl : public CSkinScrollFrameCtrlBase, public IUndoMgrNotify {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinScrollFrameCtrlBase)
public:
    enum ColorName {
        CN_BG,
        CN_TEXT,
        CN_SEL_BG,
        CN_SEL_TEXT,
        CN_EDIT_LINE_BG,
        CN_EDIT_LINE_TEXT,
        CN_CUSTOMIZED_START,

        CN_MAX                      = 255
    };

    enum {
        TIMER_DURATION_CARET        = 600
    };

public:

    struct CGlyph {
        uint8_t                     width;
        uint8_t                     clrIndex;           // color index in CSkinEditCtrl color table.
        StringIterator::CharType    chGlyph;

        bool isEqual(char ch) { return chGlyph == ch; }
        bool cmp(const char *str) { return chGlyph == *str; }
        bool iCmp(const char *str);
        bool reverseCmp(const char *str) { return chGlyph == *str; }
        bool reverseiCmp(const char *str);
        int length() { return 1; }
    };

    enum Style {
        S_MULTILINE                 = 1, // Multiple line editor
        S_READ_ONLY                 = 1 << 0x1, // The text can't be modified if this flag is set.
        S_PASSWORD                  = 1 << 0x2,
    };

    enum NewLineType {
        NLT_NONE,
        NLT_RN,
        NLT_N,
        NLT_R,
    };

    class COneLine : public vector<CGlyph> {
    public:
        COneLine() {
            newLineType = NLT_NONE;
        }
        int getReturnSize() {
            switch (newLineType) {
            case NLT_N:
            case NLT_R:
                return 1;
            case NLT_RN:
                return 2;
            default:
                break;
            }
            return 0;
        }

        static cstr_t getReturnStr(NewLineType nlt) {
            switch (nlt) {
                case NLT_N: return "\n";
                case NLT_R: return "\r";
                case NLT_RN: return "\r\n";
            default:
                break;
            }
            return "";
        }

        NewLineType                 newLineType;
    };

    typedef vector<COneLine*>    CVLines;

public:
    CSkinEditCtrl();
    virtual ~CSkinEditCtrl();

    //
    // Editor APIs
    //

    void setEditSyntaxParser(IEditSyntaxParser *pEditSyntaxParser) { m_pEditSyntaxParser = pEditSyntaxParser; }

    void setEditNotification(IEditNotification *pEditNotification) { m_pEditNotification = pEditNotification; }

    void replaceSel(cstr_t szText);
    void removeSel();

    void setSel(int nBeg, int nEnd);
    void setSel(int nBegLine, int nBegCol, int nEndLine, int nEndCol);
    void setSelOfLine(int nLine, int nCol, int nNumOfSel);

    void setCaret(int nRow, int nCol);
    void getCaret(int &nLine, int &nCol) { nLine = m_nCaretRow; nCol = m_nCaretCol; }

    void onFontChanged();
    void setSelColor(const CColor &clrSelText, const CColor &clrSelBg)
        { setColor(CN_SEL_TEXT, clrSelText); setColor(CN_SEL_BG, clrSelBg); }

    void setStyle(uint32_t nStyles) { m_nEditorStyles = nStyles; }
    uint32_t getStyle() const { return m_nEditorStyles; }

    virtual void setText(cstr_t szText) override;

    int getText(string &str);

    void selectionToText(string &str);

    int getLength();

    uint32_t getLineCount() const { return (uint32_t)m_vLines.size(); }

    bool getTextOfLine(int nLine, string &strLine);
    bool setTextOfLine(int nLine, cstr_t szLine);

    bool isSelected();

    bool canUndo();
    bool canRedo();

    bool isInBatchAction() const { return m_undoMgr.isInBatchAction(); }

    void onCmdCopy();
    void onCmdCut();
    void onCmdPaste();
    void onCmdRedo();
    void onCmdUndo();
    virtual void onCmdDelete();

public:
    virtual COneLine *newOneLine() { return new COneLine; }

protected:
    virtual int getFontHeight() { return m_font.getHeight(); }

    virtual CColor & getColor(int nColorName);
    virtual void setColor(int nColorName, const CColor & clr);

    // Alpha of background, used to implement translucent effect.
    virtual uint8_t getBgAlpha();
    virtual CRawGraph *getMemGraphics() {
        CRawGraph *canvas = m_pContainer->getMemGraph();
        canvas->setFont(m_font.getFont());

        return canvas;
    }

    void updateRectToScreen(const CRect *rc);

    void fillGraph(CRawGraph *canvas, const CRect &rc, int nColorName);
    void fillGraph(CRawGraph *canvas, int x, int y, int nWidth, int nHeight, int nColorName)
        { fillGraph(canvas, CRect(x, y, x + nWidth, y + nHeight), nColorName); }
    void fillSelectedLineBg(CRawGraph *canvas, int x, int y, COneLine *pLine, int nBegSelCol, int nEndSelCol);

    void drawLine(CRawGraph *canvas, COneLine *pLine, int x, int xMax, int y);
    void updateWidthInfoOfLine(CRawGraph *canvas, COneLine *pLine);
    void updateWidthOfAllLines();

    bool doSetText(cstr_t szText);

    void clear();
    void clearWidth();

    int getLineDy() { return (m_nLineSpace + getFontHeight()); }

    virtual void showCaret(int x, int y, int nWidth, int nHeight);
    virtual void hideCaret();
    virtual int getCaretLineHomePos() { return 0; }
    void showCaret();
    void makeCaretInSight();
    void setCaretToPos(int x, int y);

    int getEndColPosOfLine(int nLine);

    void nextPos(int &row, int &col);
    void prevPos(int &row, int &col);
    void nextWord(int &row, int &col);
    void prevWord(int &row, int &col);
    void beginOfLine(int row, int &col);

    void selectNone();
    void removeSelected(int &pnBegSelRow, int &pnBegSelCol);
    void removeStr(int nRow, int nCol, int nSize);
    void removeChar(int nRow, int nCol);

    void insertStr(cstr_t text, bool isMarkedText = false);

    void insertChar(int nRow, int nCol, WCHAR chInsert);
    void insertStr(int nRow, int nCol, cstr_t szText);

    int unindentLine(int lineNo);
    void indentLine(int lineNo);

    void updateScrollInfo(bool bHorz = true, bool bVert = true);

    void getStrAtPos(int nRow, int nCol, string &str);

    bool isPosValid(int nRow, int nCol);

    void sortSelectPos(int &nBegSelRow, int &nBegSelCol, int &nEndSelRow, int &nEndSelCol, bool &bSelectedEmpty);

    bool isReadOnly() { return isFlagSet(m_nEditorStyles, S_READ_ONLY); }
    bool isSingleLine() { return !isFlagSet(m_nEditorStyles, S_MULTILINE); }
    bool isPassword() { return isFlagSet(m_nEditorStyles, S_PASSWORD); }

public:
    //
    // CUIObject messages to draw or handle user input, etc.
    //

    virtual void onCreate() override;

    void draw(CRawGraph *canvas) override;

    // rc is the destination rectangle to draw the edit
    void onSize() override;

    virtual void onSetFocus() override;
    virtual void onKillFocus() override;

    virtual bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    virtual bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    virtual bool onLButtonDblClk(uint32_t nFlags, CPoint point) override;
    virtual bool onMouseDrag(CPoint point) override;
    virtual bool onMouseMove(CPoint point) override;
    virtual bool onRButtonUp(uint32_t nFlags, CPoint point) override;
    virtual void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) override;
    virtual bool onKeyDown(uint32_t nChar, uint32_t nFlags) override;
    virtual void onChar(uint32_t nChar) override;

    virtual void onContexMenu(int xPos, int yPos);
    virtual bool onCommand(uint32_t nId) override;

    virtual void onTimer(int nId) override;

    virtual void onAdjustHue(float hue, float saturation, float luminance) override;
    void onLanguageChanged() override;

    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override;
    virtual void onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override;

    // 最终输入的文字
    virtual void onInputText(cstr_t text) override;

    // MarketText 是临时的文字，当输入其他字符时会被替代
    virtual void onInputMarkedText(cstr_t text) override;

    void onNotifyParseLine(int nLine) {
        if (m_pEditSyntaxParser) {
            m_pEditSyntaxParser->onNotifyParseLine(nLine);
        }
    }

    void onNotifySelChanged() {
        bool bSelected = m_nBegSelRow != -1;
        if (m_bPrevSelectedStatus != bSelected) {
            m_bPrevSelectedStatus = bSelected;
            if (m_pEditNotification) {
                m_pEditNotification->onEditorTextChanged(IEditNotification::S_SEL, bSelected);
            }
        }
    }

    bool setProperty(cstr_t szProperty, cstr_t szValue) override;

    virtual string &getText() override;

#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    //
    // IUndoMgrNotify interface
    //
    virtual void onStatusChanged(Status status, bool bVal) override;
    virtual void onAction(Action action) override;

    friend _SkinEditCtrl::StashSelectionCaret;
    friend _SkinEditCtrl::EditorInsertAction;
    friend _SkinEditCtrl::AutoBatchUndo;
    friend _SkinEditCtrl::AutoInvalidate;
    friend _SkinEditCtrl::EditorBatchAction;

protected:
    uint32_t                    m_nEditorStyles;

    string                      m_placeHolder, m_placeHolderEnglish;
    CColor                      m_clrPlaceHolder;

    CVLines                     m_vLines;

    // scroll bar info
    int                         m_nTopVisibleLine;
    int                         m_nScrollPosx;

    int                         m_nLineSpace;

    // Selected text
    int                         m_nBegSelRow, m_nEndSelRow;
    int                         m_nBegSelCol, m_nEndSelCol;
    bool                        m_bInMouseSel;      // is mouse select text?

    // 输入法输入的临时文字区间
    bool                        m_isMarkedText;
    int                         m_begMarkedRow, m_endMarkedRow;
    int                         m_begMarkedCol, m_endMarkedCol;

    int                         m_nOneCharDx;
    int                         m_indentSize = 4;

    // undo
    CUndoMgr                    m_undoMgr;

    IEditSyntaxParser           *m_pEditSyntaxParser;
    IEditNotification           *m_pEditNotification;
    bool                        m_bPrevSelectedStatus;

    // Caret
    int                         m_nIDTimerCaret;
    CSkinCaret                  m_caret;
    int                         m_nCaretX, m_nCaretY;
    int                         m_nCaretRow, m_nCaretCol;
    int                         m_nCaretMaxXLatest;

    // Cursor
    Cursor                      m_cursor;

    // Font and colors
    CSkinFontProperty           m_font;
    int                         m_xMargin, m_yMargin;
    vector<CColor>              m_vClrTable;

    // Enter will post (ID_OK) message
    int                         m_cmdSubmit = ID_INVALID;

};
