#pragma once

#include "../Skin/SkinScrollFrameCtrlBase.h"


class CSkinListView;

class CColHeader {
public:
    CColHeader(cstr_t szTitle, int nWidth, int colType, bool bClickable = false, uint32_t drawTextAlignFlags = DT_LEFT) {
        this->strTitle = szTitle;
        this->nWidth = nWidth;
        this->bClickable = bClickable;
        this->colType = colType;
        this->drawTextAlignFlags = drawTextAlignFlags;
    }
    virtual ~CColHeader() { }

    enum {
        TYPE_TEXT                   = 1,
        TYPE_IMAGE                  = 2,
        TYPE_TEXT_EX                = 3,
        TYPE_NEXT                   = 5,
    };

    string                      strTitle;
    int16_t                     nWidth;
    bool                        bClickable;
    int                         colType;
    uint32_t                    drawTextAlignFlags;

};

class IListViewDataSource {
public:
    IListViewDataSource() { m_pView = nullptr; }
    virtual ~IListViewDataSource() { }

    virtual void setView(CSkinListView *pView) { m_pView = pView; }

    virtual int getRowCount() const = 0;

    virtual bool isRowSelected(int row) const = 0;
    virtual void setRowSelectionState(int nIndex, bool bSelected) = 0;

    virtual int getItemImageIndex(int row) { return -1; }
    virtual bool setItemImageIndex(int nItem, int nImageIndex, bool bRedraw = true) { return false; }

    virtual cstr_t getCellText(int row, int col) = 0;
    virtual CRawImage *getCellImage(int row, int col) = 0;

protected:
    CSkinListView               *m_pView;

};

class CSkinListCtrlEventNotify : public IUIObjNotify {
public:
    CSkinListCtrlEventNotify(CUIObject *pObject, int nClickedRow, int nClickedCol) : IUIObjNotify(pObject)
        { cmd = C_CLICK; this->nClickOnRow = nClickedRow; this->nClickOnCol = nClickedCol; }

    enum Command {
        C_CLICK,                         // Left button click
        C_CLICK_CMD, // Left button click at a cell, and the cell will trigger special command
        C_RBTN_CLICK,                    // Right mouse button click
        C_DBL_CLICK,
        C_SEL_CHANGED,
        C_ENTER,                         // Enter key pressed
        C_KEY_DELETE,                    // delete key pressed
    };

    Command                     cmd;
    int                         nClickOnRow, nClickOnCol;

};

/**
 * CSkinListView 仅仅负责 ListView 的绘制.
 */
class CSkinListView : public CSkinScrollFrameCtrlBase {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinScrollFrameCtrlBase)
public:
    enum ColorName {
        CN_BG,
        CN_BG_STRIPE_A,
        CN_BG_STRIPE_B,
        CN_TEXT,
        CN_SEL_BG,
        CN_SEL_TEXT,
        CN_NOW_PLAYING_BG,
        CN_NOW_PLAYING_TEXT,
        CN_CUSTOMIZED_START,

        CN_MAX                      = 255
    };

    enum HitTestArea {
        HTA_NONE,
        HTA_HEADER,
        HTA_HEADER_SPLIT,
        HTA_ROW,
    };

public:
    CSkinListView();
    virtual ~CSkinListView();

    void setDataSource(IListViewDataSource *pDataSource);

    void notifyDataChanged();

    void loadColumnWidth(cstr_t szProperty);
    void saveColumnWidth(cstr_t szProperty);

    virtual bool setProperty(cstr_t szProperty, cstr_t szValue) override;
#ifdef _SKIN_EDITOR_
    void enumProperties(CUIObjProperties &listProperties);
#endif // _SKIN_EDITOR_

    void setColor(int nColorName, const CColor &clr);
    void setColor(int nColorName, cstr_t clr) { setColor(nColorName, parseColorString(clr)); }
    const CColor &getColor(int nColorName);

    virtual void addColumn(cstr_t szCol, int nWidth, int colType = CColHeader::TYPE_TEXT, bool bClickable = false, int drawTextAligns = DT_LEFT);

    uint32_t getColumnCount() const { return (uint32_t)m_vHeading.size(); }
    int getColumnWidth(int nCol) const;
    bool setColumnWidth(int nCol, int cx);

    void setLineHeight(int nLineHeight);

    uint32_t getSelectedCount() const;
    int getNextSelectedItem(int nPos = -1) const;
    VecInts getSelectedItems();

    void setItemSelectionState(int nIndex, bool bSelected);
    void clearAllSelMark();

    virtual void makeSureRowVisible(int nRow);

    void increaseHeight(int value);

    int getContentHeight() const;

    int getRowCount() const;

