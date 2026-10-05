#include "MPlayerApp.h"
#include "MPSkinFactory.h"
#include "MPSkinWnd.h"
#include "LyricShowMultiRowObj.h"
#include "LyricShowTxtObj.h"
#include "LyricShowTwoRowObj.h"
#include "LyricShowSingleRowObj.h"
#include "LyricShowVobSub.h"
#include "LyricShowAgentObj.h"
#include "LyricShowTextEditObj.h"
#include "LyricShowTxtContainer.h"
#include "MPSkinTimeCtrl.h"
#include "MPSkinInfoTextCtrl.h"
#include "MediaInfoTextCtrl.h"
#include "MediaAlbumArtCtrl.h"
#include "MPSkinInfoTextCtrlEx.h"
#include "DlgAbout.h"
#include "DlgSearchLyrics.h"
#include "DlgAlbumArtDownload.h"
#include "PreferenceDlg.h"
#include "DlgUpload.h"
#include "DlgAdjustHue.h"
#include "MPlaylistCtrl.h"
#include "MPSkinMediaNumInfoCtrl.h"
#include "MPSkinMenu.h"
#include "MPFloatingLyrWnd.h"


CMPSkinFactory::CMPSkinFactory(CSkinApp *pApp, UIObjectIDDefinition uidDefinition[])
: CSkinFactory(pApp, uidDefinition) {
    m_bClickThrough = false;
}

CMPSkinFactory::~CMPSkinFactory() {

}

int CMPSkinFactory::init() {
    AddUIObjNewer(CLyricShowMultiRowObj);
    AddUIObjNewer(CLyricShowTxtObj);
    AddUIObjNewer(CLyricShowTwoRowObj);
    AddUIObjNewer(CLyricShowSingleRowObj);
    AddUIObjNewer(CLyricShowVobSub);
    AddUIObjNewer(CLyricShowTextEditObj);
    AddUIObjNewer(CLyricShowTxtContainer);
    AddUIObjNewer(CLyricShowAgentObj);
    AddUIObjNewer(CMPSkinTimeCtrl<CSkinPicText>);
    AddUIObjNewer(CMPSkinTimeCtrl<CSkinStaticText>);
    AddUIObjNewer(CMPSkinInfoTextCtrl);
    AddUIObjNewer(CMPSkinInfoTextCtrlEx);
    AddUIObjNewer(CMPSkinMediaNumInfoCtrl);
    AddUIObjNewer(CMediaInfoTextCtrl);
    AddUIObjNewer(CMediaAlbumArtCtrl);
    AddUIObjNewer(CMPlaylistCtrl);

    registerAboutPage(this);
    registerSearchLyricsPage(this);
    registerDownloadAlbumArtPage(this);
    registerPreferencePage(this);
    registerUploadLyrPage(this);
    registerAdjustHuePage(this);

    return CSkinFactory::init();
}

CSkinWnd *CMPSkinFactory::newSkinWnd(cstr_t szSkinWndName, bool bMainWnd) {
    if (bMainWnd) {
        return new CMPSkinMainWnd;
    } else {
        return new CMPSkinWnd;
    }
}

SkinMenuPtr CMPSkinFactory::newSkinMenu(CSkinWnd *pWnd, const rapidjson::Value &items) {
    auto menu = std::make_shared<CMPSkinMenu>();
    menu->addUICheckStatusIf(pWnd);
    menu->loadMenu(items);
    return menu;
}

void CMPSkinFactory::topmostAll(bool bTopmost) {
    auto itEnd = m_listSkinWnds.end();

    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pWnd = *it;
        pWnd->setTopmost(bTopmost);
    }
}

void CMPSkinFactory::minizeAll(bool bSilently) {
    auto itEnd = m_listSkinWnds.end();

    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pWnd = *it;
#ifdef _WIN32
        if (::GetParent(pWnd->getWndHandle()) != nullptr) {
            continue;
        }
#endif
        if (pWnd->isToolWindow()) {
            pWnd->minimizeNoActivate();
            pWnd->hide();
        } else if (bSilently) {
            pWnd->minimizeNoActivate();
        } else {
            pWnd->minimize();
        }
    }
}

void CMPSkinFactory::restoreAll() {
    auto itEnd = m_listSkinWnds.end();

    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pWnd = *it;

        if (pWnd->isIconic()) {
            pWnd->showNoActivate();
        }

        if (pWnd->isToolWindow()) {
            pWnd->show();
        }
    }
}

int CMPSkinFactory::getIDByNameEx(cstr_t szId, string &strToolTip) {
    int nID = CSkinFactory::getIDByNameEx(szId, strToolTip);
    if (nID == ID_INVALID) {
        return nID;
    }

    string strHotkey;

    if (MPlayerApp::getHotkey().getHotkeyText(nID, strHotkey)) {
        strToolTip = _TL(strToolTip.c_str());
        strToolTip += " (";
        strToolTip += strHotkey;
        strToolTip += ")";
    }

    return nID;
}

string CMPSkinFactory::getTooltip(int nId) {
    auto toolTip = CSkinFactory::getTooltip(nId);

    if (toolTip.size()) {
        string strHotkey;

        if (MPlayerApp::getHotkey().getHotkeyText(nId, strHotkey)) {
            toolTip = _TL(toolTip.c_str());
            toolTip += " (";
            toolTip += strHotkey;
            toolTip += ")";
        }
    }

    return toolTip;
}

void CMPSkinFactory::adjustHue(float hue, float saturation, float luminance) {
    CSkinFactory::adjustHue(hue, saturation, luminance);

    g_wndFloatingLyr.onAdjustHue(hue, saturation, luminance);
    g_wndFloatingLyr.invalidateRect();
}

void CMPSkinFactory::allUpdateTransparent() {
    int nOpaque = g_profile.getInt(SZ_SECT_UI, "WindowOpaquePercent", 100);
    uint8_t byAlpha = opaquePercentToAlpha(nOpaque);

    if (g_wndFloatingLyr.isValid()) {
        g_wndFloatingLyr.setTransparent(byAlpha, false);
    }

    auto itEnd = m_listSkinWnds.end();
    for (auto it = m_listSkinWnds.begin(); it != itEnd; ++it) {
        CSkinWnd *pWnd = *it;
        pWnd->setTransparent(byAlpha, m_bClickThrough);
    }
}

void CMPSkinFactory::setClickThrough(bool bClickThrough) {
    m_bClickThrough = bClickThrough;

    // save ClickThrough options
    g_profile.writeInt(SZ_SECT_UI, "ClickThrough", m_bClickThrough);

#ifdef _WIN32
    if (m_bClickThrough) {
        // show MusicPlayer system tray icon.
        int nShowPos;

        nShowPos = g_profile.getInt(SZ_SECT_UI, "ShowIconOn", SHOW_ICON_ON_TASKBAR);
        if (nShowPos == SHOW_ICON_ON_NONE || nShowPos == SHOW_ICON_ON_TASKBAR) {
            g_profile.writeInt(SZ_SECT_UI, "ShowIconOn", SHOW_ICON_ON_TASKBAR);
            MPlayerApp::getMainWnd()->getTrayIcon().updateShowIconPos();
        }
    }
#endif // #ifdef _WIN32

    allUpdateTransparent();
}
