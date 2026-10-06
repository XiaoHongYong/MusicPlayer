#include "Utils.h"
#include "LocalizeTool.h"
#include "App.h"

#ifdef _MAC_OS
#include <CoreFoundation/CoreFoundation.h>
#endif
#ifndef _WIN32
#include <cstdlib>
#endif


CLanguageTool g_LangTool;

#define SZ_LANG_DIR         "lang"

string getLangPackDir() {
    return getAppResourceFile(SZ_LANG_DIR);
}

static bool startsWithI(cstr_t text, cstr_t prefix) {
    return strncasecmp(text, prefix, strlen(prefix)) == 0;
}

// zh-Hans-CN / zh_CN.UTF-8 / zh-CN → zh-CN；英语返回空（用内置英文）
static string localeIdFromLangTag(string tag) {
    for (char &ch : tag) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = (char)(ch + ('a' - 'A'));
        } else if (ch == '_') {
            ch = '-';
        }
    }
    size_t cut = tag.find_first_of(".@");
    if (cut != string::npos) {
        tag.resize(cut);
    }
    if (tag.empty() || tag == "c" || tag == "posix") {
        return "";
    }
    if (startsWithI(tag.c_str(), "en")) {
        return "";
    }
    if (startsWithI(tag.c_str(), "zh-hans") || startsWithI(tag.c_str(), "zh-cn") || tag == "zh") {
        return "zh-CN";
    }
    if (startsWithI(tag.c_str(), "zh-hant") || startsWithI(tag.c_str(), "zh-tw")
        || startsWithI(tag.c_str(), "zh-hk") || startsWithI(tag.c_str(), "zh-mo")) {
        return "zh-TW";
    }
    return "";
}

#ifndef _WIN32
static bool readSystemLangTag(string &tag) {
#ifdef _MAC_OS
    CFArrayRef langs = CFLocaleCopyPreferredLanguages();
    if (!langs) {
        return false;
    }
    bool ok = false;
    if (CFArrayGetCount(langs) > 0) {
        CFStringRef code = (CFStringRef)CFArrayGetValueAtIndex(langs, 0);
        char buf[128] = {0};
        if (code && CFStringGetCString(code, buf, sizeof(buf), kCFStringEncodingUTF8) && buf[0]) {
            tag = buf;
            ok = true;
        }
    }
    CFRelease(langs);
    return ok;
#else
    const char *env = getenv("LC_ALL");
    if (!env || !env[0] || strcmp(env, "C") == 0) {
        env = getenv("LC_MESSAGES");
    }
    if (!env || !env[0] || strcmp(env, "C") == 0) {
        env = getenv("LANG");
    }
    if (!env || !env[0]) {
        return false;
    }
    tag = env;
    return true;
#endif
}
#endif

static string detectSystemLocaleId() {
#ifdef _WIN32
    string lang, full;
    if (!getUserDefaultLang(lang, full)) {
        return "";
    }
    if (strcasecmp(lang.c_str(), "English") == 0) {
        return "";
    }
    if (strcasecmp(lang.c_str(), "Simplified Chinese") == 0
        || startsWithI(full.c_str(), "Chinese (PRC)")
        || startsWithI(full.c_str(), "Chinese (Singapore)")) {
        return "zh-CN";
    }
    if (strcasecmp(lang.c_str(), "Traditional Chinese") == 0
        || startsWithI(full.c_str(), "Chinese (Taiwan)")
        || startsWithI(full.c_str(), "Chinese (Hong Kong")
        || startsWithI(full.c_str(), "Chinese (Macau")) {
        return "zh-TW";
    }
    return "";
#else
    string tag;
    if (!readSystemLangTag(tag)) {
        return "";
    }
    return localeIdFromLangTag(tag);
#endif
}


// remove '&'
string removePrefixOfAcckey(cstr_t szStr) {
    cstr_t p;

    p = strchr(szStr, '&');
    if (!p) {
        return szStr;
    }

    string str;

    // remove "(&A)"
    if (p > szStr && *(p - 1) == '(' && p[1] != '\0' && p[2] == ')' && p[3] == '\0') {
        str.append(szStr, (int)(p - szStr) - 1);
    } else {
        str.append(szStr, (int)(p - szStr));
        str.append(p + 1);
    }

    return str;
}

