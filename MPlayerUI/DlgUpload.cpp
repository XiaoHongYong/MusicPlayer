#include "MPlayerApp.h"
#include "DlgUpload.h"
#include "WaitingDlg.h"
#include "../LyricsLib/CurrentLyrics.h"


#define SZ_EX_POOL_WORK_END "WorkEnd"
#define SZ_EX_POOL_WORK_RESULT  "WorkResult"
#define SZ_EX_POOL_WORK_MSG     "WorkMsg"

// Session of lyrics upload and login
CMLClientSession g_sessionUpload;

class CLoginWorkObj : public CWorkObjBase {
public:
    CLoginWorkObj() {
        m_bEnableCancel = true;
        m_nResult = ERR_OK;
    }

    virtual uint32_t doTheJob() {
        m_strMsg.clear();

        m_nResult = g_sessionUpload.connect();
        if (m_nResult == ERR_OK) {
            m_nResult = g_sessionUpload.login(m_strMsg);
        }
        g_sessionUpload.close();

        return CWorkObjBase::doTheJob();
    }

    virtual void cancelJob() {
        g_sessionUpload.close();

        CWorkObjBase::cancelJob();
    }

public:
    string                      m_strMsg;
    int                         m_nResult;

};


//////////////////////////////////////////////////////////////////////////

class CUploadWorkObj : public CLoginWorkObj {
public:
    CUploadWorkObj() {
        m_bEnableCancel = true;
    }

    virtual uint32_t doTheJob() {
        m_strMsg.clear();

        m_nResult = g_sessionUpload.connect();
        if (m_nResult == ERR_OK) {
            if (!g_sessionUpload.isLogined()) {
                // log in
                m_nResult = g_sessionUpload.login(m_strMsg);
            }

            // upload
            if (m_nResult == ERR_OK) {
                g_sessionUpload.close();
                g_sessionUpload.connect();
                m_nResult = g_sessionUpload.upload(m_strLyricsContent, m_strLyrId, m_strMsg);
            }
            g_sessionUpload.close();

            // save Lyrics Id to lyrics content
            if (m_nResult == ERR_OK) {
                assert(m_strLyrId.size());
                assert(m_strMediaSource.size());
                assert(m_strLyrSource.size());
                if (m_strLyrId.size()) {
                    // save lyrics id
                    if (strcmp(g_currentLyrics.getMediaSource(), m_strMediaSource.c_str()) == 0
                        && strcmp(g_currentLyrics.getLyricsFileName(), m_strLyrSource.c_str()) == 0) {
                        g_currentLyrics.properties().id = m_strLyrId;
                        g_currentLyrics.save();
                    } else {
                        CurrentLyrics lyrData;
                        lyrData.openLyrics(m_strMediaSource.c_str(), 0,
                            m_strLyrId.c_str());
                        lyrData.properties().id = m_strLyrId;
                        lyrData.save();
                    }
                }
            }
        }

        return CWorkObjBase::doTheJob();
    }

public:
    string                      m_strLyricsContent;
    string                      m_strLyrSource;
    string                      m_strMediaSource, m_strLyrId;

};

//////////////////////////////////////////////////////////////////////////

class CSkinWndUploadLyr : public CMPSkinWnd {
public:
    CSkinWndUploadLyr() {
        m_pUploadWorkObj = new CUploadWorkObj();
    }

    ~CSkinWndUploadLyr() {
        if (m_pUploadWorkObj) {
            delete m_pUploadWorkObj;
        }
    }

    void onSkinLoaded() {
        CMPSkinWnd::onSkinLoaded();

        cstr_t szSwitchToPage = g_sessionUpload.isLogined() ? "Container.UploadLyrNotice" : "Container.login";
        CUIObject *pToPage = m_rootConainter.getUIObjectByClassName(szSwitchToPage);
        if (pToPage) {
            pToPage->getParent()->switchToPage(szSwitchToPage, false, 0, false);
        }
    }

    CUploadWorkObj *getUploadWorkObj() const { return m_pUploadWorkObj; }

protected:
    CUploadWorkObj              *m_pUploadWorkObj;

};

//////////////////////////////////////////////////////////////////////////

class CPageUploadWaitMessage : public CPageWaitMessage {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    CPageUploadWaitMessage() {
        m_bLoginWork = false;
    }

    void setLoginWork(bool bLoginWork) { m_bLoginWork = bLoginWork; }

    virtual void onWorkEnd() override;

protected:
    bool                        m_bLoginWork;

};

UIOBJECT_CLASS_NAME_IMP(CPageUploadWaitMessage, "Container.UploadLyrWaitMsg")

