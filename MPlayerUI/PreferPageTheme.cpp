#include "MPlayerApp.h"
#include "PreferPageTheme.h"
#include "LyricShowAgentObj.h"
#include "ThemesFile.h"
#include "LyrDisplayClrListWnd.h"
#include "MLProfile.h"


//////////////////////////////////////////////////////////////////////////

class CPagePfLyricsBgPic : public CPagePfBase {
    UIOBJECT_CLASS_NAME_DECLARE(CPagePfBase)
public:
    CPagePfLyricsBgPic() : CPagePfBase(PAGE_LYR_BG, "ID_THEME_LYR_BG_PIC") {
        m_bIntialized = false;
    }

    void onInitialUpdate() override {
        CPagePfBase::onInitialUpdate();

        m_bIntialized = true;

        addOptBool(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "UseAlbumArtAsBg", false, "CID_C_USE_ALBUM_ART_AS_BG");

        addOptBool(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "UseBgImage", false, "CID_C_ENABLE_BG_IMAGE");

        addOptBool(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "DarkenLyrBg", true, "CID_C_DARKEN_LYR_BG");

        setUIObjectText("CID_E_DELAY_TIME",
            g_profile.getString(SZ_SECT_LYR_DISPLAY, "SlideDelayTime", "10"));

        initCheckButtons();

        setUIObjectText("CID_E_PIC_FOLDER",
            CMLProfile::getDir(SZ_SECT_LYR_DISPLAY, "BgPicFolder", "").c_str());

    }

    void onDestroy() override {
        if (m_bIntialized) {
            int nDelayTime;
            string strDelayTime = getUIObjectText("CID_E_DELAY_TIME");
            nDelayTime = atoi(strDelayTime.c_str());
            if (nDelayTime > 0) {
                if (nDelayTime < 10) {
                    nDelayTime = 10;
                }
                if (nDelayTime > 10000) {
                    nDelayTime = 10000;
                }

                if (nDelayTime != g_profile.getInt(SZ_SECT_LYR_DISPLAY, "SlideDelayTime", 10)) {
                    CMPlayerSettings::setLyricsDisplaySettings("SlideDelayTime", nDelayTime);
                }
            }
        }

        CPagePfBase::onDestroy();
    }

    bool onCommand(uint32_t nId) override {
        if (nId == getIDByName("CID_BR_PIC_FOLDER")) {
            onCommandSetBkImg();

            setUIObjectText("CID_E_PIC_FOLDER",
                CMLProfile::getDir(SZ_SECT_LYR_DISPLAY, "BgPicFolder", "").c_str());
        } else {
            return CPagePfBase::onCommand(nId);
        }

        return true;
    }

    void onCommandSetBkImg() {
        CFolderDialog dlg(CMLProfile::getDir(SZ_SECT_LYR_DISPLAY, "BgPicFolder", "").c_str());
        if (dlg.doBrowse(m_pSkin)) {
            CMPlayerSettings::setSettings(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "UseBgImage", true);
            CMPlayerSettings::setSettings(ET_LYRICS_DISPLAY_SETTINGS, SZ_SECT_LYR_DISPLAY, "BgPicFolder", dlg.getFolder());
            CMLProfile::writeDir(SZ_SECT_LYR_DISPLAY, "BgPicFolder", dlg.getFolder());
        }
    }

protected:
    bool                        m_bIntialized;

};

UIOBJECT_CLASS_NAME_IMP(CPagePfLyricsBgPic, "PreferPage.LyrBgPic")

//////////////////////////////////////////////////////////////////////////

class CPagePfDisplayStyle : public CPagePfBase, public IPopupSkinWndNotify {
    UIOBJECT_CLASS_NAME_DECLARE(CPagePfBase)
protected:
    enum COLOR_SET_TYPE {
        CS_OUTLINED_HIGH_BORDER,
        CS_OUTLINED_LOW_BORDER,
        CS_BG,
        CS_SIZE,
    };

    struct ColorOption {
        ColorOption() {
            pButton = nullptr;
            nButtonID = 0;
            clrDefault = 0;
        }

        CSkinImageButton            *pButton;
        CColor                      clr;
        COLORREF                    clrDefault;
        int                         nButtonID;
        string                      strProfileKey;
    };