static bool findStringValue(cstr_t szBuff, cstr_t szName, string &strValue) {
    cstr_t szBeg, szEnd;

    strValue.resize(0);

    szBeg = strstr(szBuff, szName);
    if (!szBeg) {
        return false;
    }

    szBeg += strlen(szName);
    szEnd = szBeg;
    while (*szEnd && *szEnd != '\r' && *szEnd != '\n') {
        szEnd++;
    }

    strValue.append(szBeg, szEnd);

    return true;
}

//
// getPrivateProfileStringUtf8 don't support UTF-8 encoding file.
// Use this function instead.
//
static bool getLanguagePackInfo(cstr_t szFile, string &strLanguage, string &strLangCode, string &strLangCodeFull) {
    char buff[1024];
    FILE *fp;
    CharEncodingType encoding;
    string strBuff;
    cstr_t SZ_INFO = "[Info]";

    fp = fopenUtf8(szFile, "rb");
    if (!fp) {
        return false;
    }

    memset(buff, 0, sizeof(buff));
    size_t nRead = fread(buff, 1, sizeof(buff) - 1, fp);
    if (nRead < 8) {
        goto R_FAILED;
    }

    int bomSize;
    encoding = detectFileEncoding(buff, nRead, bomSize);
    assert(encoding == ED_UTF8 || encoding == ED_SYSDEF);
    if (bomSize < 0 || (size_t)bomSize >= nRead) {
        goto R_FAILED;
    }
    strBuff.assign(buff + bomSize, nRead - (size_t)bomSize);

    // [Info]
    if (strncasecmp(strBuff.c_str(), SZ_INFO, strlen(SZ_INFO)) != 0) {
        goto R_FAILED;
    }

    if (!findStringValue(strBuff.c_str(), "Language=", strLanguage)) {
        goto R_FAILED;
    }

    if (!findStringValue(strBuff.c_str(), "LanguageCode=", strLangCode)) {
        goto R_FAILED;
    }

    if (!findStringValue(strBuff.c_str(), "LanguageCodeFull=", strLangCodeFull)) {
        goto R_FAILED;
    }

    fclose(fp);
    return true;

R_FAILED:
    fclose(fp);
    return false;
}

