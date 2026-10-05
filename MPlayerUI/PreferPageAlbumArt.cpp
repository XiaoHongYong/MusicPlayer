#include "MPlayerApp.h"
#include "PreferPageAlbumArt.h"
#include "AlbumArtDownloadMgr.h"


class CPagePfAlbumArt : public CPagePfBase {
    UIOBJECT_CLASS_NAME_DECLARE(CPagePfBase)
public:
    CPagePfAlbumArt() : CPagePfBase(PAGE_ALBUM_ART, "ID_ALBUM_ART_DOWNLOAD") {
        CID_AA_DOWN_DIR = 0;
        CID_C_AA_SAVE_SONG_DIR = 0;
        CID_C_AA_SAVE_CUSTOM = 0;
    }

    void onInitialUpdate() override {
        CPagePfBase::onInitialUpdate();

        GET_ID_BY_NAME3(CID_AA_DOWN_DIR, CID_C_AA_SAVE_SONG_DIR, CID_C_AA_SAVE_CUSTOM);

        addOptBool(ET_NULL, SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_ENABLE, true, "CID_C_ENABLE_ALBUM_ART_DL");

        {
            OptRadioInt opt;
            opt.set(ET_NULL, SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_SOURCE, ALBUM_ART_SRC_AUTO);
            opt.addCtrlValue(getIDByName("CID_C_AA_SRC_AUTO"), ALBUM_ART_SRC_AUTO);
            opt.addCtrlValue(getIDByName("CID_C_AA_SRC_MB"), ALBUM_ART_SRC_MUSICBRAINZ);
            opt.addCtrlValue(getIDByName("CID_C_AA_SRC_CHINA"), ALBUM_ART_SRC_CHINA);
            addOptRadioInt(opt);
        }
        {
            OptRadioInt opt;
            opt.set(ET_NULL, SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_SAVE_MODE, ALBUM_ART_SAVE_NEXT_TO_SONG);
            opt.addCtrlValue(CID_C_AA_SAVE_SONG_DIR, ALBUM_ART_SAVE_NEXT_TO_SONG);
            opt.addCtrlValue(CID_C_AA_SAVE_CUSTOM, ALBUM_ART_SAVE_CUSTOM_DIR);
            addOptRadioInt(opt);
        }

        setUIObjectText(CID_AA_DOWN_DIR, g_albumArtDownloader.getCustomSaveDir().c_str());
        int saveMode = g_profile.getInt(SZ_SECT_LYR_DL, SZ_KEY_ALBUM_ART_SAVE_MODE, ALBUM_ART_SAVE_NEXT_TO_SONG);
        enableUIObject(CID_AA_DOWN_DIR, saveMode == ALBUM_ART_SAVE_CUSTOM_DIR, false);

        initCheckButtons();
    }

    bool onCommand(uint32_t nId) override {
        if (nId == CID_AA_DOWN_DIR) {
            string dir = g_albumArtDownloader.getCustomSaveDir();
            CFolderDialog dlg(dir.c_str());
            if (dlg.doBrowse(m_pSkin)) {
                if (strcasecmp(dir.c_str(), dlg.getFolder()) != 0) {
                    dir = dlg.getFolder();
                    dirStringAddSep(dir);
                    if (!isDirWritable(dir.c_str())) {
                        m_pSkin->messageOut("Can't save album art in the selected folder.");
                    } else {
                        g_albumArtDownloader.setCustomSaveDir(dir.c_str());
                    }
                    setUIObjectText(nId, g_albumArtDownloader.getCustomSaveDir().c_str());
                }
            }
            return true;
        }

        bool handled = CPagePfBase::onCommand(nId);
        if (nId == CID_C_AA_SAVE_SONG_DIR || nId == CID_C_AA_SAVE_CUSTOM) {
            enableUIObject(CID_AA_DOWN_DIR, isButtonChecked(CID_C_AA_SAVE_CUSTOM));
            return true;
        }
        return handled;
    }

protected:
    int                         CID_AA_DOWN_DIR, CID_C_AA_SAVE_SONG_DIR, CID_C_AA_SAVE_CUSTOM;
};

UIOBJECT_CLASS_NAME_IMP(CPagePfAlbumArt, "PreferPage.AlbumArt")

//////////////////////////////////////////////////////////////////////////

UIOBJECT_CLASS_NAME_IMP(CPagePfAlbumArtRoot, "PreferPage.AlbumArtRoot")

CPagePfAlbumArtRoot::CPagePfAlbumArtRoot() : CPagePfBase(PAGE_UNKNOWN, "ID_ROOT_ALBUM_ART") {
}

void CPagePfAlbumArtRoot::onInitialUpdate() {
    CPagePfBase::onInitialUpdate();

    checkToolbarDefaultPage("CID_TOOLBAR_ALBUM_ART");
}

void registerPfAlbumArtPages(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPagePfAlbumArt);
    AddUIObjNewer2(pSkinFactory, CPagePfAlbumArtRoot);
}