protected:
    class CPaintUpdater {
    public:
        CPaintUpdater(CSkinListView *pHost) : m_pHost(pHost) {
            // m_nEndSelRow = pHost->m_nEndSelRow;
            m_nFirstVisibleRow = pHost->m_nFirstVisibleRow;
            m_bUpdate = false;
        }

        ~CPaintUpdater() {
            if (m_pHost->m_pVertScrollBar) {
                if (m_pHost->m_nFirstVisibleRow != m_pHost->m_pVertScrollBar->getScrollPos()) {
                    m_pHost->m_pVertScrollBar->setScrollPos(m_pHost->m_nFirstVisibleRow);
                }
            }

            if (m_bUpdate) {
                goto RET_UPDATE;
            }
            if (m_nFirstVisibleRow != m_pHost->m_nFirstVisibleRow) {
                goto RET_UPDATE;
            }

            return;
        RET_UPDATE:
            m_pHost->invalidate();
        }

        void setUpdateFlag(bool bUpdate) { m_bUpdate = bUpdate; }

    protected:
        CSkinListView               *m_pHost;
        bool                        m_bUpdate;
        int                         m_nFirstVisibleRow;

    };
    friend class CPaintUpdater;

    class CSelChangedCheck {
    public:
        CSelChangedCheck(CSkinListView *pListCtrl) : m_pListCtrl(pListCtrl) {
            int n = -1;
            while ((n = m_pListCtrl->getNextSelectedItem(n)) != -1) {
                m_vSelected.push_back(n);
            }
        }

        bool isSelectChanged() {
            int i = 0, n = -1;
            while ((n = m_pListCtrl->getNextSelectedItem(n)) != -1) {
                if (m_vSelected.size() == i || m_vSelected[i] != n) {
                    return true;
                }
                i++;
            }
            if (i != m_vSelected.size()) {
                return true;
            }
            return false;
        }

    public:
        CSkinListView               *m_pListCtrl;
        vector<int>                 m_vSelected;

    };
    friend class CSelChangedCheck;

protected:
    void getRowColAtPoint(CPoint pt, int &row, int &col, CPoint *ptAtItemCell = nullptr, bool *bItemClickable = nullptr);

    void setClickCursor();

    int getLinesOfPerPage() const;
    int getLastVisibleRow() const;

    void reverseSelectStatOfRow(bool bClearOldSel, int nRow);
    void selectRangeRow(int nBeg, int nEnd);

    void recalculateColWidth(CRawGraph *canvas);

    void setScrollInfo(bool bRedraw = true);

    void setHorzScrollInfo(bool bRedraw = true);

    virtual void sendNotifyEvent(CSkinListCtrlEventNotify::Command cmd, int nClickedRow = -1, int nClickedCol = -1);

    HitTestArea hitTest(CPoint &point, int &nColumnIndex);

    void drawHeader(CRawGraph *canvas);

    void invalidateItem(int nItem);

    virtual bool isClickedOn(int row, int col, CColHeader *pHeader, int x, int y);

    virtual void drawCell(int row, int col, CRect &rcCell, CRawGraph *canvas, const CColor &clrText);
    virtual void drawCellImage(CRawImage &image, CRect &rcCell, CRawGraph *canvas, uint32_t drawFlags);

public:
    void draw(CRawGraph *canvas) override;

    bool onKeyDown(uint32_t nChar, uint32_t nFlags) override;
    virtual bool onHandleKeyDown(uint32_t nChar, uint32_t nFlags);

    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    bool onLButtonDblClk(uint32_t nFlags, CPoint point) override;
    bool onRButtonDown(uint32_t nFlags, CPoint point) override;
    bool onRButtonUp(uint32_t nFlags, CPoint point) override;
    bool onMouseDrag(CPoint point) override;
    bool onMouseMove(CPoint point) override;

    void onCreate() override;
    void onSize() override;

    virtual void onAdjustHue(float hue, float saturation, float luminance) override;

    virtual void onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override;
    virtual void onHScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) override;

protected:
    typedef vector<CColHeader *>    V_COLUMNS;

    friend class LC_ITEMEX;
    friend class CPopupSkinListWnd;
    friend class CListViewDataSource;

    bool                        m_bHoverMouseSel;

    IListViewDataSource         *m_dataSource;
    V_COLUMNS                   m_vHeading;

    bool                        m_bDrawBorder;
    bool                        m_bDrawHeader;
    bool                        m_bAscending;
    bool                        m_bEnableSort;
    int                         m_nHeaderHeight;
    CRawPen                     m_penLine;
    CSFImage                    m_imageSortedHeader;
    CSFImage                    m_imageHeader;
    CSFImage                    m_imageAscending;
    CSFImage                    m_imageDescending;
    int                         m_nSortedCol;

    CSFImage                    m_imageBorder;
    int                         m_nBorderWidth;

    CSFImgList                  m_imageList;
    int                         m_imageListIconCx;
    int                         m_imageListRightSpace;
    string                      m_strImageList;

    vector<CColor>              m_vColors;
    vector<CColor>              m_vColorsOrg;
    CSkinFontProperty           m_font;
    int                         m_nBegSelRow, m_nEndSelRow;
    bool                        m_bClickBtDown;

    int                         m_nowPlayingRow;

    int                         m_nLineHeight, m_nLineHeightOrg;
    int                         m_nXMargin;
    int                         m_nCellPadding;      // 单元格左右内边距（现代 table 风格）
    int                         m_nFirstVisibleRow;

    // Drag to adjust the width of columns
    bool                        m_bAdjustColWidth;
    int                         m_nResizingCol;
    CPoint                      m_ptStartDrag;      // 鼠标上一次的位置
    Cursor                      m_curResizeCol;
    Cursor                      m_curHand;

    // 插入到 ListView 的 header 和 footer 控件
    CUIObject                   *m_header, *m_footer;

    // DoubleClick or Enter will post (ID_OK) message
    int                         m_cmdSubmit = ID_INVALID;

};
