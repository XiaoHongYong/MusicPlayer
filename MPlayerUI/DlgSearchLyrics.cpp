#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "DlgSearchLyrics.h"
#include "DlgAbout.h"
#include "OnlineSearch.h"
#include "WaitingDlg.h"
#include "Helper.h"
#include "ErrorResolveDlg.h"
#include "AutoProcessEmbeddedLyrics.h"


#define SZ_EX_POOL_ARTIST       "artist"
#define SZ_EX_POOL_TITLE        "title"
#define SZ_EX_POOL_ALBUM        "album"
#define SZ_EX_POOL_MEDIA_URL    "media_url"
#define SZ_EX_POOL_MEDIA_KEY    "media_key"

bool isHttpUrl(cstr_t szUrl) {
    cstr_t SZ_HTTP_HEADER = "http://";

    return strncasecmp(szUrl, SZ_HTTP_HEADER, strlen(SZ_HTTP_HEADER)) == 0;
}

void updateLyrResultArtistTitle(ListLyrSearchResults &vResultsLocal) {
    ListLyrSearchResults::iterator it;

    for (it = vResultsLocal.begin(); it != vResultsLocal.end(); ++it) {
        LrcSearchResult &result = *it;

        if (result.strArtist.empty() && result.strTitle.empty()) {
            getArtistTitleFromFileName(result.strArtist, result.strTitle, result.strSaveFileName.c_str());
        }
    }
}

class CPageRename : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();

        m_pSkin->setCaptionText(_TLT("Rename lyrics file"));

        setUIObjectText("CID_E_FILENAME", getExPoolStr("FileTitle").c_str(), false);
        setExPool("RenameDlgResult", IDCANCEL);
    }

    bool onOK() override {
        setExPool("FileTitle", getUIObjectText("CID_E_FILENAME").c_str());
        setExPool("RenameDlgResult", IDOK);
        m_pContainer->switchToLastPage(0, true);
        return false;
    }

    bool onCancel() override {
        m_pContainer->switchToLastPage(0, true);
        return false;
    }

};

UIOBJECT_CLASS_NAME_IMP(CPageRename, "Container.RenameLyr")

//////////////////////////////////////////////////////////////////////

class CSearchWorkObj : public CWorkObjBase {
public:
    CSearchWorkObj() {
        m_nCurPage = 0;
        m_nPageCount = 1;
        m_bEnableCancel = true;
    }

    virtual uint32_t doTheJob() {
        m_pvSearchResult->clear();

        m_strMsg = "";

        assert(m_strMediaKey.size() > 0);
        m_nResult = g_OnlineSearch.searchOnline(m_session, m_strArtist.c_str(), m_strTitle.c_str(), m_strMediaKey.c_str(), *m_pvSearchResult, m_strMsg, m_nCurPage, m_nPageCount);

        return CWorkObjBase::doTheJob();
    }

    virtual void cancelJob() {
        m_session.close();

        CWorkObjBase::cancelJob();
    }

public:
    ListLyrSearchResults           *m_pvSearchResult;
    string                      m_strMsg;
    int                         m_nCurPage, m_nPageCount;
    int                         m_nResult;
    CMLClientSession            m_session;

    string                      m_strArtist, m_strTitle, m_strMediaKey;

};

//////////////////////////////////////////////////////////////////////////

class CPageSearchLyrics : public CSkinContainer, public IUIObjNotifyHandler, public IEditNotification {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    enum { COL_ARTIST, COL_TITLE, COL_RATING, COL_DOWNLOADS, COL_FROM, COL_ALBUM, COL_URL};
    enum {
        IMG_NONE                    = 0,

        // Local
        IMG_L_TXT,
        IMG_L_LRC,
        IMG_L_UNKNOWN,

        // Inet
        IMG_I_TXT,
        IMG_I_LRC,
        IMG_I_UNKNOWN,

        IMG_OFFSET_INET             = IMG_I_UNKNOWN - IMG_L_UNKNOWN,
    };
    int                         TIMER_ID_RESEARCH;
    enum {
        DURATION_AUTO_SEARCH        = 1000,

