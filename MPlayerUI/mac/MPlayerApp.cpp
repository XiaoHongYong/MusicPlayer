﻿#include "MPlayerAppBase.h"
#include "DownloadMgr.h"
#include "VersionUpdate.h"
#include "OnlineSearch.h"
#include "AutoProcessEmbeddedLyrics.h"
#include "MPFloatingLyrWnd.h"
#include "LyricShowObj.h"
#include "HeadPhoneWatch.hpp"


CMPlayerApp::CMPlayerApp() {
    m_bRunning = false;
}

CMPlayerApp::~CMPlayerApp() {
}

bool CMPlayerApp::isAnotherInstanceRunning() {
    return false;
}

bool CMPlayerApp::setRunningFlag() {
    // Is Product already running?
    m_bRunning = true;

    return true;
}

bool CMPlayerApp::init() {
    g_log.setSrcRootDir(__FILE__, 2);

    g_LangTool.setMacro(SZ_MACRO_PRODUCT_NAME, SZ_APP_NAME);
    g_LangTool.setMacro(SZ_MACRO_COMPANY_NAME, SZ_COMPANY_NAME);

    if (!CMPlayerAppBase::init()) {
        return false;
    }

    setupHeadPhonePlugWatch();

    return true;
}

void CMPlayerApp::quit() {
    if (!m_bRunning) {
        return;
    }

    m_bRunning = false;

    CMPlayerAppBase::quit();
}