//////////////////////////////////////////////////////////////////////////

class CPageLogin : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    CPageLogin()
        { CID_E_PASSWORD = CID_REMEMBER_PWD = CID_E_NAME = 0; m_strText = _TLM("Log in"); }

    void onInitialUpdate() override {
        CSkinContainer::onInitialUpdate();

        GET_ID_BY_NAME3(CID_E_NAME, CID_REMEMBER_PWD, CID_E_PASSWORD);

        setUIObjectProperty("CID_SIGNUP", "Link", getStrName(SN_HTTP_SIGNUP));

        setUIObjectText(CID_E_NAME, g_sessionUpload.getLoginName());
        bool bRememberPwd = g_profile.getBool("rememberPwd", false);
        if (bRememberPwd) {
            setUIObjectText(CID_E_PASSWORD, g_sessionUpload.getLoginPwd());
        }
        checkButton(CID_REMEMBER_PWD, bRememberPwd);
    }

    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();

        if (getExPoolBool(SZ_EX_POOL_WORK_END, false)) {
            setExPool(SZ_EX_POOL_WORK_END, false);

            if (m_loginWorkObj.m_strMsg.size()) {
                m_pSkin->messageOut(m_loginWorkObj.m_strMsg.c_str());
            }

            if (m_loginWorkObj.m_nResult != ERR_OK && !m_loginWorkObj.isJobCanceled()) {
                m_pSkin->messageOut(ERROR2STR_LOCAL(m_loginWorkObj.m_nResult));
                return;
            }
        }
    }

    bool onOK() override {
        string name = getUIObjectText(CID_E_NAME);
        string strPwd = getUIObjectText(CID_E_PASSWORD);
        bool bRememberePwd = isButtonChecked(CID_REMEMBER_PWD);

        if (name.empty() || strPwd.empty()) {
            return true;
        }

        g_profile.writeInt("rememberPwd", bRememberePwd);
        g_sessionUpload.setUploader(name.c_str(), strPwd.c_str(), bRememberePwd);

        CPageUploadWaitMessage *pPageWait = (CPageUploadWaitMessage *)m_pContainer->getChildByClass(CPageUploadWaitMessage::className());
        assert(pPageWait);
        if (pPageWait) {
            pPageWait->setLoginWork(true);
            pPageWait->startWork(_TLT("Log in"),
                _TLT("Signing in. It may take a few seconds."),
                &m_loginWorkObj);
        }

        return false;
    }

    bool onCancel() override {
        m_pContainer->switchToLastPage(0, true);
        return true;
    }

protected:
    int                         CID_E_PASSWORD, CID_REMEMBER_PWD, CID_E_NAME;
    CLoginWorkObj               m_loginWorkObj;

};

UIOBJECT_CLASS_NAME_IMP(CPageLogin, "Container.login")

//////////////////////////////////////////////////////////////////////////

class CPageUploadNotice : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    CPageUploadNotice() {
        m_pUploadObj = nullptr;
        m_strText = _TLM("Upload lyrics");
    }

    void onCreate() override {
        CSkinContainer::onCreate();

        m_pUploadObj = ((CSkinWndUploadLyr *)m_pSkin)->getUploadWorkObj();
        assert(m_pUploadObj);

        GET_ID_BY_NAME(CID_CHANGE_ACCOUNT);
    }

    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();

        assert(g_sessionUpload.isLogined());
        setUIObjectText("CID_ACCOUNT_NOTICE",
            stringPrintf(_TLT("Your lyrics will be uploaded under the account: %s"), g_sessionUpload.getLoginName()).c_str(), false);
    }

    bool onOK() override {
        CPageUploadWaitMessage *pPageWait = (CPageUploadWaitMessage *)m_pContainer->getChildByClass(CPageUploadWaitMessage::className());
        assert(pPageWait);
        if (pPageWait) {
            pPageWait->setLoginWork(false);
            pPageWait->startWork(_TLT("Upload lyrics"),
                _TLT("$Product$ is uploading lyrics. It may take a few seconds."),
                m_pUploadObj);
        }

        return false;
    }

    bool onCommand(uint32_t id) override {
        if (id == CID_CHANGE_ACCOUNT) {
            m_pContainer->switchToPage(CPageLogin::className(), false, 0, true);
        } else {
            return CSkinContainer::onCommand(id);
        }

        return true;
    }

protected:
    int                         CID_CHANGE_ACCOUNT;
    CLoginWorkObj               *m_pUploadObj;

};

UIOBJECT_CLASS_NAME_IMP(CPageUploadNotice, "Container.UploadLyrNotice")

//////////////////////////////////////////////////////////////////////////