    ColorOption                 m_ColorSet[CS_SIZE];

public:
    virtual void popupSkinWndOnSelected() override {
        createLOBXlightBmp(true);
        createLOBXlightBmp(false);
        invalidateUIObject(CID_DS_CLR_HIGH);
        invalidateUIObject(CID_DS_CLR_LOW);
    }

public:
    CPagePfDisplayStyle(PreferPageID pfPageId, cstr_t szAssociateTabButtonId) : CPagePfBase(pfPageId, szAssociateTabButtonId) {
        CID_DS_STYLE = 0;
        CID_DS_FADEOUT = 0;
        CID_DS_ALIGN = 0;
        CID_DS_LOAD_LATIN_FONT = 0;
        CID_DS_LOAD_OTHER_FONT = 0;
        CID_DS_CLR_HIGH = 0;
        CID_DS_CLR_LOW = 0;
        m_eventType = ET_LYRICS_DISPLAY_SETTINGS;
    }

    void onInitialUpdate() override {
        CPagePfBase::onInitialUpdate();

        GET_ID_BY_NAME(CID_DS_STYLE);
        GET_ID_BY_NAME(CID_DS_FADEOUT);
        GET_ID_BY_NAME(CID_DS_ALIGN);
        GET_ID_BY_NAME(CID_DS_LOAD_LATIN_FONT);
        GET_ID_BY_NAME(CID_DS_LOAD_OTHER_FONT);
        GET_ID_BY_NAME(CID_DS_CLR_HIGH);
        GET_ID_BY_NAME(CID_DS_CLR_LOW);

        if (m_pfPageId == PAGE_LYR_DISPLAY) {
            setSettingType(ST_NORMAL_LYR);
            m_eventType = ET_LYRICS_DISPLAY_SETTINGS;
        } else {
            setSettingType(ST_FLOATING_LYR);
            m_eventType = ET_LYRICS_FLOATING_SETTINGS;
        }
    }

    bool onCommand(uint32_t nId) override {
        if (nId == CID_DS_LOAD_LATIN_FONT || nId == CID_DS_LOAD_OTHER_FONT) {
            bool bLatin9 = (nId == CID_DS_LOAD_LATIN_FONT);

            // get default settings in profile
            FontInfoEx font;
            profileGetLyricsFont(m_strSectName.c_str(), font);

            CDlgChooseFont chooser;

            int ret = chooser.doModal(m_pSkin, bLatin9 ? font.nameLatin9.c_str() : font.nameOthers.c_str(),
                font.height, font.weight, font.italic != 0);
            if (ret == IDOK) {
                font.height = chooser.getSize();
                if (font.height < 0) {
                    font.height = -font.height;
                }

                if (bLatin9) {
                    font.nameLatin9 = chooser.getFaceName();
                } else {
                    font.nameOthers = chooser.getFaceName();
                }

                // now save to profile
                profileWriteLyricsFont(m_eventType, m_strSectName.c_str(), font);
            }
        } else if (nId == CID_DS_CLR_HIGH || nId == CID_DS_CLR_LOW) {
            CRect rc;
            getUIObjectRect(nId, rc);

            m_pSkin->clientToScreen(rc);

            // popup list window
            m_popupHighClrListWnd.create(m_pSkin, rc,
                nId == CID_DS_CLR_HIGH, m_eventType == ET_LYRICS_FLOATING_SETTINGS, this);
        } else {
            //
            // lyrics display Color settings
            //
            for (int i = 0; i < CountOf(m_ColorSet); i++) {
                if (nId == m_ColorSet[i].nButtonID) {
                    if (m_ColorSet[i].pButton) {
                        m_ColorSet[i].clr = m_ColorSet[i].pButton->getColor();

                        string strColor = colorToStr(m_ColorSet[i].clr);
                        CMPlayerSettings::setSettings(m_eventType, m_strSectName.c_str(), m_ColorSet[i].strProfileKey.c_str(), strColor.c_str());
                    }
                    return true;
                }
            }

            return CPagePfBase::onCommand(nId);
        }

        return true;
    }

protected:
    enum SettingType {
        ST_NORMAL_LYR               = 0,
        ST_FLOATING_LYR             = 1,
    };

