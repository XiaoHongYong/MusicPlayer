#include "MPlayerApp.h"
#include "DownloadMgr.h"
#include "VersionUpdate.h"
#include "../version.h"
#include "../../Utils/rapidjson.h"


// 站点文件: http://crintsoft.com/download/music-player-update.json
// 由 build.sh 打包时生成（紧凑、未 format 的 JSON），字段:
// {"version":"1.1.abcdef","date":"2026-10-19","url":"http://www.crintsoft.com",
//  "notes":{"en":"...","zh-CN":"..."}}
// 仅提供英文与简体中文说明；兼容旧字段 release-date。

static bool parseVersionParts(cstr_t version, int &verMajor, int &verMinor) {
    verMajor = 0;
    verMinor = 0;
    if (!version || !version[0]) {
        return false;
    }

    int ret = sscanf(version, "%d.%d", &verMajor, &verMinor);
    return ret == 2;
}

bool isNewVersion(cstr_t version) {
    int remoteMajor = 0, remoteMinor = 0;
    if (!parseVersionParts(version, remoteMajor, remoteMinor)) {
        return false;
    }

    if (remoteMajor != MAJOR_VERSION) {
        return remoteMajor > MAJOR_VERSION;
    }
    if (remoteMinor != MINOR_VERSION) {
        return remoteMinor > MINOR_VERSION;
    }

    // 主次版本相同：完整版本串不同则视为有更新（BUILD 为 commit hash，不可序比较）
    return strcmp(version, VERSION_STR) != 0;
}

static bool isChineseUi() {
    string langFile;
    if (!CLanguageTool::getCurrentLanguageFile(langFile)) {
        return false; // 内置英文
    }

    // 语言包文件名：zh-CN.ini 等
    string title = fileGetTitle(langFile.c_str());
    return strncasecmp(title.c_str(), "zh", 2) == 0;
}

static string pickNotes(const rapidjson::Value &notesObj) {
    if (!notesObj.IsObject()) {
        return "";
    }

    // 仅中文界面用 zh-CN；其它语言一律英文（中文缺失时也回退英文）
    if (isChineseUi()) {
        string notes = getMemberString(notesObj, "zh-CN");
        if (!notes.empty()) {
            return notes;
        }
    }
    return getMemberString(notesObj, "en");
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

    if (!isNewVersion(versionInfo.verNew.c_str())) {
        return;
    }

    string strMessage = _TLT("A new version of $Product$ is available.");
    strMessage += " ";
    strMessage += versionInfo.verNew;
    if (!versionInfo.releaseDate.empty()) {
        strMessage += " (";
        strMessage += versionInfo.releaseDate;
        strMessage += ")";
    }
    if (!versionInfo.notes.empty()) {
        strMessage += "\n\n";
        strMessage += versionInfo.notes;
    }
    strMessage += "\n\n";
    strMessage += _TLT("Do you want to visit our website for more information?");

    if (IDYES == MPlayerApp::getInstance()->messageOut(strMessage.c_str(), MB_YESNO, _TLT("New version notice"))) {
        cstr_t url = versionInfo.url.empty() ? getStrName(SN_HTTP_DOMAIN) : versionInfo.url.c_str();
        openUrl(MPlayerApp::getMainWnd(), url);
    }
}

int CVersionUpdate::getUpdateInfo(CDownloadTask *pTask, CVersionInfo &versionInfo) {
    assert(pTask);

    // 成功拉到版本文件后再记录检查日期（避免失败也把 7 天节流写死）
    auto dateNow = DateTime::localTime();
    g_profile.encryptWriteString("CheckVerTime", dateNow.toDateString().c_str());

    rapidjson::Document doc;
    if (doc.Parse(pTask->buffContent.c_str(), pTask->buffContent.size()).HasParseError() || !doc.IsObject()) {
        ERR_LOG1("Failed to parse version json file: %s", pTask->buffContent.c_str());
        return ERR_BAD_FILE_FORMAT;
    }

    versionInfo.verNew = getMemberString(doc, "version");
    if (versionInfo.verNew.empty()) {
        ERR_LOG0("version field missing in update json");
        return ERR_BAD_FILE_FORMAT;
    }

    versionInfo.releaseDate = getMemberString(doc, "date");
    if (versionInfo.releaseDate.empty()) {
        // 兼容旧格式
        versionInfo.releaseDate = getMemberString(doc, "release-date");
    }

    versionInfo.url = getMemberString(doc, "url");
    if (versionInfo.url.empty()) {
        versionInfo.url = getStrName(SN_HTTP_DOMAIN);
    }

    versionInfo.notes = pickNotes(getMember(doc, "notes"));

    return ERR_OK;
}

void CVersionUpdate::checkNewVersion(bool bAutoCheck) {
    if (bAutoCheck) {
        string strCheckVerTime = g_profile.encryptGetString("CheckVerTime", "");
        DateTime dateLastCheck;
        dateLastCheck.fromString(strCheckVerTime.c_str(), (uint32_t)strCheckVerTime.size());

        // 如果日期相隔不足 7 天，则跳过自动检查
        auto dateNow = DateTime::localTime();
        if (abs((long)dateNow.getTime() - (long)dateLastCheck.getTime()) <= 7 * DateTime::SECOND_IN_ONE_DAY) {
            return;
        }
    }

    g_LyricsDownloader.downloadVersionFile(getStrName(SN_HTTP_VERSION_UPDATE), !bAutoCheck);
}