class CPageUploadResult : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    void onCreate() override {
        CSkinContainer::onCreate();

        GET_ID_BY_NAME(CID_TRY_AGAIN);
    }

    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();

        if (getExPoolBool(SZ_EX_POOL_WORK_END, false)) {
            setExPool(SZ_EX_POOL_WORK_END, false);

            int nResult = getExPoolInt(SZ_EX_POOL_WORK_RESULT, ERR_FALSE);
            bool bUploadOK = (nResult == ERR_OK);
            setUIObjectVisible("CID_C_UPLOAD_OK", bUploadOK, false);
            setUIObjectVisible("CID_C_UPLOAD_FAILED", !bUploadOK, false);
            if (!bUploadOK) {
                setUIObjectText("CID_RESULT", stringPrintf("%s", ERROR2STR_LOCAL(nResult)).c_str(), false);
            }

            if (getExPoolStr(SZ_EX_POOL_WORK_MSG).size()) {
                m_pSkin->postCustomCommandMsg(ID_SHOW_ERR_RESULT);
            }
        }
    }

    bool onCommand(uint32_t nId) override {
        if (nId == ID_SHOW_ERR_RESULT) {
            assert(getExPoolStr(SZ_EX_POOL_WORK_MSG).size());
            m_pSkin->messageOut(getExPoolStr(SZ_EX_POOL_WORK_MSG).c_str());
        } else if (nId == CID_TRY_AGAIN) {
            CPageUploadWaitMessage *pPageWait = (CPageUploadWaitMessage *)m_pContainer->getChildByClass(CPageUploadWaitMessage::className());
            assert(pPageWait);
            if (pPageWait) {
                pPageWait->setLoginWork(false);
                pPageWait->startWork(_TLT("Upload lyrics"),
                    _TLT("$Product$ is uploading lyrics. It may take a few seconds."),
                    ((CSkinWndUploadLyr *)m_pSkin)->getUploadWorkObj());
            }
        } else {
            return CSkinContainer::onCommand(nId);
        }

        return true;
    }

protected:
    int                         CID_TRY_AGAIN;

};

UIOBJECT_CLASS_NAME_IMP(CPageUploadResult, "Container.UploadLyrResult")

//////////////////////////////////////////////////////////////////////////

void CPageUploadWaitMessage::onWorkEnd() {
    if (m_pWorkObj->isJobCanceled()) {
        if (m_bLoginWork) {
            m_pContainer->switchToPage(CPageLogin::className(), false, 0, true);
        } else {
            m_pContainer->switchToPage(CPageUploadNotice::className(), false, 0, true);
        }
    } else {
        setExPool(SZ_EX_POOL_WORK_END, true);

        if (m_bLoginWork) {
            CLoginWorkObj *pLoginWork = (CLoginWorkObj *)m_pWorkObj;
            if (pLoginWork->m_nResult == ERR_OK) {
                m_pContainer->switchToPage(CPageUploadNotice::className(), false, 0, true);
            } else {
                m_pContainer->switchToLastPage(0, true);
            }
        } else {
            CUploadWorkObj *pUploadWork = (CUploadWorkObj *)m_pWorkObj;
            setExPool(SZ_EX_POOL_WORK_RESULT, pUploadWork->m_nResult);
            setExPool(SZ_EX_POOL_WORK_MSG, pUploadWork->m_strMsg.c_str());
            m_pContainer->switchToPage(CPageUploadResult::className(), false, 0, true);
        }
    }
}

void showUploadLyrDialog(CSkinWnd *pParent, string &strLyricsContent, cstr_t szMediaSource, cstr_t szLyrSource) {
    assert(strLyricsContent.size() > 0 || !isEmptyString(szLyrSource));

    g_sessionUpload.init(getAppNameLong().c_str());

    SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, _SZ_SKINWND_CLASS_NAME,
        "UploadLyrics.xml", pParent);
    CSkinWndUploadLyr *pWndUpload = new CSkinWndUploadLyr();
    skinWndStartupInfo.pSkinWnd = pWndUpload;
    CUploadWorkObj *pUploadWork = pWndUpload->getUploadWorkObj();
    pUploadWork->m_strLyricsContent = strLyricsContent;
    pUploadWork->m_strMediaSource = szMediaSource;
    pUploadWork->m_strLyrSource = szLyrSource;

    CSkinApp::getInstance()->getSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
}

void registerUploadLyrPage(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPageLogin);
    AddUIObjNewer2(pSkinFactory, CPageUploadNotice);
    AddUIObjNewer2(pSkinFactory, CPageUploadWaitMessage);
    AddUIObjNewer2(pSkinFactory, CPageUploadResult);
}