bool getUserDefaultLang(string &strLang, string &strLangFull) {
#ifdef _WIN32
    struct LangCode {
        int                         nLangId;
        const char                  *szLangShort;
        const char                  *szLangFull;
    };

    static LangCode gLangCode[] = {
        //    { 0x0000, "", "Language Neutral" },
        //    { 0x007f, "", "The language for the invariant locale (LOCALE_INVARIANT). See MAKELCID." },
        //    { 0x0400, "", "process or User Default Language" },
        //    { 0x0800, "", "System Default Language" },
        { 0x0436, "Afrikaans", "Afrikaans" },
        { 0x041c, "Albanian", "Albanian" },
        { 0x0401, "Arabic", "Arabic (Saudi Arabia)" },
        { 0x0801, "Arabic", "Arabic (Iraq)" },
        { 0x0c01, "Arabic", "Arabic (Egypt)" },
        { 0x1001, "Arabic", "Arabic (Libya)" },
        { 0x1401, "Arabic", "Arabic (Algeria)" },
        { 0x1801, "Arabic", "Arabic (Morocco)" },
        { 0x1c01, "Arabic", "Arabic (Tunisia)" },
        { 0x2001, "Arabic", "Arabic (Oman)" },
        { 0x2401, "Arabic", "Arabic (Yemen)" },
        { 0x2801, "Arabic", "Arabic (Syria)" },
        { 0x2c01, "Arabic", "Arabic (Jordan)" },
        { 0x3001, "Arabic", "Arabic (Lebanon)" },
        { 0x3401, "Arabic", "Arabic (Kuwait)" },
        { 0x3801, "Arabic", "Arabic (U.A.E.)" },
        { 0x3c01, "Arabic", "Arabic (Bahrain)" },
        { 0x4001, "Arabic", "Arabic (Qatar)" },
        { 0x042b, "Armenian", "Armenian" },
        { 0x042c, "Azeri", "Azeri (Latin)" },
        { 0x082c, "Azeri", "Azeri (Cyrillic)" },
        { 0x042d, "Basque", "Basque" },
        { 0x0423, "Belarusian", "Belarusian" },
        { 0x0402, "Bulgarian", "Bulgarian" },
        { 0x0455, "Burmese", "Burmese" },
        { 0x0403, "Catalan", "Catalan" },
        { 0x0404, "Traditional Chinese", "Chinese (Taiwan)" },
        { 0x0804, "Simplified Chinese", "Chinese (PRC)" },
        { 0x0c04, "Chinese", "Chinese (Hong Kong SAR, PRC)" },
        { 0x1004, "Simplified Chinese", "Chinese (Singapore)" },
        { 0x1404, "Traditional Chinese", "Chinese (Macau SAR)" },
        { 0x041a, "Croatian", "Croatian" },
        { 0x0405, "Czech", "Czech" },
        { 0x0406, "Danish", "Danish" },
        { 0x0465, "Divehi", "Divehi" },
        { 0x0413, "Dutch", "Dutch (Netherlands)" },
        { 0x0813, "Dutch", "Dutch (Belgium)" },
        { 0x0409, "English", "English (United States)" },
        { 0x0809, "English", "English (United Kingdom)" },
        { 0x0c09, "English", "English (Australian)" },
        { 0x1009, "English", "English (Canadian)" },
        { 0x1409, "English", "English (New Zealand)" },
        { 0x1809, "English", "English (Ireland)" },
        { 0x1c09, "English", "English (South Africa)" },
        { 0x2009, "English", "English (Jamaica)" },
        { 0x2409, "English", "English (Caribbean)" },
        { 0x2809, "English", "English (Belize)" },
        { 0x2c09, "English", "English (Trinidad)" },
        { 0x3009, "English", "English (Zimbabwe)" },
        { 0x3409, "English", "English (Philippines)" },
        { 0x0425, "Estonian", "Estonian" },
        { 0x0438, "Faeroese", "Faeroese" },
        { 0x0429, "Farsi", "Farsi" },
        { 0x040b, "Finnish", "Finnish" },
        { 0x040c, "French", "French (Standard)" },
        { 0x080c, "French", "French (Belgian)" },
        { 0x0c0c, "French", "French (Canadian)" },
        { 0x100c, "French", "French (Switzerland)" },
        { 0x140c, "French", "French (Luxembourg)" },
        { 0x180c, "French", "French (Monaco)" },
        { 0x0456, "Galician", "Galician" },
        { 0x0437, "Georgian", "Georgian" },
        { 0x0407, "German", "German (Standard)" },
        { 0x0807, "German", "German (Switzerland)" },
        { 0x0c07, "German", "German (Austria)" },
        { 0x1007, "German", "German (Luxembourg)" },
        { 0x1407, "German", "German (Liechtenstein)" },
        { 0x0408, "Greek", "Greek" },
        { 0x0447, "Gujarati", "Gujarati" },
        { 0x040d, "Hebrew", "Hebrew" },
        { 0x0439, "Hindi", "Hindi" },
        { 0x040e, "Hungarian", "Hungarian" },
        { 0x040f, "Icelandic", "Icelandic" },
        { 0x0421, "Indonesian", "Indonesian" },
        { 0x0410, "Italian", "Italian (Standard)" },
        { 0x0810, "Italian", "Italian (Switzerland)" },
        { 0x0411, "Japanese", "Japanese" },
        { 0x044b, "Kannada", "Kannada" },
        { 0x0457, "Konkani", "Konkani" },
        { 0x0412, "Korean", "Korean" },
        { 0x0812, "Korean", "Korean (Johab)" },
        { 0x0440, "Kyrgyz", "Kyrgyz" },
        { 0x0426, "Latvian", "Latvian" },
        { 0x0427, "Lithuanian", "Lithuanian" },
        { 0x0827, "Lithuanian", "Lithuanian (Classic)" },
        { 0x042f, "FYRO Macedonian", "FYRO Macedonian" },
        { 0x043e, "Malay", "Malay (Malaysian)" },
        { 0x083e, "Malay", "Malay (Brunei Darussalam)" },
        { 0x044e, "Marathi", "Marathi" },
        { 0x0450, "Mongolian", "Mongolian" },
        { 0x0414, "Norwegian", "Norwegian (Bokmal)" },
        { 0x0814, "Norwegian", "Norwegian (Nynorsk)" },
        { 0x0415, "Polish", "Polish" },
        { 0x0416, "Portuguese", "Portuguese (Brazil)" },
        { 0x0816, "Portuguese", "Portuguese (Portugal)" },
        { 0x0446, "Punjabi", "Punjabi" },
        { 0x0418, "Romanian", "Romanian" },
        { 0x0419, "Russian", "Russian" },
        { 0x044f, "Sanskrit", "Sanskrit" },
        { 0x0c1a, "Serbian", "Serbian (Cyrillic)" },
        { 0x081a, "Serbian", "Serbian (Latin)" },
        { 0x041b, "Slovak", "Slovak" },
        { 0x0424, "Slovenian", "Slovenian" },
        { 0x040a, "Spanish", "Spanish (Spain, Traditional Sort)" },
        { 0x080a, "Spanish", "Spanish (Mexican)" },
        { 0x0c0a, "Spanish", "Spanish (Spain, International Sort)" },
        { 0x100a, "Spanish", "Spanish (Guatemala)" },
        { 0x140a, "Spanish", "Spanish (Costa Rica)" },
        { 0x180a, "Spanish", "Spanish (Panama)" },
        { 0x1c0a, "Spanish", "Spanish (Dominican Republic)" },
        { 0x200a, "Spanish", "Spanish (Venezuela)" },
        { 0x240a, "Spanish", "Spanish (Colombia)" },
        { 0x280a, "Spanish", "Spanish (Peru)" },
        { 0x2c0a, "Spanish", "Spanish (Argentina)" },
        { 0x300a, "Spanish", "Spanish (Ecuador)" },
        { 0x340a, "Spanish", "Spanish (Chile)" },
        { 0x380a, "Spanish", "Spanish (Uruguay)" },
        { 0x3c0a, "Spanish", "Spanish (Paraguay)" },
        { 0x400a, "Spanish", "Spanish (Bolivia)" },
        { 0x440a, "Spanish", "Spanish (El Salvador)" },
        { 0x480a, "Spanish", "Spanish (Honduras)" },
        { 0x4c0a, "Spanish", "Spanish (Nicaragua)" },
        { 0x500a, "Spanish", "Spanish (Puerto Rico)" },
        { 0x0430, "Sutu", "Sutu" },
        { 0x0441, "Swahili", "Swahili (Kenya)" },
        { 0x041d, "Swedish", "Swedish" },
        { 0x081d, "Swedish", "Swedish (Finland)" },
        { 0x045a, "Syriac", "Syriac" },
        { 0x0449, "Tamil", "Tamil" },
        { 0x0444, "Tatar", "Tatar (Tatarstan)" },
        { 0x044a, "Telugu", "Telugu" },
        { 0x041e, "Thai", "Thai" },
        { 0x041f, "Turkish", "Turkish" },
        { 0x0422, "Ukrainian", "Ukrainian" },
        { 0x0420, "Urdu", "Urdu (Pakistan)" },
        { 0x0820, "Urdu", "Urdu (India)" },
        { 0x0443, "Uzbek", "Uzbek (Latin)" },
        { 0x0843, "Uzbek", "Uzbek (Cyrillic)" },
        { 0x042a, "Vietnamese", "Vietnamese" },
        { 0x0, nullptr, nullptr }
    };

    LANGID wLangId = GetUserDefaultLangID();

    for (LangCode *pItem = gLangCode; pItem->szLangFull != nullptr; pItem++) {
        if (pItem->nLangId == wLangId) {
            strLang = pItem->szLangShort;
            strLangFull = pItem->szLangFull;
            return true;
        }
    }
#endif

#ifndef _WIN32
    string tag;
    if (!readSystemLangTag(tag)) {
        return false;
    }
    string localeId = localeIdFromLangTag(tag);
    if (localeId.empty()) {
        strLang = "English";
        strLangFull = "English";
        return true;
    }
    if (localeId == "zh-CN") {
        strLang = "Simplified Chinese";
        strLangFull = "Chinese (PRC)";
        return true;
    }
    if (localeId == "zh-TW") {
        strLang = "Traditional Chinese";
        strLangFull = "Chinese (Taiwan)";
        return true;
    }
#endif

    return false;
}