        R_CODE_RENAME_LYR           = 1,
    };

    CPageSearchLyrics() {
        m_msgNeed |= UO_MSG_WANT_COMMAND;
        TIMER_ID_RESEARCH = 0;
        m_bAutoSearchLocal = true;
        m_nCurPage = 0;
        m_nPageCount = 0;

        m_pLyricsList = nullptr;
        m_pSearchObj = nullptr;

        CID_PREV_PAGE = CID_NEXT_PAGE = CID_GOOGLE = CID_MORE = CID_E_ARTIST = 0;
        CID_E_TITLE = CID_SEARCH = CID_OPEN = CID_L_LYRICS = 0;
        m_strTitle = _TLM("Search lyrics");
    }

    ~CPageSearchLyrics() {
        if (m_pSearchObj) {
            m_pSearchObj->cancelJob();
            delete m_pSearchObj;
        }
    }

    // SkinListControl notification
    void onUIObjNotify(IUIObjNotify *pNotify) override {
        if (pNotify->isKindOf(CSkinListCtrl::className())) {
            CSkinListCtrlEventNotify *pListCtrlNotify = (CSkinListCtrlEventNotify*)pNotify;
            switch (pListCtrlNotify->cmd) {
            case CSkinListCtrlEventNotify::C_DBL_CLICK:
            case CSkinListCtrlEventNotify::C_ENTER:
                if (onOK()) {
                    m_pSkin->postDestroy();
                }
                break;
            default:
                break;
            }
        }
    }

    // Edit control notification
    void onEditorTextChanged() override {
        if (m_bAutoSearchLocal) {
            m_pSkin->unregisterTimerObject(this, TIMER_ID_RESEARCH);
            TIMER_ID_RESEARCH = m_pSkin->registerTimerObject(this, DURATION_AUTO_SEARCH);
        }
    }

    // Edit control notification
    bool onEditorKeyDown(uint32_t code, uint32_t flags) override {
        if (code == VK_RETURN) {
            onSearch();
            return true;
        }
        return false;
    }

    void onCreate() override {
        CSkinContainer::onCreate();

        GET_ID_BY_NAME3(CID_PREV_PAGE, CID_GOOGLE, CID_NEXT_PAGE);
        GET_ID_BY_NAME3(CID_MORE, CID_E_ARTIST, CID_E_TITLE);
        GET_ID_BY_NAME3(CID_SEARCH, CID_OPEN, CID_L_LYRICS);

        // save current media info.
        m_strSongFile = getExPoolStr(SZ_EX_POOL_MEDIA_URL);
        m_strMediaKey = getExPoolStr(SZ_EX_POOL_MEDIA_KEY);
        m_strArtist = getExPoolStr(SZ_EX_POOL_ARTIST);
        m_strTitle = getExPoolStr(SZ_EX_POOL_TITLE);

        m_pLyricsList = (CSkinListCtrl *)m_pSkin->getUIObjectById(CID_L_LYRICS, CSkinListCtrl::className());
        assert(m_pLyricsList);
        if (!m_pLyricsList) {
            return;
        }

        setUIObjectText(CID_E_ARTIST, m_strArtist.c_str(), false);
        setUIObjectText(CID_E_TITLE, m_strTitle.c_str(), false);

        cstr_t szHead[] = {"Artist", "Title", "Rating", "Downloads", "From", "Album", nullptr};
        int arrWidth[] = {130, 160, 90, 80, 150, 150, 0};

        assert(CountOf(arrWidth) == CountOf(szHead));
        for (int i = 0; szHead[i] != nullptr; i++) {
            m_pLyricsList->addColumn(_TL(szHead[i]), arrWidth[i]);
        }
        if (g_profile.getInt("ShowLink", false)) {
            m_pLyricsList->addColumn("URL", 400);
        }
        m_pLyricsList->loadColumnWidth("OpenLyrics_ListColWidth");

        g_OnlineSearch.searchCacheForCur(&m_vResultsInet);
        m_vResultsInet.sort();

        searchLocal();

        updateSearchResultToListCtrl();

        m_pSkin->registerUIObjNotifyHandler(CID_L_LYRICS, this);

        CSkinEditCtrl *pEdit = (CSkinEditCtrl*)m_pSkin->getUIObjectById(CID_E_ARTIST, CSkinEditCtrl::className());
        if (pEdit) {
            pEdit->setEditNotification(this);
            pEdit = (CSkinEditCtrl*)m_pSkin->getUIObjectById(CID_E_TITLE, CSkinEditCtrl::className());
            if (pEdit) {
                pEdit->setEditNotification(this);
            }
        }
    }

