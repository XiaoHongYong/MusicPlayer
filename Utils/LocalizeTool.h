#pragma once

bool getUserDefaultLang(string &strLang, string &strLangFull);

class CLanguageFile {
public:
    bool init(cstr_t szProfile);
    void close();
    cstr_t getTranslation(cstr_t szString);

protected:
    MapStrings                  m_mapString;

};

class CLanguageTool {
public:
    CLanguageTool();
    virtual ~CLanguageTool();

public:
    struct TransFileInfo {
        string                      strFileName;
        string                      strLanguage;
        string                      strLanguageCode;
        string                      strLanguageCodeFull;
    };

    typedef vector<TransFileInfo>        V_TRANSFILEINFO;

public:
    void close();

    cstr_t toLocalString(cstr_t szString);

    void onLanguageChanged();

    bool isEnglish();

    static bool getCurrentLanguageFile(string &strLanguageFile);

    static bool listTransFiles(cstr_t szDir, V_TRANSFILEINFO &vTransFiles);

    // Replace $ProductName$, $CompanyName$ to actual value.
    void setMacro(cstr_t szName, cstr_t szValue);

    bool hasMacro() const { return !m_mapMacros.empty(); }

protected:
    cstr_t replaceMacroStr(cstr_t szString);

protected:
    bool                        m_bInitialized;
    bool                        m_bEnglish;

    CLanguageFile               m_LanguageFile;

    enum { MAX_CACHED_STR = 5 };
    string                      m_vStrCached[MAX_CACHED_STR];
    int                         m_nCurStr;

    typedef map<string, string>    map_string;

    map_string                  m_mapMacros;

};

extern CLanguageTool g_LangTool;


// TL = To Local
// TLT = Tolocal
// TLM = To Local Mark: Mark it need to local
// TL3 = To Local level 3
#define _TL(szString)       (g_LangTool.toLocalString(szString))
#define _TLT(szString)      (g_LangTool.toLocalString(szString))
#define _TLM(szString)      szString

#define ERROR2STR_LOCAL(nError) (g_LangTool.toLocalString(Error2Str(nError)))

string removePrefixOfAcckey(cstr_t szStr);
