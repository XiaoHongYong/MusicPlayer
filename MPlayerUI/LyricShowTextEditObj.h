// LyricShowTextEditObj.h: interface for the CLyricShowTextEditObj class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

class CLyricShowTextEditObj;

class CLyrEditSyntaxParser : public IEditSyntaxParser
{
public:
    enum LYR_CLR
    {
        C_LOWLIGHT        = CSkinEditCtrl::CN_CUSTOMIZED_START,
        C_TAG,
        C_HILIGHT,
    };

    CLyrEditSyntaxParser();
    virtual ~CLyrEditSyntaxParser();

    virtual void init(CSkinEditCtrl *pEdit);
    virtual void onNotifyParseLine(int nLine);

public:
    CLyricShowTextEditObj    *m_pLyrEdit;

};

class CLyricShowTextEditObj : public CSkinEditCtrl, public IEditNotification, public IEventHandler
{
    UIOBJECT_CLASS_NAME_DECLARE(CSkinEditCtrl)
public:
    CLyricShowTextEditObj();
    virtual ~CLyricShowTextEditObj();

    class CLyrOneLine : public CSkinEditCtrl::COneLine
    {
    public:
        CLyrOneLine()
        {
            nBegTime = 0, nEndTime = 0;
            bTimeLine = false;
            bTempBegTime = true;
            bTempEndTime = true;
        }
        int            nBegTime, nEndTime;
        bool        bTempBegTime, bTempEndTime;
        bool        bTimeLine;
    };

public:
    enum FindTextFlag
    {
        FTF_DOWN        = 0x1,            // search continue from selection
        FTF_MATCH_CASE    = 0x1 << 1,        // case sensitive
        FTF_WHOLEWORD    = 0x1 << 2,        // 
    };

    void onKeyDown(uint32_t nChar, uint32_t nFlags);

    void autoVScrollDown(int nDownLine);

    bool findText(cstr_t szText, uint32_t dwFlags);

    CLyrOneLine *getLine(int nLine);

    // for lyrics draw
    void fastDraw(CRawGraph *canvas, CRect *prcUpdate = nullptr);
    void reDraw(CRawGraph *canvas);
    void onPlayTimeChangedUpdate();
    void drawLineGradual(CRawGraph *canvas, COneLine *pLine, int x, int xMax, int y);

    void setColorTheme(CColor &clrHilight, CColor &clrLowlight, CColor &clrTag, CColor &clrEditLineBg);

    bool isTextChanged();

public:
    virtual void onCmdDelete();

    virtual COneLine *newOneLine() { return new CLyrOneLine; }

public:
    //
    // Find and replace dialog
    //
    void showFindDialog(bool bFind = true);
    bool findNext(bool bSearchDown = true);

protected:
    virtual int getCaretLineHomePos();

public:
    virtual void onCreate();
    virtual void onDestroy();

    virtual void onEvent(const IEvent *pEvent);

    virtual bool onCustomCommand(int nId);

    virtual void onSaveLyrics();

    virtual void draw(CRawGraph *canvas);

public:
    void updateLyricsProperties(bool bRedraw = false);
    void onLyricsChanged();

public:
    virtual void onStatusChanged(IEditNotification::Status status, bool bVal);
    virtual void onTextChanged();

    virtual void onTimer(int nId);

protected:
    void updateTextFontColor();

    void syncTimeTag(bool bMoveToNextLine);

    void adjustSyncTimeOfSelected(bool bIncreaseTime);

    bool                m_bDiscardLyrChangeEvent;
    int                    m_nTimerIDUpdateLyrics;

    enum FindReplaceStatus
    {
        FRS_NONE,
        FRS_FIND,
        FRS_REPLACE,
    };

protected:
    CLyrEditSyntaxParser    m_lyrEditSyntaxParser;

    FindReplaceStatus        m_findReplaceStatus;
    int                        CID_MATCHCASE, CID_FIND_PREV, CID_FIND_NEXT, CID_E_FIND;
    int                        CID_REPLACE, CID_REPLACE_ALL, CID_E_REPLACE;
    int                        CID_E_ARTIST, CID_E_TITLE, CID_E_ALBUM, CID_E_BY;
    int                        CID_E_OFFSET, CID_E_MEDIA_LENGTH;
    int                        CID_RERESH_ARTIST, CID_RERESH_ALBUM, CID_RERESH_TITLE, CID_RERESH_BY, CID_RERESH_MEDIA_LENGTH;
    int                        CID_TB_SEEK_BAR;

    // 
    int                    m_nTopVisibleLineOld;
    int                    m_nCurPosLineOld;
    int                    m_nDrawYOfCurPosLine;
    int                    m_nXOfCurPosLineOld;

};