    void setSettingType(SettingType settingType) {
        clearOptions();

        MPlayerApp::getInstance()->getCurLyrDisplaySettingName(settingType == ST_FLOATING_LYR, m_strSectName, m_eventType);

        {
            // Lyrics border colors and background
            m_ColorSet[CS_OUTLINED_HIGH_BORDER].strProfileKey = "HilightBorderColor";
            m_ColorSet[CS_OUTLINED_LOW_BORDER].strProfileKey = "LowlightBorderColor";
            m_ColorSet[CS_BG].strProfileKey = "BgColor";

            m_ColorSet[CS_OUTLINED_HIGH_BORDER].clrDefault = RGB(255, 0, 0);
            m_ColorSet[CS_OUTLINED_LOW_BORDER].clrDefault = RGB(0, 0, 255);
            m_ColorSet[CS_BG].clrDefault = RGB(79, 88, 110);

            if (m_eventType == ET_LYRICS_FLOATING_SETTINGS) {
                m_ColorSet[CS_OUTLINED_HIGH_BORDER].clrDefault = RGB(0, 0, 255);
                m_ColorSet[CS_OUTLINED_LOW_BORDER].clrDefault = RGB(128, 128, 255);
            }

            m_ColorSet[CS_OUTLINED_HIGH_BORDER].nButtonID = getIDByName("CID_DS_CLR_HIGH_BORDER");
            m_ColorSet[CS_OUTLINED_LOW_BORDER].nButtonID = getIDByName("CID_DS_CLR_LOW_BORDER");
            m_ColorSet[CS_BG].nButtonID = getIDByName("CID_DS_CLR_BG");

            for (int i = 0; i < CountOf(m_ColorSet); i++) {
                m_ColorSet[i].pButton = (CSkinImageButton*)getUIObjectById(m_ColorSet[i].nButtonID, CSkinImageButton::className());
                if (m_ColorSet[i].pButton) {
                    m_ColorSet[i].clr.set(m_ColorSet[i].clrDefault);

                    profileGetColorValue(m_ColorSet[i].clr,
                        m_strSectName.c_str(), m_ColorSet[i].strProfileKey.c_str());

                    m_ColorSet[i].pButton->setColor(m_ColorSet[i].clr, true);
                }
            }
        }

        CSkinComboBox *m_pcbStyle;
        m_pcbStyle = (CSkinComboBox *)getUIObjectById(CID_DS_STYLE, CSkinComboBox::className());
        if (m_pcbStyle) {
            //
            // add lyrics display style options
            //
            cstr_t        vszItemName[] = { _TLM("Vertical scrolling"), _TLM("Double lines"), _TLM("Single line"),
                _TLM("Static text"), _TLM("Movie subtitle") };

                cstr_t        vszItemValues[] = { CLyricShowMultiRowObj::className(), CLyricShowTwoRowObj::className(),
                CLyricShowSingleRowObj::className(),
                SZ_TXT_LYR_CONTAINER, CLyricShowVobSub::className() };

                OptComboStr        opt;
                opt.nIDWidgetCombo = CID_DS_STYLE;
                opt.set(ET_UI_SETTINGS_CHANGED,
                m_strSectName.c_str(),
                "LyrDisplayStyle",
                CLyricShowMultiRowObj::className());

                m_pcbStyle->resetContent();
                assert(CountOf(vszItemName) == CountOf(vszItemValues));
                for (int i = 0; i < CountOf(vszItemValues); i++) {
                m_pcbStyle->insertString(i, _TL(vszItemName[i]));
                opt.addItemValue(vszItemValues[i]);
            }
            addOptComboStr(opt);
        }

        CSkinComboBox *m_pcbAlign;
        m_pcbAlign = (CSkinComboBox *)getUIObjectById(CID_DS_ALIGN, CSkinComboBox::className());
        if (m_pcbAlign) {
            //
            // lyrics align
            //
            cstr_t vszItemName[] = { _TLM("Left"), _TLM("Center"), _TLM("Right") };
            cstr_t vszItemValues[] = { "left", "center", "right" };

            OptComboStr opt;
            opt.nIDWidgetCombo = CID_DS_ALIGN;
            opt.set(m_eventType, m_strSectName.c_str(), "LyrAlign", "center");

            assert(CountOf(vszItemName) == CountOf(vszItemValues));
            m_pcbAlign->resetContent();
            for (int i = 0; i < CountOf(vszItemValues); i++) {
                m_pcbAlign->insertString(i, _TL(vszItemName[i]));
                opt.addItemValue(vszItemValues[i]);
            }
            addOptComboStr(opt);
        }

        CSkinComboBox *m_pcbFadeOut;
        m_pcbFadeOut = (CSkinComboBox *)getUIObjectById(CID_DS_FADEOUT, CSkinComboBox::className());
        if (m_pcbFadeOut) {
            // lyrics Fade out options
            cstr_t        vszItemName[] = { _TLM("Automatic"), _TLM("Normal"), _TLM("Fade in lyrics"),
                _TLM("Fade out background") };

                OptComboStr        opt;
                opt.nIDWidgetCombo = CID_DS_FADEOUT;
                opt.set(m_eventType, m_strSectName.c_str(), "LyrDrawOpt", "fadein");

                assert(CountOf(vszItemName) == DO_MAX);
                m_pcbFadeOut->resetContent();
                for (int i = 0; i < CountOf(vszItemName); i++) {
                m_pcbFadeOut->insertString(i, _TL(vszItemName[i]));
                opt.addItemValue(g_idsLyrDisplayOpt[i].szId);
            }
            addOptComboStr(opt);
        }

        // karaoke option
        addOptBool(m_eventType, m_strSectName.c_str(), "Karaoke", false, "CID_C_KARAOKE");

        addOptBool(m_eventType, m_strSectName.c_str(), "OutlineLyrText", false, "CID_C_DRAW_SHADOW");

        initCheckButtons();

        createLOBXlightBmp(true);
        createLOBXlightBmp(false);

        m_pSkin->invalidateRect();
    }

