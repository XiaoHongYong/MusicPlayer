#include "MPlayerApp.h"
#include "DlgAlbumArtDownload.h"
#include "WaitingDlg.h"
#include "AlbumArtDownloadMgr.h"


#define UMSG_ALBUM_ART_LOG      2


class CAlbumArtDlWork : public CWorkObjBase, public IAlbumArtDownloadLog {
public:
    CAlbumArtDlWork() {
        m_bEnableCancel = true;
    }

    void onLog(cstr_t line) override {
        if (!line || !line[0]) {
            return;
        }
        {
            MutexAutolock lock(m_mutexLog);
            if (!m_pendingLog.empty()) {
                m_pendingLog += "\n";
            }
            m_pendingLog += line;
        }
        if (m_hWndNotify) {
            m_hWndNotify->postUserMessage(UMSG_ALBUM_ART_LOG, 0);
        }
    }

    string takePendingLog() {
        MutexAutolock lock(m_mutexLog);
        string out;
        out.swap(m_pendingLog);
        return out;
    }

    uint32_t doTheJob() override {
        g_albumArtDownloader.downloadNow(task, this, &m_bJobCanceled);
        return 0;
    }

    AlbumArtDownloadTask        task;

protected:
    std::mutex                  m_mutexLog;
    string                      m_pendingLog;
};


class CPageAlbumArtDownload : public CSkinContainer {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    CPageAlbumArtDownload() {
        m_working = false;
        m_closeWhenDone = false;
    }

    ~CPageAlbumArtDownload() {
        m_work.cancelJob();
    }

    void onCreate() override {
        CSkinContainer::onCreate();

        AlbumArtDownloadTask task;
        if (g_player.isMediaOpened()) {
            task.mediaFile = g_player.getSrcMedia();
            task.candidates = extractMediaIdentityCandidates(
                g_player.getArtist(), g_player.getAlbum(), g_player.getTitle(), task.mediaFile.c_str());
        }

        appendLog("Download album art...");
        m_work.task = task;
        m_working = true;
        setUIObjectText(ID_CANCEL, _TLT("Cancel"), false);
        m_work.createWorkThread(m_pSkin);
    }

    void onDestroy() override {
        m_work.cancelJob();
        CSkinContainer::onDestroy();
    }

    bool onCancel() override {
        if (m_working) {
            appendLog("Cancelling...");
            m_work.cancelJob();
            m_closeWhenDone = true;
            return false;
        }
        return true;
    }

    bool onClose() override {
        return onCancel();
    }

    bool onUserMessage(int nMessageID, LPARAM param) override {
        if (nMessageID == UMSG_ALBUM_ART_LOG) {
            string chunk = m_work.takePendingLog();
            if (!chunk.empty()) {
                appendLog(chunk.c_str());
            }
            return true;
        }
        if (nMessageID == UMSG_WORK_END) {
            m_working = false;
            setUIObjectText(ID_CANCEL, _TLT("Close"), true);
            if (m_closeWhenDone && m_pSkin) {
                m_pSkin->postCustomCommandMsg(ID_CLOSE);
            }
            return true;
        }
        return CSkinContainer::onUserMessage(nMessageID, param);
    }

protected:
    void appendLog(cstr_t line) {
        if (!line || !line[0]) {
            return;
        }
        if (!m_log.empty()) {
            m_log += "\n";
        }
        m_log += line;
        setUIObjectText("CID_E_LOG", m_log.c_str(), true);
    }

    CAlbumArtDlWork             m_work;
    string                      m_log;
    bool                        m_working;
    bool                        m_closeWhenDone;
};

UIOBJECT_CLASS_NAME_IMP(CPageAlbumArtDownload, "Container.AlbumArtDownload")

void showDownloadAlbumArtDialog(CSkinWnd *pParent) {
    CSkinApp::getInstance()->showDialog(pParent, "DlgAlbumArtDownload.xml");
}

void registerDownloadAlbumArtPage(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPageAlbumArtDownload);
}