    void onDestroy() override {
        m_pSkin->unregisterUIObjNotifyHandler(this);
        m_pLyricsList->saveColumnWidth("OpenLyrics_ListColWidth");
    }

    bool onOK() override {
        CUIObject *pObj = m_pSkin->getFocusUIObj();
        if (pObj) {
            int nFocusId = m_pSkin->getFocusUIObj()->getID();
            if (nFocusId == CID_E_ARTIST || nFocusId == CID_E_TITLE) {
                m_pSkin->postCustomCommandMsg(CID_SEARCH);
            } else if (nFocusId == CID_L_LYRICS) {
                m_pSkin->postCustomCommandMsg(CID_OPEN);
            }
        }

        return false;
    }

    void onOpen() {
        if (m_pLyricsList->getSelectedCount() == 0) {
            return;
        }

        LrcSearchResult *pSel;

        pSel = getCurSel();
        if (!pSel) {
            return;
        }

        if (isHttpUrl(pSel->strUrl.c_str())) {
            // Download selected lyrics
            g_LyricsDownloader.downloadLyrics(m_strMediaKey.c_str(),
                m_strSongFile.c_str(), pSel->strUrl.c_str(), pSel->strSaveFileName.c_str());
        } else {
            // Associate lyrics file
            if (g_LyricSearch.associateLyrics(m_strMediaKey.c_str(), pSel->strUrl.c_str())) {
                MPlayerApp::getInstance()->dispatchResearchLyrics();
            }
        }
        m_pSkin->postDestroy();
    }

    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();

        CSkinWnd::CAutoRedrawLock redrawLock(m_pSkin);

        if (m_pSearchObj) {
            onOnlineSearchEnd();
            delete m_pSearchObj;
            m_pSearchObj = nullptr;
        } else if (getExPoolInt("RenameDlgResult") == IDOK) {
            //
            // Rename selected lyrics file
            //
            LrcSearchResult *pSel;
            setExPool("RenameDlgResult", IDCANCEL);

            pSel = getCurSel();
            if (!pSel || lyrSrcTypeFromName(pSel->strUrl.c_str()) != LST_FILE) {
                return;
            }

            string strFileTitle = getExPoolStr("FileTitle");
            string strFileTitleOld = fileGetTitle(pSel->strUrl.c_str());

            // Same?
            if (strcmp(strFileTitle.c_str(), strFileTitleOld.c_str()) == 0) {
                return;
            }

            string strFileNew = fileGetPath(pSel->strUrl.c_str());
            strFileNew += strFileTitle.c_str();
            strFileNew += fileGetExt(pSel->strUrl.c_str());
            if (!moveFile(pSel->strUrl.c_str(), strFileNew.c_str())) {
                m_pSkin->messageOut(_TLT("Failed to rename lyrics file."));
            } else {
                // Change lyrics association, if necessary.
                char szAssociateLyr[MAX_PATH];
                if (g_LyricSearch.getAssociatedLyrics(m_strMediaKey.c_str(), szAssociateLyr, CountOf(szAssociateLyr))
                    && strcmp(szAssociateLyr, pSel->strUrl.c_str()) == 0) {
                    g_LyricSearch.associateLyrics(m_strMediaKey.c_str(), strFileNew.c_str());
                }

                pSel->strUrl = strFileNew;

                int nSel = m_pLyricsList->getNextSelectedItem(-1);
                m_pLyricsList->setItemText(nSel, COL_FROM, strFileNew.c_str());
            }
        }

