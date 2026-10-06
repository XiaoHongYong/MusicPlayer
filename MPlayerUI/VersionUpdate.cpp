#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "VersionUpdate.h"
#include "../version.h"
#include "../../Utils/rapidjson.h"


#define SOFTWARE_INFO       "MusicPlayer"
#define VER_FILE            "dhpver.xml"


uint32_t parseVersionStr(cstr_t version) {
    int verMajor = 0, verMinor = 0;
    int ret = sscanf(version, "%d.%d", &verMajor, &verMinor);
    if (ret == 2) {
        return verMajor * 1000 + verMinor;
    }

    return 0;
}

bool isNewVersion(cstr_t version) {
    return MAJOR_VERSION * 1000 + MINOR_VERSION < parseVersionStr(version);
}

CVersionUpdate::CVersionUpdate() {

}

CVersionUpdate::~CVersionUpdate() {

}

void CVersionUpdate::onDownloadOK(CDownloadTask *pTask) {
    if (pTask->taskType != DTT_CHECK_VERSION) {
        return;
    }

    CVersionInfo versionInfo;
    int nRet = getUpdateInfo(pTask, versionInfo);
    if (nRet != ERR_OK) {
        return;
    }

    if (isNewVersion(versionInfo.verNew.c_str())) {
        string strMessage = _TLT("Do you want to visit our website for more information?");
        if (IDYES == MPlayerApp::getInstance()->messageOut(strMessage.c_str(), MB_YESNO, _TLT("New version notice"))) {
            openUrl(MPlayerApp::getMainWnd(), getStrName(SN_HTTP_DOMAIN));
        }
    }
}

int CVersionUpdate::getUpdateInfo(CDownloadTask *pTask, CVersionInfo &versionInfo) {
    assert(pTask);

    time_t lTime;

    time(&lTime);
    tm *curtime = localtime(&lTime);

    //
    // 将检查更新时间写入配置文件中
    char szCheckVerTime[256];
    snprintf(szCheckVerTime, CountOf(szCheckVerTime), "%d-%d-%d", curtime->tm_year + 1900, curtime->tm_mon + 1, curtime->tm_mday);
    g_profile.encryptWriteString("CheckVerTime", szCheckVerTime);

    rapidjson::Document doc;
    if (doc.Parse(pTask->buffContent.c_str(), pTask->buffContent.size()).HasParseError() || !doc.IsObject()) {
        ERR_LOG1("Failed to parse version json file: %s", pTask->buffContent.c_str());
        return false;
    }

    //  http://crintsoft.com/download/music-player-update.json 的格式
    /*
    {
        "version": "1.0.abcdef",
        "release-date": "2025-01-2"
    }
    */
    versionInfo.verNew = getMemberString(doc, "version");
    versionInfo.releaseDate = getMemberString(doc, "release-date");

    return ERR_OK;
}

void CVersionUpdate::checkNewVersion(bool bAutoCheck) {
    if (bAutoCheck) {
        string strCheckVerTime = g_profile.encryptGetString("CheckVerTime", "");
        DateTime dateLastCheck;
        dateLastCheck.fromString(strCheckVerTime.c_str(), (uint32_t)strCheckVerTime.size());

        // 如果日期相隔7天，则检查更新
        auto dateNow = DateTime::localTime();
        if (abs((long)dateNow.getTime() - (long)dateLastCheck.getTime()) <= 7 * DateTime::SECOND_IN_ONE_DAY) {
            return;
        }

        g_profile.encryptWriteString("CheckVerTime", dateNow.toDateString().c_str());
    }

    g_LyricsDownloader.downloadVersionFile(getStrName(SN_HTTP_VERSION_UPDATE), !bAutoCheck);
}