bool CLanguageFile::init(cstr_t szProfile) {
    //
    // init and load all the strings section
    //
    string str;
    cstr_t szBeg, szEnd;
    string strAppName, strKey, strValue;

    if (!readFileByBom(szProfile, str)) {
        return false;
    }

    //
    // 查找 AppName
    szBeg = str.c_str();
    while (*szBeg) {
        while (*szBeg == ' ' || *szBeg == '\t' || *szBeg == '\n') {
            szBeg++;
        }

        if (*szBeg != '[') {
            while (*szBeg && *szBeg != '\n') {
                szBeg++;
            }
            continue;
        }
        szBeg++;

        while (*szBeg == ' ' || *szBeg == '\t') {
            szBeg++;
        }

        szEnd = szBeg;
        while (*szEnd && *szEnd != ']' && *szEnd != '\n') {
            szEnd++;
        }

        if (*szEnd != ']') {
            while (*szBeg && *szBeg != '\n') {
                szBeg++;
            }
            continue;
        }

        // found appname now
        strAppName.resize(0);
        strAppName.append(szBeg, szEnd);
        trimStr(strAppName);

        szBeg = szEnd + 1;

        // only process "string" section
        if (strcasecmp(strAppName.c_str(), "string") != 0) {
            continue;
        }

        while (*szBeg) {
            // get key and values
            while (*szBeg == ' ' || *szBeg == '\t') {
                szBeg++;
            }

            while (*szBeg == '\r' || *szBeg == '\n') {
                szBeg++;
            }

            if (*szBeg == '[') {
                break;
            }

            szEnd = szBeg;
            while (*szEnd && *szEnd != '=' && *szEnd != '\n') {
                szEnd++;
            }

            if (*szEnd != '=') {
                szBeg = szEnd;
                while (*szBeg && *szBeg != '\n') {
                    szBeg++;
                }
                continue;
            }

            // found key
            strKey.resize(0);
            strKey.append(szBeg, szEnd);

            szBeg = szEnd + 1;
            while (*szEnd && *szEnd != '\n') {
                szEnd++;
            }

            while (*szEnd == '\r' || *szEnd == '\n') {
                szEnd--;
            }
            if (*szEnd) {
                szEnd++;
            }
            strValue.resize(0);
            strValue.append(szBeg, szEnd);

            m_mapString[strKey] = strValue;

            szBeg = szEnd;
        }
    }

    return true;
}