        redrawLock.cancelDrawUpdate();
    }

    bool onTimer(uint32_t nIDEvent) {
        if (nIDEvent != TIMER_ID_RESEARCH) {
            return false;
        }

        m_pSkin->unregisterTimerObject(this, TIMER_ID_RESEARCH);

        string strArtist = getUIObjectText(CID_E_ARTIST);
        string strTitle = getUIObjectText(CID_E_TITLE);

        // Not changed?
        if ((strcasecmp(strArtist.c_str(), m_strArtist.c_str()) == 0
            && strcasecmp(strTitle.c_str(), m_strTitle.c_str()) == 0)
            || isEmptyString(strTitle.c_str())) {
            return true;
        }

        m_strArtist = strArtist;
        m_strTitle = strTitle;

        searchLocal();

        updateSearchResultToListCtrl();

        return true;
    }

    bool onCommand(uint32_t nId) override {
        switch (nId) {
        case ID_NO_SUITTABLE_LYRICS:
            {
                if (m_strMediaKey.empty()) {
                    break;
                }

                g_LyricSearch.associateLyrics(m_strMediaKey.c_str(), NONE_LYRCS);
                MPlayerApp::getInstance()->dispatchResearchLyrics();
                m_pSkin->postDestroy();
            }
            break;
        case ID_NO_PROMPT:
            // Don't prompt this dialog automatically.
            g_profile.writeInt(SZ_SECT_LYR_DL, "DownLrcUserSelect",
                !g_profile.getBool(SZ_SECT_LYR_DL, "DownLrcUserSelect", false));
            break;
        case ID_RENAME:
            {
                //
                // Rename selected lyrics file
                //
                LrcSearchResult *pSel;

                pSel = getCurSel();
                if (!pSel || lyrSrcTypeFromName(pSel->strUrl.c_str()) != LST_FILE) {
                    break;
                }

                string strTitle = fileGetTitle(pSel->strUrl.c_str());

                setExPool("FileTitle", strTitle.c_str());
                m_pContainer->switchToPage(CPageRename::className(), true, R_CODE_RENAME_LYR, true);
            }
            break;
        case ID_DEL_FILE:
            {
                LrcSearchResult *pSel;

                pSel = getCurSel();
                if (!pSel) {
                    break;
                }

                LRC_SOURCE_TYPE lst = lyrSrcTypeFromName(pSel->strUrl.c_str());
                bool bDeleted = false;
                if (lst == LST_FILE) {
                    SHDeleteFile(pSel->strUrl.c_str(), m_pSkin);
                    if (!isFileExist(pSel->strUrl.c_str())) {
                        bDeleted = true;
                    }
                } else {
                    VecStrings vLyrNames;
                    vLyrNames.push_back(pSel->strUrl);
                    int nRet = g_autoProcessEmbeddedLyrics.removeEmbeddedLyrics(m_strSongFile.c_str(), vLyrNames, true);
                    if (nRet == ERR_OK) {
                        bDeleted = true;
                    } else {
                        m_pSkin->messageOut(ERROR2STR_LOCAL(nRet));
                    }
                }

                if (bDeleted) {
                    int nSel = m_pLyricsList->getNextSelectedItem(-1);
                    m_vResultsLocal.deleteByOrder(nSel);
                    m_pLyricsList->deleteItem(nSel);
                }
            }
            break;
        case ID_EXTERNAL_LYR_EDIT:
            //
            // Edit lyrics
            //
            {
                LrcSearchResult *pSel;

                pSel = getCurSel();
                if (!pSel) {
                    break;
                }

                if (lyrSrcTypeFromName(pSel->strUrl.c_str()) != LST_FILE) {
                    break;
                }

                string strEditor;
                getNotepadEditor(strEditor);
                execute(m_pSkin,
                    CMLProfile::getDir(SZ_SECT_UI, "LyricsEditor", strEditor.c_str()).c_str(),
                    pSel->strUrl.c_str());
            }
            break;
        case ID_BROWSE:
            {
                //
                // Browse lyrics file and associate.
                //
                LrcSearchResult *pSel;
                string strFile;

                pSel = getCurSel();
                if (pSel) {
                    if (lyrSrcTypeFromName(pSel->strUrl.c_str()) != LST_FILE) {
                        strFile = pSel->strUrl;
                    }
                }

                static const char *SZ_SUPPORTED_FILE_TYPE = "All supported files (*.lrc; *.txt; *.srt)\0*.lrc;*.txt;*.srt\0Lyrics File (*.lrc)\0*.LRC\0Text File (*.txt)\0*.txt\0Vob Subtitle File(*.srt)\0*.srt\0\0";
                CFileOpenDlg dlg(_TLT("Open lyrics file"), strFile.c_str(), SZ_SUPPORTED_FILE_TYPE, 1);

                if (dlg.doModal(m_pSkin) == IDOK) {
                    strFile = dlg.getOpenFile();
                    if (g_LyricSearch.associateLyrics(m_strMediaKey.c_str(), strFile.c_str())) {
                        MPlayerApp::getInstance()->dispatchResearchLyrics();
                    }

                    m_pSkin->postDestroy();
                }
            }
            break;
        default:
            if (nId == CID_SEARCH) {
                onSearch();
            } else if (nId == CID_OPEN) {
                onOpen();
            } else if (nId == CID_GOOGLE) {
                //
                string strLink = "http://www.google.com/cse?cx=partner-pub-6917905549616855%3All7s7l-jge2&ie=ISO-8859-1&q=";
                string strArtist = getUIObjectText(CID_E_ARTIST);
                string strTitle = getUIObjectText(CID_E_TITLE);
                strrep(strArtist, " ", "+");
                strrep(strTitle, " ", "+");
                strLink += strArtist + "+" + strTitle;
                strLink += "&siteurl=viewlyrics.com%2Fsearch.htm";
                openUrl(m_pSkin, strLink.c_str());
            } else if (nId == ID_SHOW_ERR_RESULT) {
                if (m_nErrResultToShow == ERR_NOT_FOUND) {
                    string strMsg;
                    strMsg = _TLT("Sorry, no results were returned. Please make sure the artist and title are correct.");
                    strMsg += "\r\n";
                    strMsg += _TLT("If you have the lyrics, please upload them to our server to share with others.");
                    m_pSkin->messageOut(strMsg.c_str());
                } else if (m_nErrResultToShow != ERR_OK) {
                    showInetErrorDlg(m_pSkin, m_nErrResultToShow);
                }
            } else if (nId == CID_MORE) {
                CRect rc;
                bool bLocalLyr = false;
                bool bLocalLyrFile = false;

                LrcSearchResult *pSel;

                pSel = getCurSel();
                if (pSel) {
                    if (!isHttpUrl(pSel->strUrl.c_str())) {
                        bLocalLyr = true;
                        if (lyrSrcTypeFromName(pSel->strUrl.c_str()) == LST_FILE) {
                            bLocalLyrFile = true;
                        }
                    }
                }

                auto menu = m_pSkin->getSkinFactory()->loadMenu(m_pSkin, "OpenLyricsDlgMenu");
                if (menu) {
                    menu->enableItem(ID_EXTERNAL_LYR_EDIT, bLocalLyrFile);
                    menu->enableItem(ID_DEL_FILE, bLocalLyr);
                    menu->enableItem(ID_RENAME, bLocalLyrFile);
                    menu->checkItem(ID_NO_PROMPT,
                        !g_profile.getBool(SZ_SECT_LYR_DL, "DownLrcUserSelect", false));

                    getUIObjectRect(CID_MORE, rc);
                    m_pSkin->clientToScreen(rc);
                    menu->trackPopupMenu(rc.left, rc.top, m_pSkin);
                }
            } else if (nId == CID_NEXT_PAGE) {
                if (m_nCurPage >= m_nPageCount - 1) {
                    return true;
                }
                m_nCurPage++;

                if (m_nCurPage < (int)m_vCachePages.size()) {
                    m_vResultsInet = m_vCachePages[m_nCurPage];
                } else {
                    searchOnline();
                }

                updateSearchResultToListCtrl();

                m_pLyricsList->setItemSelectionState(0, true);

                enableUIObject(CID_NEXT_PAGE, m_nCurPage < m_nPageCount - 1);
                enableUIObject(CID_PREV_PAGE, m_nCurPage > 0);
            } else if (nId == CID_PREV_PAGE) {
                if (m_nCurPage <= 0) {
                    return true;
                }
                m_nCurPage--;

                assert(m_nCurPage < (int)m_vCachePages.size());
                m_vResultsInet = m_vCachePages[m_nCurPage];

                updateSearchResultToListCtrl();

                m_pLyricsList->setItemSelectionState(0, true);

                enableUIObject(CID_NEXT_PAGE, m_nCurPage < m_nPageCount - 1);
                enableUIObject(CID_PREV_PAGE, m_nCurPage > 0);
            } else {
                return false;
            }

            return true;
        }

        return true;
    }

