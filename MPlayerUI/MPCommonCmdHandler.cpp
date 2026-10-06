#include "MPlayerApp.h"
#include "MLCmd.h"
#include "MPCommonCmdHandler.h"
#include "LyricShowAgentObj.h"
#include "LyricShowTextEditObj.h"
#include "DlgSearchLyrics.h"
#include "DlgAlbumArtDownload.h"
#include "DlgUpload.h"
#include "AutoProcessEmbeddedLyrics.h"
#include "DlgSaveEmbeddedLyrics.hpp"
#include "PreferenceDlg.h"
#include "DlgAbout.h"
#include "MPSkinMenu.h"
#include "DownloadMgr.h"
#include "DlgAdjustHue.h"
#include "MPHelper.h"
#include "MPFloatingLyrWnd.h"
#include "../Skin/SkinRateCtrl.h"
#include "../LocalServer/LocalServer.hpp"


bool g_bInModalDlLrcSelDlg;

const int SET_SPEED_SPAN = 500;

CMPCommonCmdHandler::CMPCommonCmdHandler(bool bFloatingLyr) {
    MPlayerApp::getInstance()->getCurLyrDisplaySettingName(bFloatingLyr, m_strSectName, m_etDispSettings);
}

CMPCommonCmdHandler::~CMPCommonCmdHandler() {

}

