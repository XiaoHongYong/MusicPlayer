#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "VersionUpdate.h"
#include "WaitingDlg.h"
#include "DlgAbout.h"
#include "../version.h"


class CPageAbout : public CSkinContainer, public IEventHandler {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    CPageAbout() {
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_DOWNLOAD_END);
        m_strText = _TLM("about $Product$");
    }

    void onCreate() override {
        CSkinContainer::onCreate();

        setUIObjectProperty("CID_VERSION", SZ_PN_TEXT, VERSION_STR);

        setUIObjectProperty("CID_COPYRIGHT", SZ_PN_LINK, getStrName(SN_HTTP_DOMAIN));
        setUIObjectText("CID_COPYRIGHT", stringPrintf("%s  2001-2025 Crintsoft MusicPlayer", _TLT("Copyright")).c_str());

        setUIObjectProperty("CID_FEEDBACK", SZ_PN_LINK, getStrName(SN_HTTP_FEEDBACK));

        CVersionUpdate verUpdate;
        verUpdate.checkNewVersion(false);
    }

    void onSwitchTo() override {
        CSkinContainer::onSwitchTo();
    }

    void onEvent(const IEvent *pEvent) override {
        if (pEvent->eventType == ET_DOWNLOAD_END) {
            CEventDownloadEnd *pDlEvt = (CEventDownloadEnd *)pEvent;
            if (pDlEvt->pTask->taskType == DTT_CHECK_VERSION_NOUI) {
                int nRet = ERR_OK;

                if (pDlEvt->pTask->m_errResult == ERR_OK) {
                    CVersionUpdate verCheck;
                    CVersionInfo versionInfo;

                    // get new version info
                    nRet = verCheck.getUpdateInfo(pDlEvt->pTask, versionInfo);
                    if (nRet == ERR_OK) {
                        if (isNewVersion(versionInfo.verNew.c_str())) {
                            // new version
                            string strNewVersion = _TLT("A new version of $Product$ is available.");
                            strNewVersion += " ";
                            strNewVersion += versionInfo.verNew;

                            setUIObjectProperty("CID_NEW_VERSION", SZ_PN_TEXT, strNewVersion.c_str());
                            setUIObjectProperty("CID_NEW_VERSION", SZ_PN_LINK, getStrName(SN_HTTP_DOMAIN));
                            setUIObjectVisible("CID_CHECK_VERSION", false, true);
                            setUIObjectVisible("CID_NEW_VERSION", true, true);
                        } else {
                            setUIObjectProperty("CID_CHECK_VERSION", SZ_PN_TEXT, _TLT("Your $Product$ is up to date."));
                            invalidateUIObject("CID_CHECK_VERSION");
                        }
                        return;
                    }
                } else {
                    nRet = pDlEvt->pTask->m_errResult;
                }

                setUIObjectProperty("CID_CHECK_VERSION", SZ_PN_TEXT, ERROR2STR_LOCAL(nRet));
                invalidateUIObject("CID_CHECK_VERSION");
            }
        }
    }

};

UIOBJECT_CLASS_NAME_IMP(CPageAbout, "Container.about");

//////////////////////////////////////////////////////////////////////////

void showAboutDialog(Window *pWndParent) {
    CSkinApp::getInstance()->showDialog(pWndParent, "AboutBox.xml");
}

//////////////////////////////////////////////////////////////////////////

void registerAboutPage(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPageAbout);
}