protected:
    void onOnlineSearchEnd() {
        assert(m_pSearchObj);

        if (m_pSearchObj->m_strMsg.size()) {
            m_pSkin->messageOut(m_pSearchObj->m_strMsg.c_str());
        }

        else if (m_pSearchObj->m_nResult != ERR_OK && !m_pSearchObj->isJobCanceled()) {
            m_nErrResultToShow = m_pSearchObj->m_nResult;
            m_pSkin->postCustomCommandMsg(ID_SHOW_ERR_RESULT);
        }

        m_nCurPage = m_pSearchObj->m_nCurPage;
        m_nPageCount = m_pSearchObj->m_nPageCount;

        m_vResultsInet.sort();

        searchLocal();

        updateSearchResultToListCtrl();

        if (m_pLyricsList->getItemCount() > 0) {
            m_pLyricsList->setItemSelectionState(0, true);
        }

        if (m_nPageCount > 1) {
            m_vCachePages.push_back(m_vResultsInet);
            hidePreNextButtons(false);
        } else {
            hidePreNextButtons(true);
        }
    }

    void onSearch() {
        m_strArtist = getUIObjectText(CID_E_ARTIST);
        m_strTitle = getUIObjectText(CID_E_TITLE);

        m_strArtistLastSearch = m_strArtist;
        m_strTitleLastSearch = m_strTitle;

        m_nCurPage = 0;
        m_nPageCount = 1;

        m_vCachePages.clear();

        searchOnline();
    }

    void searchLocal() {
        m_vResultsLocal.clear();

        auto nTimeStart = getTickCount();
        const int nAcceptableCostTime = 500;

        g_LyricSearch.searchAllMatchLyrics(m_strSongFile.c_str(), m_strArtist.c_str(), m_strTitle.c_str(), m_vResultsLocal);

        //
        // set best match lyrics
        //
        string strLyricsFile;
        if (g_LyricSearch.getBestMatchLyrics(m_strSongFile.c_str(), m_strArtist.c_str(), m_strTitle.c_str(), strLyricsFile)
            && !g_LyricSearch.isAssociatedWithNoneLyrics(m_strMediaKey.c_str())) {
            LrcSearchResult *pResult;
            pResult = m_vResultsLocal.searchByUrl(strLyricsFile.c_str());
            if (pResult) {
                pResult->nMatchValue = MATCH_VALUE_MAX;
            } else {
                LrcSearchResult result;
                result.strUrl = strLyricsFile;
                result.nMatchValue = MATCH_VALUE_MAX;
                m_vResultsLocal.addResult(result);
            }
        }

        m_vResultsLocal.sort();

        m_vResultsLocal.updateLyricsName();

        updateLyrResultArtistTitle(m_vResultsLocal);

        m_bAutoSearchLocal = (getTickCount() - nTimeStart <= nAcceptableCostTime);
    }

    void searchOnline() {
        m_vResultsInet.clear();

        if (m_strArtistLastSearch.empty() && m_strTitleLastSearch.empty()) {
            return;
        }

        m_pLyricsList->deleteAllItems();

        assert(!m_pSearchObj);
        m_pSearchObj = new CSearchWorkObj();

        m_pSearchObj->m_strArtist = m_strArtistLastSearch;
        m_pSearchObj->m_strTitle = m_strTitleLastSearch;
        m_pSearchObj->m_strMediaKey = m_strMediaKey;
        m_pSearchObj->m_pvSearchResult = &m_vResultsInet;
        m_pSearchObj->m_nCurPage = m_nCurPage;

        CPageWaitMessage *pPageWait = (CPageWaitMessage *)m_pContainer->getChildByClass(CPageWaitMessage::className());
        assert(pPageWait);
        if (pPageWait) {
            pPageWait->startWork(_TLT("Searching lyrics"),
                _TLT("$Product$ is searching for lyrics. It may take a few seconds."),
                m_pSearchObj);
        }
    }

    void updateSearchResultToListCtrl() {
        m_pLyricsList->deleteAllItems(false);

        for (ListLyrSearchResults::iterator it = m_vResultsLocal.begin(); it != m_vResultsLocal.end(); it++) {
            appendItemInResultsList(*it, false);
        }

        for (ListLyrSearchResults::iterator it = m_vResultsInet.begin(); it != m_vResultsInet.end(); it++) {
            appendItemInResultsList(*it, true);
        }

        if (m_pLyricsList->getRowCount() > 0
            && !g_LyricSearch.isAssociatedWithNoneLyrics(m_strMediaKey.c_str())) {
            m_pLyricsList->setItemSelectionState(0, true);
        }

        m_pLyricsList->invalidate();
    }

    void appendItemInResultsList(LrcSearchResult &item, bool bInetFile) {
        char szTemp[MAX_PATH];
        int nImg = 0;
        bool bLocalFile = false;

        LRC_SOURCE_TYPE lrcSrcType = lyrSrcTypeFromName(item.strUrl.c_str());

        auto ct  = getLyricsContentTypeByFileExt(item.strSaveFileName.c_str());
        if (ct == LCT_LRC) nImg = IMG_L_LRC;
        else if (ct == LCT_TXT) nImg = IMG_L_TXT;
        else nImg = IMG_L_UNKNOWN;

        if (!bInetFile) {
            // local file...
            if (lrcSrcType == LST_ID3V2_SYLT || lrcSrcType == LST_ID3V2_LYRICS || lrcSrcType == LST_LYRICS3V2 || lrcSrcType == LST_SOLE_EMBEDDED_LYRICS) {
                nImg = IMG_L_LRC;
            } else if (lrcSrcType == LST_ID3V2_USLT) {
                nImg = IMG_L_TXT;
            } else if (lrcSrcType == LST_NONE) {
                nImg = IMG_NONE;
            } else if (lrcSrcType == LST_FILE) {
                bLocalFile = true;
            }
        } else {
            nImg += IMG_OFFSET_INET;
        }

        int index = m_pLyricsList->insertItem(m_pLyricsList->getItemCount(), item.strArtist.c_str(), nImg, 0, false);
        m_pLyricsList->setItemText(index, COL_TITLE, item.strTitle.c_str(), false);

        if (item.fRate != 0.0) {
            snprintf(szTemp, CountOf(szTemp), "%.1f/5 (%d %s)", item.fRate, item.nRateCount, _TLT("Votes"));
        } else {
            strcpy_safe(szTemp, CountOf(szTemp), "-");
        }
        m_pLyricsList->setItemText(index, COL_RATING, szTemp, false);

        if (bInetFile) {
            itoa(item.nDownloads, szTemp);
            m_pLyricsList->setItemText(index, COL_DOWNLOADS, szTemp, false);
        }

        if (bLocalFile) {
            m_pLyricsList->setItemText(index, COL_FROM, item.strUrl.c_str(), false);
        } else {
            m_pLyricsList->setItemText(index, COL_FROM, item.strUploader.c_str(), false);
        }

        m_pLyricsList->setItemText(index, COL_ALBUM, item.strAlbum.c_str(), false);
        if (m_pLyricsList->getColumnCount() > COL_URL) {
            m_pLyricsList->setItemText(index, COL_URL, item.strUrl.c_str(), false);
        }
    }

    LrcSearchResult *getCurSel() {
        int nPos;

        nPos = m_pLyricsList->getNextSelectedItem(-1);
        if (nPos < 0) {
            return nullptr;
        }

        if (nPos < (int)m_vResultsLocal.size()) {
            return m_vResultsLocal.getByOrder(nPos);
        }

        nPos -= m_vResultsLocal.size();
        if (nPos < (int)m_vResultsInet.size()) {
            return m_vResultsInet.getByOrder(nPos);
        } else {
            assert(0 && "Invalid order");
            return nullptr;
        }
    }

    void hidePreNextButtons(bool bHide) {
        CRect rcBt, rcResultList = m_pLyricsList->m_rcObj;

        if (getUIObjectRect(CID_NEXT_PAGE, rcBt)) {
            int nIncValue;
            if (bHide) {
                nIncValue = rcBt.bottom - rcResultList.bottom - 3;
            } else {
                nIncValue = -(rcResultList.bottom - (rcBt.top - 8));
            }

            m_pLyricsList->increaseHeight(nIncValue);
        }

        setUIObjectVisible(CID_PREV_PAGE, !bHide, false);
        setUIObjectVisible(CID_NEXT_PAGE, !bHide, false);

        invalidate();
    }