    void createLOBXlightBmp(bool bHilight) {
        CRect rc;
        CLyricShowObj::TextOverlayBlending tob;

        if (!getUIObjectRect(getIDByName("CID_DS_CLR_HIGH"), rc)) {
            return;
        }

        loadLyrOverlayBlendingSettings(m_strSectName.c_str(), tob, rc.height(), bHilight);

        if (tob.obm == OBM_COLOR) {
            RawImageDataPtr image = createRawImageData(rc.width(), rc.height(), 24);
            rawImageSet(image.get(), tob.clr[CLyricShowObj::TCI_FILL].r(),
                tob.clr[CLyricShowObj::TCI_FILL].g(),
                tob.clr[CLyricShowObj::TCI_FILL].b(),
                tob.clr[CLyricShowObj::TCI_FILL].getAlpha());
            tob.imgPattern = image;
        }

        if (tob.imgPattern) {
            CSkinImageButton *pButton;
            pButton = (CSkinImageButton*)getUIObjectById(
                bHilight ? CID_DS_CLR_HIGH : CID_DS_CLR_LOW, CSkinImageButton::className());

            if (pButton) {
                pButton->setContentImage(tob.imgPattern);
            }
        }
    }

protected:
    EventType                   m_eventType;
    string                      m_strSectName;

    int                         CID_DS_STYLE, CID_DS_FADEOUT, CID_DS_ALIGN;
    int                         CID_DS_LOAD_LATIN_FONT, CID_DS_LOAD_OTHER_FONT, CID_DS_CLR_HIGH, CID_DS_CLR_LOW;

    CLyrDisplayClrListWnd       m_popupHighClrListWnd;
    CLyrDisplayClrListWnd       m_popupLowClrListWnd;

};

UIOBJECT_CLASS_NAME_IMP(CPagePfDisplayStyle, "PreferPage.LyrDisplayStyle")

//////////////////////////////////////////////////////////////////////////

class CPagePfTheme : public CPagePfBase {
    UIOBJECT_CLASS_NAME_DECLARE(CPagePfBase)
public:
    CPagePfTheme() : CPagePfBase(PAGE_THEMES, "ID_THEME_THEME") {
        m_nThemeListID = 0;
    }

    virtual void onUIObjNotify(IUIObjNotify *pNotify) override {
        if (pNotify->nID == m_nThemeListID && pNotify->isKindOf(CSkinListCtrl::className())) {
            CSkinListCtrlEventNotify *pListCtrlNotify = (CSkinListCtrlEventNotify*)pNotify;
            if (pListCtrlNotify->cmd == CSkinListCtrlEventNotify::C_SEL_CHANGED) {
                string strTheme;
                int nSel = m_pThemeList->getNextSelectedItem();
                if (nSel != -1) {
                    m_pThemeList->getItemText(nSel, 0, strTheme);
                }
                setUIObjectText("CID_E_THEMENAME", strTheme.c_str(), true);
            }
        }
    }

    void onInitialUpdate() override {
        CPagePfBase::onInitialUpdate();

        m_nThemeListID = getIDByName("CID_THME_LIST");
        m_pSkin->registerUIObjNotifyHandler(m_nThemeListID, this);

        m_pThemeList = (CSkinListCtrl *)getUIObjectById(m_nThemeListID, CSkinListCtrl::className());
        assert(m_pThemeList);

        if (m_pThemeList) {
            m_pThemeList->addColumn("Theme", 300);
        }

        updateThemesList();
    }

    void onDestroy() override {
        m_pSkin->unregisterUIObjNotifyHandler(this);

        CPagePfBase::onDestroy();
    }