// if the command id is processed, return true.
bool CMPCommonCmdHandler::onCommand(uint32_t nID) {
    switch (nID) {
    case ID_EDITOR_LYR_COLOR:
    case ID_EDITOR_HIGH_COLOR:
    case ID_TAG_COLOR:
    case ID_EDIT_LINE_COLOR:
    case ID_EDITOR_BG_COLOR:
        {
            cstr_t szColorValueName = nullptr;
            if (nID == ID_EDITOR_LYR_COLOR) {
                szColorValueName = "Ed_LowColor";
            } else if (nID == ID_EDITOR_HIGH_COLOR) {
                szColorValueName = "Ed_HighColor";
            } else if (nID == ID_TAG_COLOR) {
                szColorValueName = "Ed_TagColor";
            } else if (nID == ID_EDITOR_BG_COLOR) {
                szColorValueName = "Ed_BgColor";
            } else {
                szColorValueName = "Ed_FocusLineBgColor";
            }

            CColor clr(RGB(0, 0, 0));
            profileGetColorValue(clr, SZ_SECT_LYR_DISPLAY, szColorValueName);

            CDlgChooseColor dlg;
            if (dlg.doModal(m_pSkinWnd, clr) == IDOK) {
                clr = dlg.getColor();
                CMPlayerSettings::setSettings(ET_LYRICS_DISPLAY_SETTINGS,
                    SZ_SECT_LYR_DISPLAY, szColorValueName, colorToStr(clr).c_str());
            }
        }
        break;
    case ID_NO_SUITTABLE_LYRICS:
    case ID_INSTRUMENTAL_MUSIC:
        if (g_LyricSearch.associateLyrics(g_player.getMediaKey().c_str(), NONE_LYRCS)) {
            MPlayerApp::getInstance()->dispatchResearchLyrics();
        }
        break;
    case ID_SEARCH_LYR_SUGGESTIONS:
        openUrl(m_pSkinWnd, getStrName(SN_HTTP_HELP_SEARCH_LYR_SUGGESTIONS));
        break;

    case ID_RATE_LYR_1:
    case ID_RATE_LYR_2:
    case ID_RATE_LYR_3:
    case ID_RATE_LYR_4:
    case ID_RATE_LYR_5:
        {
            int nRate;
            if (nID == ID_RATE_LYR_2) nRate = 2;
            else if (nID == ID_RATE_LYR_3) nRate = 3;
            else if (nID == ID_RATE_LYR_4) nRate = 4;
            else if (nID == ID_RATE_LYR_5) nRate = 5;
            else nRate = 1;

            string strUrl;

            strUrl = getStrName(SN_HTTP_RATE_LRC);
            strUrl += g_currentLyrics.properties().id;

            strUrl += stringPrintf("&rating=%d", nRate).c_str();
            openUrl(m_pSkinWnd, strUrl.c_str());
        }
        break;
    case ID_LDO_KARAOKE:
        CMPlayerSettings::setSettings(m_etDispSettings, m_strSectName.c_str(), "Karaoke",
            !g_profile.getBool(m_strSectName.c_str(), "Karaoke", false));
        break;
    case ID_LDO_NORMAL:
    case ID_LDO_FADE_IN:
    case ID_LDO_FADEOUT_BG:
    case ID_LDO_AUTO:
        {
            DISPLAY_OPTIONS displayOpt;

            if (nID == ID_LDO_NORMAL) {
                displayOpt = DO_NORMAL;
            } else if (nID == ID_LDO_FADE_IN) {
                displayOpt = DO_FADEOUT_LOWCOLOR;
            } else if (nID == ID_LDO_FADEOUT_BG) {
                displayOpt = DO_FADEOUT_BG;
            } else if (nID == ID_LDO_AUTO) {
                displayOpt = DO_AUTO;
            } else {
                return false;
            }

            CMPlayerSettings::setSettings(m_etDispSettings, m_strSectName.c_str(), "LyrDrawOpt",
                displayOptToStr(displayOpt));
        }
        break;

    case ID_SHOW_MAIN_WND:
        m_pSkinWnd->activateWindow();
        break;
    case ID_TOGGLE_MP:
        {
            if (m_pSkinWnd->isIconic() || !m_pSkinWnd->isForeground()) {
                m_pSkinWnd->activateWindow();
            } else {
                m_pSkinWnd->minimizeNoActivate();
                if (m_pSkinWnd->isToolWindow()) {
                    m_pSkinWnd->hide();
                }
            }
        }
        break;
    case ID_ADJUST_HUE:
        {
            showAdjustHueDialog(m_pSkinWnd);
        }
        break;

    case ID_OPEN_MEDIA_CENTER:
        {
            // 确保 LocalServer 已启动，再打开浏览器访问媒体中心.
            try {
                openUrl(m_pSkinWnd, LocalServer::getInstance()->getHttpBaseUrl().c_str());
            } catch (const std::exception &e) {
                ERR_LOG1("Open media center failed: %s", e.what());
            }
        }
        break;

    case ID_FLOATING_LYRICS:
        {
            g_profile.writeInt("FloatingLyr", !g_wndFloatingLyr.isValid());
            if (!g_wndFloatingLyr.isValid()) {
                g_wndFloatingLyr.create();
            } else {
                g_wndFloatingLyr.destroy();
            }
        }
        break;

    case ID_PREFERENCES:
        showPreferenceDialog(m_pSkinWnd);
        break;
    case ID_DISPLAY_OPT:
        showPreferenceDialog(m_pSkinWnd, m_etDispSettings == ET_LYRICS_FLOATING_SETTINGS ? PAGE_FLOAT_LYR_DISPLAY : PAGE_LYR_DISPLAY);
        break;

    case ID_WEBHOME:
        openUrl(m_pSkinWnd, getStrName(SN_HTTP_DOMAIN));
        break;
    case ID_EMAIL:
        // feed back
        {
            string str;

            str = getStrName(SN_EMAIL);
            str += "?subject=feedback";

            openUrl(m_pSkinWnd, str.c_str());
        }
        break;
    case ID_APPLY_ACCOUNT:
        {
            openUrl(m_pSkinWnd, getStrName(SN_HTTP_SIGNUP));
        }
        break;
    case ID_LOGIN_VIA_IE:
        {
            openUrl(m_pSkinWnd, getStrName(SN_HTTP_LOGIN));
        }
        break;
    case ID_ABOUT:
        {
            /*            CSkinWnd    *pWnd = new CDlgAbout();
            int nRet = MPlayerApp::getMPSkinFactory()->openOrCloseSkinWnd("about_box",
                "AboutBox",
                "AboutBox.xml", MPlayerApp::getMainWnd(), &pWnd);*/
            showAboutDialog(m_pSkinWnd);
        }
        break;
    case ID_BR_ALBUM_ART:
        {
            string strAlbumArtFileName = fileGetPath(g_player.getSrcMedia());
            if (!isDirExist(strAlbumArtFileName.c_str())) {
                break;
            }

            if (isEmptyString(g_player.getAlbum())) {
                strAlbumArtFileName += "Folder";
            } else {
                strAlbumArtFileName += g_player.getAlbum();
            }

            CFileDlgExtFilter strFileExt;
            strFileExt.addExtention(_TLT("All Picture Files"), "*.jpg;*.gif;*.bmp;*.png");
            strFileExt.addExtention("JPEG (*.jpg)", "*.jpg");
            strFileExt.addExtention("PNG (*.png)", "*.png");
            strFileExt.addExtention(_TLT("Bitmap files (*.bmp)"), "*.bmp");
            CFileOpenDlg        dlg(_TLT("Browse album art picture file"), "",
                strFileExt.c_str(), 0);

            if (dlg.doModal(m_pSkinWnd) != IDOK) {
                break;
            }

            strAlbumArtFileName += fileGetExt(dlg.getOpenFile());

            if (strcasecmp(strAlbumArtFileName.c_str(), dlg.getOpenFile()) == 0) {
                break;
            }

            copyFile(dlg.getOpenFile(), strAlbumArtFileName.c_str(), false);

            MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_PLAYER_CUR_MEDIA_INFO_CHANGED);
        }
        break;
    case ID_DL_ALBUM_ART:
        showDownloadAlbumArtDialog(m_pSkinWnd);
        break;
    case ID_LYR_EDITOR:
        {
            SkinWndStartupInfo skinWndStartupInfo("LyrEditor", _TLT("Lyrics Editor"), "LyricsEditor.xml", nullptr);
            MPlayerApp::getMPSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
        }
        break;
    case ID_EDIT_HELP:
        openUrl(m_pSkinWnd, getStrName(SN_HTTP_HELP_EDIT_LYR));
        break;
    case ID_HELP:
        {
            CUIObject *pObjLyrEdit = m_pSkinWnd->getUIObjectByClassName(CLyricShowTextEditObj::className());
            if (pObjLyrEdit && pObjLyrEdit->isVisible()) {
                openUrl(m_pSkinWnd, getStrName(SN_HTTP_HELP_EDIT_LYR));
            } else {
                openUrl(m_pSkinWnd, getStrName(SN_HTTP_HELP));
            }
        }
        break;
    case ID_PAUSE:
        g_player.pause();
        break;
    case ID_PLAY:
        g_player.play();
        break;
    case ID_PLAYPAUSE:
        g_player.playPause();
        break;
    case ID_STOP:
        g_player.stop();
        break;
    case ID_NEXT:
        g_player.next();
        break;
    case ID_PREVIOUS:
        g_player.prev();
        break;
    case ID_SEEK:
        {
            CSkinSeekCtrl *pCtrl = (CSkinSeekCtrl*)m_pSkinWnd->getUIObjectById(ID_SEEK, CSkinSeekCtrl::className());
            if (pCtrl) {
                g_player.seekTo(pCtrl->getScrollPos());
            }
        }
        break;
    case ID_BACKWARD:
        {
            int dwCurPos;

            dwCurPos = g_player.getPlayPos();
            dwCurPos -= 2000;
            if (dwCurPos < 0) {
                dwCurPos = 0;
            }
            dwCurPos -= dwCurPos % 1000;
            g_player.seekTo(dwCurPos);
        }
        break;
    case ID_FORWARD:
        {
            int dwCurPos;

            dwCurPos = g_player.getPlayPos();
            dwCurPos += 2000;
            if (dwCurPos < 0) {
                dwCurPos = 0;
            }
            dwCurPos -= dwCurPos % 1000;
            g_player.seekTo(dwCurPos);
        }
        break;
    case ID_VOL_INC:
        {
            int vol = g_player.getVolume();
            if (vol < MP_VOLUME_MAX) {
                vol += 5;
                vol -= vol % 5;
                if (vol > MP_VOLUME_MAX) {
                    vol = MP_VOLUME_MAX;
                }
                g_player.setVolume(vol);
                MPlayerApp::getInstance()->dispatchInfoText(stringPrintf("%s %d%%", _TLT("Set volume"), vol).c_str());
            }
        }
        break;
    case ID_VOL_DEC:
        {
            int vol = g_player.getVolume();
            if (vol > 0) {
                vol -= 5;
                vol -= vol % 5;
                if (vol < 0) {
                    vol = 0;
                }
                g_player.setVolume(vol);
                MPlayerApp::getInstance()->dispatchInfoText(stringPrintf("%s %d%%", _TLT("Set volume"), vol).c_str());
            }
        }
        break;
    case ID_MUTE:
        {
            bool bMute = !g_player.isMute();
            g_player.setMute(bMute);
            MPlayerApp::getInstance()->dispatchInfoText(bMute ? _TL("Mute On") : _TL("Mute Off"));
        }
        break;
    case ID_SHUFFLE:
        {
            bool bShuffle = !g_player.isShuffle();
            g_player.setShuffle(bShuffle);
            MPlayerApp::getInstance()->dispatchInfoText(bShuffle ? _TL("Shuffle On") : _TL("Shuffle Off"));
        }
        break;
    case ID_LOOP_OFF:
        g_player.setLoop(MP_LOOP_OFF);
        MPlayerApp::getInstance()->dispatchInfoText(_TL("Repeat Off"));
        break;
    case ID_LOOP_ALL:
        g_player.setLoop(MP_LOOP_ALL);
        MPlayerApp::getInstance()->dispatchInfoText(_TL("Repeat All"));
        break;
    case ID_LOOP_TRACK:
        g_player.setLoop(MP_LOOP_TRACK);
        MPlayerApp::getInstance()->dispatchInfoText(_TL("Repeat Track"));
        break;
    case ID_LOOP:
        {
            g_player.setToNextLoopMode();

            string str;
            LoopMode loopMode = g_player.getLoop();
            if (loopMode == MP_LOOP_OFF) {
                str = _TL("Repeat Off");
            } else if (loopMode == MP_LOOP_TRACK) {
                str = _TL("Repeat Track");
            } else {
                str = _TL("Repeat All");
            }

            MPlayerApp::getInstance()->dispatchInfoText(str.c_str());
        }
        break;
    case ID_RATE:
        {
            CSkinRateCtrl *pRateCtrl;
            pRateCtrl = (CSkinRateCtrl*)m_pSkinWnd->getUIObjectById(ID_RATE, CSkinRateCtrl::className());
            if (pRateCtrl) {
                auto mediaLib = g_player.getMediaLibrary();
                auto media = g_player.getCurrentMedia();
                if (media) {
                    mediaLib->rate(media.get(), pRateCtrl->getRating());
                }
            }
        }
        break;
    case ID_PL_OPEN_FILE:
        onSongOpenFileCmd(m_pSkinWnd, true);
        break;
    case ID_PL_ADD_FILE:
        onSongOpenFileCmd(m_pSkinWnd, false);
        break;
    case ID_PL_OPEN_DIR:
        onSongOpenDirCmd(m_pSkinWnd, true);
        break;
    case ID_PL_ADD_DIR:
        onSongOpenDirCmd(m_pSkinWnd, false);
        break;

        //
        // Lyrics ....
        //
    case ID_NEW_LRC:
        {
            MPlayerApp::getInstance()->getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_ON_SAVE_EDIT);

            if (g_currentLyrics.isContentModified()) {
                string        strMessage = stringPrintf(_TLT("The lyrics of the %s file have changed."),
                    fileGetName(g_currentLyrics.getMediaSource())).c_str();
                strMessage += "\r\n\r\n";
                strMessage += _TLT("Do you want to save the changes?");
                int nRet = m_pSkinWnd->messageOut(strMessage.c_str(), MB_ICONQUESTION | MB_YESNOCANCEL);
                if (nRet == IDCANCEL) {
                    break;
                } else if (nRet == IDYES) {
                    if (!CMPCommonCmdHandler::saveCurrentLyrics(m_pSkinWnd, false)) {
                        break;
                    }
                }
            }
            MPlayerApp::getInstance()->newLyrics();
        }
        break;
    case ID_OPEN_LRC:
        //
        // 歌词下载
        //
        if (!g_player.isMediaOpened()) {
            m_pSkinWnd->messageOut(_TLT("No media was opened."));
            break;
        }

        {
            if (!g_bInModalDlLrcSelDlg) {
                g_bInModalDlLrcSelDlg = true;

                showSearchLyricsDialog(m_pSkinWnd);
                g_bInModalDlLrcSelDlg = false;
            }
        }
        break;
    case ID_SAVE_LRC:
        saveCurrentLyrics(m_pSkinWnd, true);
        break;
    case ID_SAVE_LRC_AS:
        {
            // notify to save...
            IEvent *pEvent = new IEvent;
            pEvent->eventType = ET_LYRICS_ON_SAVE_EDIT;
            MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(pEvent);

            saveAsLyricsFile(m_pSkinWnd);
        }
        break;
    case ID_SAVE_LYR_IN_SONG_FILE:
        showSaveEmbeddedLyricsDialog(m_pSkinWnd);
        break;
    case ID_FORWARD_LYRICS:
        {
            CUIObject *pObj;
            pObj = m_pSkinWnd->getUIObjectByClassName(CLyricShowTxtObj::className());
            if (pObj) {
                if (pObj) {
                    pObj->onKeyDown(VK_DOWN, 0);
                }
            } else {
                g_currentLyrics.setOffsetTime(g_currentLyrics.getOffsetTime() + SET_SPEED_SPAN);
                MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);

                MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_EDITOR_RELOAD_TAG);

                MPlayerApp::getInstance()->dispatchInfoText(_TLT("Lyrics Forward 0.5 Sec"));
            }
        }
        break;
    case ID_BACKWARD_LYRICS:
        {
            CUIObject *pObj;
            pObj = m_pSkinWnd->getUIObjectByClassName(CLyricShowTxtObj::className());
            if (pObj) {
                // move down one line.
                if (pObj) {
                    pObj->onKeyDown(VK_UP, 0);
                }
            } else {
                g_currentLyrics.setOffsetTime(g_currentLyrics.getOffsetTime() - SET_SPEED_SPAN);
                MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);

                MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_EDITOR_RELOAD_TAG);

                MPlayerApp::getInstance()->dispatchInfoText(_TLT("Lyrics Backward 0.5 Sec"));
            }
        }
        break;

    case ID_FONT_SIZE_INC:
    case ID_FONT_SIZE_DEC:
        {
            // increase the font size.
            FontInfoEx font;

            // get default settings in profile
            profileGetLyricsFont(m_strSectName.c_str(), font);

            if (nID == ID_FONT_SIZE_INC) {
                font.height++;
                if (font.height >= 96 * 2) { // 96 is the max font size in font choosing dialog.
                    break;
                }
            } else {
                font.height--;
                if (font.height < 8) { // 8 is a reasonable smallest font size.
                    break;
                }
            }

            // now save to profile
            profileWriteLyricsFont(m_etDispSettings, m_strSectName.c_str(), font);
        }
        break;
    case ID_CLR_PREV_HUE:
    case ID_CLR_NEXT_HUE:
        {
            float hue = (float)(g_profile.getInt(MPlayerApp::getMPSkinFactory()->getSkinName(), "Hue", 0));

            if (nID == ID_CLR_NEXT_HUE) {
                hue += 30;
            } else {
                hue -= 30;
            }
            if (hue >= 360)   hue -= 360;
            else if (hue < 0) hue += 360;

            g_profile.writeInt(MPlayerApp::getMPSkinFactory()->getSkinName(), "Hue", (int)hue);
            MPlayerApp::getMPSkinFactory()->adjustHue(hue);
        }
        break;
    case ID_LYR_HIGH_CLR_LIST:
        {
            CRect rc;
            CUIObject *pObj = m_pSkinWnd->getUIObjectById(ID_LYR_HIGH_CLR_LIST, nullptr);
            if (pObj) {
                rc = pObj->m_rcObj;
                m_pSkinWnd->screenToClient(rc);
            } else {
                CPoint pt = getCursorPos();
                rc.setLTRB(pt.x, pt.y, pt.x + 100, pt.y + 25);
            }
            m_popupHighClrListWnd.create(m_pSkinWnd, rc, true,
                m_etDispSettings == ET_LYRICS_FLOATING_SETTINGS, nullptr);
        }
        break;
    case ID_ALBUMART:
        {
            // album art
            m_pSkinWnd->getSkinFactory()->showPopupMenu(m_pSkinWnd, "AlbumArtMenu");
        }
        break;
    case ID_RATE_LYR:
        {
            if (!g_currentLyrics.hasLyricsOpened()) {
                m_pSkinWnd->messageOut(_TLT("No Lyrics file was opened."));
                break;
            }

            m_pSkinWnd->getSkinFactory()->showPopupMenu(m_pSkinWnd, "RateMenu");
        }
        break;
    case ID_RELOAD_LYR:
        MPlayerApp::getInstance()->dispatchResearchLyrics();
        break;
    case ID_J_PREV_LINE:
    case ID_J_NEXT_LINE:
        {
            // Jump to next/previous line of lyrics.
            LyricsLines &vLyrics = g_currentLyrics.getLyricsLines();

            int nCurLine = g_currentLyrics.getCurPlayLine(vLyrics);
            if (nCurLine >= 0 && nCurLine < (int)vLyrics.size()) {
                if (nID == ID_J_NEXT_LINE) {
                    if (nCurLine == vLyrics.size() - 1) {
                        return true;
                    }
                    nCurLine++;
                } else {
                    if (nCurLine > 0) {
                        nCurLine--;
                    }
                }

                g_player.seekTo(vLyrics[nCurLine].beginTime);
            }
        }
        break;
    case ID_UPLOAD_LYR:
        {
            MPlayerApp::getInstance()->getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_ON_SAVE_EDIT);

            if (!g_currentLyrics.hasLyricsOpened()) {
                m_pSkinWnd->messageOut(_TLT("No Lyrics file was opened."));
                break;
            }

            auto &props = g_currentLyrics.properties();
            if (props.artist.empty()) { props.artist = g_player.getArtist(); }
            if (props.title.empty()) { props.title = g_player.getTitle(); }
            if (props.album.empty()) { props.album = g_player.getAlbum(); }
            if (props.mediaLength.empty()) { props.setMediaLength(g_player.getMediaLength() / 1000); }

            if (g_currentLyrics.doesChooseNewFileName()) {
                if (!saveAsLyricsFile(m_pSkinWnd)) {
                    return false;
                }
            }

            // show upload lyrics dialog
            string strLyrics = g_currentLyrics.toString(true);
            showUploadLyrDialog(m_pSkinWnd,
                strLyrics,
                g_currentLyrics.getMediaSource(),
                g_currentLyrics.getLyricsFileName());
        }
        break;
    case ID_EXTERNAL_LYR_EDIT:
        //
        // Edit lyrics with external editor.
        if (g_currentLyrics.getLyricsSourceType() != LST_FILE) {
            m_pSkinWnd->messageOut(_TLT("Embedded lyrics cannot be edited with external editors."));
            break;
        }

        if (g_currentLyrics.hasLyricsOpened()) {
            if (!MPlayerApp::getInstance()->onLyricsChangingSavePrompt()) {
                break;
            }

            string strEditor;
            getNotepadEditor(strEditor);
            execute(m_pSkinWnd,
                CMLProfile::getDir(SZ_SECT_UI, "LyricsEditor", strEditor.c_str()).c_str(),
                g_currentLyrics.getLyricsFileName());
        }
        break;
    default:
        {
            if (onCommandCharEncoding(nID)) {
                break;
            }
            if (onCommandSkin(nID)) {
                break;
            }
            return false;
        }
    }

    return true;
}