void CLanguageFile::close() {
    m_mapString.clear();
}

cstr_t CLanguageFile::getTranslation(cstr_t szString) {
    MapStrings::iterator it;
    it = m_mapString.find(szString);
    if (it == m_mapString.end()) {
        return szString;
    }

    if ((*it).second.empty()) {
        return szString;
    }

    return (*it).second.c_str();
}


CLanguageTool::CLanguageTool() {
    m_bEnglish = true;
    m_bInitialized = false;
    m_nCurStr = 0;
}

CLanguageTool::~CLanguageTool() {
}

bool CLanguageTool::getCurrentLanguageFile(string &strLanguageFile) {
    strLanguageFile = getLangPackDir();

    string langFileName = g_profile.getString("Language", "");
    if (langFileName.empty()) {
        string strLang, strLangFull;
        getUserDefaultLang(strLang, strLangFull);

        if (strcasecmp(strLang.c_str(), "English") == 0) {
            return false;
        }

        V_TRANSFILEINFO vTransFiles;
        string strFileName, strFileNameBest;
        string localeId = detectSystemLocaleId();

        listTransFiles(getLangPackDir().c_str(), vTransFiles);

        for (int i = 0; i < (int)vTransFiles.size(); i++) {
            TransFileInfo &item = vTransFiles[i];
            string packId = fileGetTitle(item.strFileName.c_str());
            if (!localeId.empty() && strcasecmp(packId.c_str(), localeId.c_str()) == 0) {
                strFileNameBest = item.strFileName;
                break;
            }
            if (!strLangFull.empty() && strcmp(item.strLanguageCodeFull.c_str(), strLangFull.c_str()) == 0) {
                strFileNameBest = item.strFileName;
                break;
            }
            if (strFileName.empty() && !strLang.empty()
                && strcmp(item.strLanguageCode.c_str(), strLang.c_str()) == 0) {
                strFileName = item.strFileName;
            }
        }

        if (strFileNameBest.size()) {
            langFileName = strFileNameBest;
        } else if (strFileName.size()) {
            langFileName = strFileName;
        }
        // 未手动选择时不写入配置，下次启动仍跟随系统语言
    }

    strLanguageFile = dirStringJoin(getLangPackDir().c_str(), langFileName.c_str());
    return (!langFileName.empty() && isFileExist(strLanguageFile.c_str()));
}