    bool onCommand(uint32_t nId) override {
        if (nId == getIDByName("CID_LOAD_THEME")) {
            loadTheme();
        } else if (nId == getIDByName("CID_SAVE_THEME")) {
            saveTheme();
        } else if (nId == getIDByName("CID_DELETE_THEME")) {
            deleteTheme();
        } else {
            return CPagePfBase::onCommand(nId);
        }

        return true;
    }

protected:
    void loadTheme() {
        // load theme
        int nSel;

        nSel = m_pThemeList->getNextSelectedItem(-1);
        if (nSel == -1) {
            m_pSkin->messageOut(_TLT("Please select a theme from the list."));
            return;
        }

        CThemesFile file;
        string strSelTheme;

        m_pThemeList->getItemText(nSel, 0, strSelTheme);
        file.open();

        g_profile.writeString("CurTheme", strSelTheme.c_str());
        file.setCurTheme(strSelTheme.c_str());
    }

    void saveTheme() {
        // save theme
        CThemesFile file;

        string strThemeName = getUIObjectText("CID_E_THEMENAME");
        if (strThemeName.empty()) {
            m_pSkin->messageOut(_TLT("Please enter the theme name."));
            return;
        }

        file.open();

        file.useCurSettingAsTheme(strThemeName.c_str());

        file.save();

        updateThemesList();
    }

    void deleteTheme() {
        int nSel;

        nSel = m_pThemeList->getNextSelectedItem(-1);
        if (nSel == -1) {
            m_pSkin->messageOut(_TLT("Please select a theme from the list."));
            return;
        }

        CThemesFile file;
        string strSelTheme;

        m_pThemeList->getItemText(nSel, 0, strSelTheme);
        file.open();

        file.remove(strSelTheme.c_str());
        file.save();
        updateThemesList();

        setUIObjectText("CID_E_THEMENAME", "", true);
    }

    void updateThemesList() {
        if (!m_pThemeList) {
            return;
        }

        CThemesFile file;
        VecStrings vThemes;
        string strCurTheme;
        int nSel = -1;

        file.open();
        file.enumAllThemes(vThemes);

        m_pThemeList->deleteAllItems();

        strCurTheme = g_profile.getString("CurTheme", "");

        for (int i = 0; i < (int)vThemes.size(); i++) {
            m_pThemeList->insertItem(i, vThemes[i].c_str());
            if (nSel == -1 && strcmp(strCurTheme.c_str(), vThemes[i].c_str())) {
                nSel = i;
            }
        }

        if (nSel != -1) {
            m_pThemeList->setItemSelectionState(nSel, true);
        }
    }

protected:
    CSkinListCtrl               *m_pThemeList;
    int                         m_nThemeListID;

};

UIOBJECT_CLASS_NAME_IMP(CPagePfTheme, "PreferPage.Theme")

//////////////////////////////////////////////////////////////////////////

UIOBJECT_CLASS_NAME_IMP(CPagePfThemeRoot, "PreferPage.ThemeRoot")

CPagePfThemeRoot::CPagePfThemeRoot() : CPagePfBase(PAGE_UNKNOWN, "ID_ROOT_THEME") {
}

void CPagePfThemeRoot::onInitialUpdate() {
    CPagePfBase::onInitialUpdate();

    setDefaultPreferPage(this, (PreferPageID)getExPoolInt(SZ_EX_POOL_PF_DEFAULT_PAGE));

    checkToolbarDefaultPage("CID_TOOLBAR_THEME");
}

class PagePfDisplayStyleNewer : public IUIObjNewer {
public:
    PagePfDisplayStyleNewer(PreferPageID pfPageId, cstr_t szAssociateTabButtonId) : pfPageId(pfPageId), szAssociateTabButtonId(szAssociateTabButtonId) { }
    virtual CUIObject *New() { return new CPagePfDisplayStyle(pfPageId, szAssociateTabButtonId); }

    PreferPageID                    pfPageId;
    cstr_t                          szAssociateTabButtonId;

};

void registerPfThemePages(CSkinFactory *pSkinFactory) {
    AddUIObjNewer2(pSkinFactory, CPagePfThemeRoot);
    AddUIObjNewer2(pSkinFactory, CPagePfTheme);
    pSkinFactory->registerUIObjNewer("PreferPage.LyrDisplayStyle", new PagePfDisplayStyleNewer(PAGE_LYR_DISPLAY, "ID_THEME_LYR_DISPLAY"));
    pSkinFactory->registerUIObjNewer("PreferPage.FloatLyrDisplayStyle", new PagePfDisplayStyleNewer(PAGE_FLOAT_LYR_DISPLAY, "ID_THEME_FLOAT_LYR_DISPLAY"));
    AddUIObjNewer2(pSkinFactory, CPagePfLyricsBgPic);
}