bool CMPCommonCmdHandler::onUIObjNotify(IUIObjNotify *pNotify) {
    if (pNotify->nID == ID_SEEK) {
        if (pNotify->pUIObject->isKindOf(CSkinSeekCtrl::className())) {
            CSkinSeekCtrlEventNotify *pListCtrlNotify = (CSkinSeekCtrlEventNotify *)pNotify;
            if (pListCtrlNotify->cmd == CSkinSeekCtrlEventNotify::C_BEG_DRAG) {
                // Use seek time instead of playing time.
                g_player.setUseSeekTimeAsPlayingTime(true);
            } else if (pListCtrlNotify->cmd == CSkinSeekCtrlEventNotify::C_END_DRAG) {
                // Use seek time instead of playing time.
                g_player.setUseSeekTimeAsPlayingTime(false);
            }
        }
    } else {
        return false;
    }

    return true;
}

bool CMPCommonCmdHandler::getChecked(uint32_t nID, bool &bChecked) {
    switch (nID) {
    case ID_LDO_KARAOKE:
        bChecked = g_profile.getBool(m_strSectName.c_str(), "Karaoke", false);
        break;
    case ID_FLOATING_LYRICS:
        bChecked = g_wndFloatingLyr.isValid();
        break;
    default:
        return false;
    }

    return true;
}