bool CLanguageTool::listTransFiles(cstr_t szDir, V_TRANSFILEINFO &vTransFiles) {
    FileFind find;
    string str;
    TransFileInfo item;
    string strFilter, strEngTransFileName;
    string strLangDir;

    if (szDir && !isEmptyString(szDir)) {
        strLangDir = szDir;
    } else {
        strLangDir = getLangPackDir();
    }

    // add english default translation
    item.strFileName = "English";
    item.strLanguage = "English";
    item.strLanguageCode = "English";
    item.strLanguageCodeFull = "English";
    vTransFiles.push_back(item);

    strFilter = "*.ini";
    strEngTransFileName = "English.ini";

    if (!find.openDir(strLangDir.c_str(), strFilter.c_str())) {
        return false;
    }

    while (find.findNext()) {
        if (!find.isCurDir()) {
            TransFileInfo item;

            if (strcasecmp(find.getCurName(), strEngTransFileName.c_str()) == 0) {
                continue;
            }

            str = dirStringJoin(strLangDir.c_str(), find.getCurName());

            if (getLanguagePackInfo(str.c_str(), item.strLanguage,
                item.strLanguageCode, item.strLanguageCodeFull)) {
                item.strFileName = find.getCurName();
                vTransFiles.push_back(item);
            }
        }
    }

    return true;
}

// COMMENT:
//        将字符串转换为本地语言。
// RETURN:
//        return translated string
cstr_t CLanguageTool::toLocalString(cstr_t szString) {
    if (!m_bInitialized) {
        // init select correct language...
        onLanguageChanged();
    }

    szString = replaceMacroStr(szString);

    if (m_bEnglish) {
        return szString;
    }

    return m_LanguageFile.getTranslation(szString);
}

void CLanguageTool::setMacro(cstr_t szName, cstr_t szValue) {
    m_mapMacros[szName] = szValue;
}

cstr_t CLanguageTool::replaceMacroStr(cstr_t szString) {
    cstr_t szPos;

    for (map_string::iterator it = m_mapMacros.begin(); it != m_mapMacros.end(); ++it) {
        szPos = strstr(szString, (*it).first.c_str());
        while (szPos) {
            string &str = m_vStrCached[m_nCurStr];
            str.resize(0);
            str.append(szString, (int)(szPos - szString));
            str += (*it).second;
            str += szPos + (*it).first.size();

            szString = str.c_str();
            m_nCurStr++;
            if (m_nCurStr >= MAX_CACHED_STR) {
                m_nCurStr = 0;
            }

            szPos = strstr(szString, (*it).first.c_str());
        }
    }

    return szString;
}

void CLanguageTool::close() {
    m_LanguageFile.close();
    m_bInitialized = false;
    m_bEnglish = true;
    m_mapMacros.clear();
}

bool CLanguageTool::isEnglish() {
    if (!m_bInitialized) {
        // init select correct language...
        onLanguageChanged();
    }

    return m_bEnglish;
}

void CLanguageTool::onLanguageChanged() {
    m_bInitialized = true;
    m_bEnglish = true;

    string strLangFile;
    if (getCurrentLanguageFile(strLangFile)) {
        m_bEnglish = false;
        m_LanguageFile.init(strLangFile.c_str());
    }
}
