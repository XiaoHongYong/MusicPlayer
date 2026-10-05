#pragma once

#define SZ_EX_POOL_PF_DEFAULT_PAGE  "DefaultPage"

enum PreferPageID {
    PAGE_UNKNOWN,

    // User interface
    PAGE_UI,
    PAGE_SKINS,

    // Themes
    PAGE_THEMES,
    PAGE_LYR_DISPLAY,
    PAGE_FLOAT_LYR_DISPLAY,
    PAGE_LYR_BG,

    // System
    PAGE_INET,
    PAGE_ASSOCIATION,
    PAGE_STARTUP,

    // Lyrics
    PAGE_LYR_DOWNLOAD,
    PAGE_LYR_SAVE_OPTIONS,
    PAGE_LYR_OUTPUT_PLUGINS,

    // Album Art
    PAGE_ALBUM_ART,

    PAGE_ADVANCED,
};

class CPagePfBase : public CSkinContainer, public IUIObjNotifyHandler {
    UIOBJECT_CLASS_NAME_DECLARE(CSkinContainer)
public:
    struct OptBase {
        EventType                   eventType;
        string                      strSection;
        string                      strSettingName;
        string                      strDefaultValue;
        int                         nDefaultValue;

        void set(EventType evtType, cstr_t szSection, cstr_t szSettingName, cstr_t szDefValue) {
            eventType = evtType;
            strSection = szSection;
            strSettingName = szSettingName;
            strDefaultValue = szDefValue;
        }

        void set(EventType evtType, cstr_t szSection, cstr_t szSettingName, int nDefValue) {
            eventType = evtType;
            strSection = szSection;
            strSettingName = szSettingName;
            nDefaultValue = nDefValue;
        }
    };
    struct OptBool : public OptBase {
        int                         nCtrl;
    };
    typedef    list<OptBool>            VOptBool;

    struct OptComboStr: public OptBase {
        int                         nIDWidgetCombo;
        vector<string>              vValues;

        void addItemValue(cstr_t szValue) {
            vValues.push_back(szValue);
        }
    };
    typedef    list<OptComboStr>        VOptComboStr;

    struct OptRadioInt : public OptBase {
        vector<int>                 vCtrls;
        vector<int>                 vValues;

        void addCtrlValue(int nCtrl, int value) {
            vCtrls.push_back(nCtrl);
            vValues.push_back(value);
        }
    };
    typedef    list<OptRadioInt>        VOptRadioInt;

public:
    CPagePfBase(PreferPageID pfPageId, cstr_t szAssociateTabButtonId);

    void onInitialUpdate() override;

    void onDestroy() override;

    bool onCommand(uint32_t nId) override;

    void addOptBool(EventType evtType, cstr_t szSection, cstr_t szSettingName, bool bDefValue, cstr_t szCtrlId);
    void addOptComboStr(OptComboStr &optComboStr) { m_vOptComboStr.push_back(optComboStr); }
    void addOptRadioInt(OptRadioInt &optRadioInt) { m_vOptRadioInt.push_back(optRadioInt); }

    void initCheckButtons();

    void onUIObjNotify(IUIObjNotify *pNotify) override;

    PreferPageID getPfPageID() const { return m_pfPageId; }
    int getAssociateTabButtonId() const { return m_nAssociateTabButtonId; }
    bool setDefaultPreferPage(CSkinContainer *pContainer, PreferPageID pfPageIdDefault);

    void checkToolbarDefaultPage(cstr_t szToolbarId);

protected:
    void clearOptions();
    bool onSwitchToPageCmd(int nId);

protected:
    VOptBool                    m_vOptBool;
    VOptComboStr                m_vOptComboStr;
    VOptRadioInt                m_vOptRadioInt;
    string                      m_strPageId;
    PreferPageID                m_pfPageId;
    int                         m_nAssociateTabButtonId;

    int                         m_nDefaultPageAssociateTabButtonId;

};