bool CMPCommonCmdHandler::getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) {
    switch (vIDs[0]) {
    case ID_LDO_NORMAL:
    case ID_LDO_FADE_IN:
    case ID_LDO_FADEOUT_BG:
    case ID_LDO_AUTO:
        {
            DISPLAY_OPTIONS        displayOpt = displayOptFromStr(
                g_profile.getString(m_strSectName.c_str(), "LyrDrawOpt", ""));

            if (displayOpt == DO_NORMAL) {
                nIDChecked = ID_LDO_NORMAL;
            } else if (displayOpt == DO_FADEOUT_LOWCOLOR) {
                nIDChecked = ID_LDO_FADE_IN;
            } else if (displayOpt == DO_FADEOUT_BG) {
                nIDChecked = ID_LDO_FADEOUT_BG;
            } else {
                nIDChecked = ID_LDO_AUTO;
            }
        }
        break;
    case ID_LOOP_OFF:
    case ID_LOOP_ALL:
    case ID_LOOP_TRACK:
        {
            switch (g_player.getLoop()) {
                case MP_LOOP_ALL: nIDChecked = ID_LOOP_ALL; break;
                case MP_LOOP_TRACK: nIDChecked = ID_LOOP_TRACK; break;
                case MP_LOOP_OFF: nIDChecked = ID_LOOP_OFF; break;
            }
            break;
        }
    default:
        return false;
    }

    return true;
}