protected:
    int                         CID_PREV_PAGE, CID_NEXT_PAGE, CID_GOOGLE, CID_MORE;
    int                         CID_E_ARTIST, CID_E_TITLE, CID_OPEN, CID_SEARCH, CID_L_LYRICS;

    ListLyrSearchResults           m_vResultsLocal;
    ListLyrSearchResults           m_vResultsInet;

    string                      m_strArtistLastSearch, m_strTitleLastSearch;
    int                         m_nCurPage, m_nPageCount;

    // cache returned result.
    vector<ListLyrSearchResults>   m_vCachePages;

    string                      m_strSongFile, m_strMediaKey, m_strArtist, m_strTitle;

    //
    // Auto search local lyrics, if artist/tile changed?
    //
    bool                        m_bAutoSearchLocal;

    CSkinListCtrl               *m_pLyricsList;

    CSearchWorkObj              *m_pSearchObj;
    int                         m_nErrResultToShow;

};

UIOBJECT_CLASS_NAME_IMP(CPageSearchLyrics, "Container.SearchLyr")

//////////////////////////////////////////////////////////////////////////

void showSearchLyricsDialog(CSkinWnd *pParent) {
    SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, _SZ_SKINWND_CLASS_NAME,
        "openLyrics.xml", pParent);

    skinWndStartupInfo.mapExchangePool[SZ_EX_POOL_ARTIST] = g_player.getArtist();
    skinWndStartupInfo.mapExchangePool[SZ_EX_POOL_TITLE] = g_player.getTitle();
    skinWndStartupInfo.mapExchangePool[SZ_EX_POOL_MEDIA_URL] = g_player.getSrcMedia();
    skinWndStartupInfo.mapExchangePool[SZ_EX_POOL_ALBUM] = g_player.getAlbum();
    skinWndStartupInfo.mapExchangePool[SZ_EX_POOL_MEDIA_KEY] = g_player.getMediaKey();

    CSkinApp::getInstance()->getSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
}

void registerSearchLyricsPage(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPageSearchLyrics);
    AddUIObjNewer2(pSkinFactory, CPageWaitMessage);
    AddUIObjNewer2(pSkinFactory, CPageRename);
}
