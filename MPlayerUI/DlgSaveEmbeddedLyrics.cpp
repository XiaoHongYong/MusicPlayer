//
//  DlgSaveEmbeddedLyrics.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/9/18.
//

#include "MPlayerApp.h"
#include "DlgSaveEmbeddedLyrics.hpp"
#include "AutoProcessEmbeddedLyrics.h"
#include "../LyricsLib/CurrentLyrics.h"

int saveEmbeddedLyrics(const string &mediaUrl, const string &lyrics, const VecStrings &lyrUrls, CSkinWnd *parent) {
    int ret = g_autoProcessEmbeddedLyrics.saveEmbeddedLyrics(mediaUrl.c_str(), lyrics, lyrUrls);
    if (ret != ERR_OK) {
        string strMsg = _TLT("Failed to save embedded lyrics. $Product$ will try again later.");
        strMsg += "\n\n";
        strMsg += ERROR2STR_LOCAL(ret);
        parent->messageOut(strMsg.c_str());
    }

    return ret;
}

class SkinWndSaveEmbeddedLyrics : public CMPSkinWnd {
public:
    void onCreate() override {
        CMPSkinWnd::onCreate();

        auto suggestedUrl = MediaTags::getSuggestedEmbeddedLyricsUrl(_mediaFile.c_str());

        for (int i = 0; i < 5; i++) {
            _checkIds.push_back(m_pSkinFactory->getIDByName(("CID_C_EL" + std::to_string(i)).c_str()));
            auto obj = getUIObjectById(_checkIds[i]);
            assert(obj);
            if (i < _embeddedLyrUrls.size()) {
                auto lst = lyrSrcTypeFromName(_embeddedLyrUrls[i].c_str());
                obj->setText(lyrSrcTypeToDesc(lst));
                if (_embeddedLyrUrls[i] == suggestedUrl) {
                    m_rootConainter.checkButton(_checkIds[i], true);
                }
            } else {
                obj->setVisible(false);
            }
        }
    }

    void onCommand(uint32_t id) override {
        if (id == ID_OK) {
            VecStrings choosedUrls;
            for (int i = 0; i < _embeddedLyrUrls.size() && i < _checkIds.size(); i++) {
                if (m_rootConainter.isButtonChecked(_checkIds[i])) {
                    choosedUrls.push_back(_embeddedLyrUrls[i]);
                }
            }

            if (saveEmbeddedLyrics(_mediaFile.c_str(), _lyrics, choosedUrls, this) != ERR_OK) {
                return;
            }
        }

        CMPSkinWnd::CSkinWnd::onCommand(id);
    }

public:
    string                  _mediaFile;
    string                  _lyrics;
    VecStrings              _embeddedLyrUrls;
    VecInts                 _checkIds;

};

void showSaveEmbeddedLyricsDialog(CSkinWnd *parent) {
    if (!g_currentLyrics.hasLyricsOpened()) {
        parent->messageOut(_TLT("No Lyrics file was opened."));
        return;
    }

    // Get current media and lyrics information
    string mediaUrl = g_player.getSrcMedia();
    if (mediaUrl.empty() || !isFileExist(mediaUrl.c_str())) {
        parent->messageOut(_TL("Can't locate the song file."));
        return;
    }

    auto lyrics = g_currentLyrics.toString();

    VecStrings lyrUrls = MediaTags::getSupportedEmbeddedLyricsUrls(mediaUrl.c_str());

    if (lyrUrls.size() == 1) {
        // save lyrics directly
        if (saveEmbeddedLyrics(mediaUrl, lyrics, lyrUrls, parent) == ERR_OK) {
            parent->messageOut(_TL("Embedded Lyrics were saved successfully."));
        }
    } else if (lyrUrls.size() > 1) {
        SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, _SZ_SKINWND_CLASS_NAME,
            "SaveEmbeddedLyrics.xml", parent);
        SkinWndSaveEmbeddedLyrics *dialog = new SkinWndSaveEmbeddedLyrics();
        skinWndStartupInfo.pSkinWnd = dialog;

        dialog->_mediaFile = mediaUrl;
        dialog->_lyrics = lyrics;
        dialog->_embeddedLyrUrls = lyrUrls;

        CSkinApp::getInstance()->getSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
    } else {
        parent->messageOut(_TL("This file doesn't support embedded lyrics."));
    }
}