bool CMPCommonCmdHandler::onCommandCharEncoding(int nCmdId) {
    int nEncodingId = cmdIdToEncoding(nCmdId);
    if (nEncodingId == -1) {
        return false;
    }

    if (!g_currentLyrics.hasLyricsOpened()) {
        return true;
    }

    //
    // if (修改了歌词)
    //        提示需要重新加载歌词，提示用户是否继续
    if (g_currentLyrics.isContentModified()) {
        string str;

        str += _TLT("Changing the character encoding will reload the lyrics, and your modifications will be lost.");
        str += "\r\n";
        str += _TLT("Do you want to continue?");
        if (m_pSkinWnd->messageOut(str.c_str(), MB_ICONINFORMATION | MB_YESNO) != IDYES) {
            return true;
        }
    }

    // 以用户指定的编码重新打开歌词
    g_currentLyrics.reopenWithEncoding((CharEncodingType)nEncodingId);

    MPlayerApp::getInstance()->dispatchLyricsChangedSyncEvent();

    return true;
}

// COMMENT:
//        保存歌词文件为：
//        FT_UNKNOWN            = 1,
//        FT_LYRICS_LRC        = 2,
//        FT_LYRICS_TXT        = 3,
// RETURN:
//        true    -    保存成功，注意：如果没有歌词数据也返回true；
//        false    -    用户选择取消，或者保存失败;
bool CMPCommonCmdHandler::saveAsLyricsFile(Window *pWndParent) {
    if (!g_currentLyrics.hasLyricsOpened()) {
        pWndParent->messageOut(_TLT("No Lyrics file was opened."));
        return true;
    }

    string strFile = g_currentLyrics.getLyricsFileName();
    if (strFile.empty() || !isFileExist(strFile.c_str())) {
        strFile = g_LyricsDownloader.getSaveLyricsFile(g_currentLyrics.getMediaSource(),
            g_currentLyrics.getSuggestedLyricsFileName().c_str());
    }

    int nFileExtIndex;
    if (g_currentLyrics.getLyrContentType() >= LCT_LRC) {
        fileSetExt(strFile, ".lrc");
        nFileExtIndex = 2;
    } else {
        fileSetExt(strFile, ".txt");
        nFileExtIndex = 3;
    }

    CFileSaveDlg        dlg(_TLT("Save As"), strFile.c_str(),
        "All supported files (*.lrc; *.txt; *.snc)\0*.lrc;*.txt;*.snc\0LRC Lyrics File (*.lrc)\0*.lrc\0Text File (*.txt)\0*.txt\0Snc File (*.snc)\0*.snc\0\0",
        nFileExtIndex);

    if (dlg.doModal(pWndParent) == IDOK) {
        strFile = dlg.getSaveFile();

        bool bUseNewFileName;
        int nRet = g_currentLyrics.saveAsFile(strFile.c_str(), bUseNewFileName);
        if (nRet != ERR_OK) {
            pWndParent->messageOut(ERROR2STR_LOCAL(nRet));
            return false;
        }
        if (bUseNewFileName) {
            g_LyricSearch.associateLyrics(g_currentLyrics.getMediaSource(), g_currentLyrics.getLyricsFileName());
        }
    } else {
        return false;
    }

    return true;
}

