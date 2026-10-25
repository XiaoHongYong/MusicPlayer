#pragma once


bool isNewVersion(cstr_t version);

class CVersionInfo {
public:
    string                      verNew;
    string                      releaseDate;      // YYYY-MM-DD
    string                      url;              // 打开的下载/介绍页
    string                      notes;            // 当前界面语言的更新说明

};

class CVersionUpdate {
public:
    CVersionUpdate();
    virtual ~CVersionUpdate();

    void checkNewVersion(bool bAutoCheck = true);

    virtual void onDownloadOK(CDownloadTask *pTask);

    int getUpdateInfo(CDownloadTask *pTask, CVersionInfo &versionInfo);

};