bool CMPCommonCmdHandler::saveCurrentLyrics(CSkinWnd *pSkinWnd, bool bDispatchOnSave) {
    int nRet;

    if (bDispatchOnSave) {
        // Dispatch on save lyrics message.
        IEvent *pEvent = new IEvent;
        pEvent->eventType = ET_LYRICS_ON_SAVE_EDIT;
        MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(pEvent);
    }

    if (g_currentLyrics.doesChooseNewFileName()) {
        if (!saveAsLyricsFile(pSkinWnd)) {
            return false;
        }
    } else {
        nRet = g_currentLyrics.save();
        if (nRet != ERR_OK) {
            if (g_currentLyrics.getLyricsSourceType() != LST_FILE) {
                VecStrings vLyrNames;
                vLyrNames.push_back(g_currentLyrics.getLyricsFileName());

                g_autoProcessEmbeddedLyrics.saveEmbeddedLyrics(g_currentLyrics.getMediaSource(),
                    g_currentLyrics.toString(true), vLyrNames);
            }
            pSkinWnd->messageOut(stringPrintf("%s\n%s", ERROR2STR_LOCAL(nRet), _TLT("Failed to save embedded lyrics. $Product$ will try again later.")).c_str());
        }
    }

    return true;
}

LyricsLines &CMPCommonCmdHandler::getDisplayLyrics() {
    CLyricShowObj *pObj = (CLyricShowObj*)m_pSkinWnd->getUIObjectByClassName(CLyricShowMultiRowObj::className());
    if (pObj) {
        return pObj->getLyrics();
    } else {
        return g_currentLyrics.getLyricsLines();
    }
}
