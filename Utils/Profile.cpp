﻿/********************************************************************
    Created  :    2002/01/04    21:30
    FileName :    Profile.cpp
    Author   :    xhy

    Purpose  :    
*********************************************************************/

#include "Utils.h"
#include "Profile.h"


CProfile::CProfile() {
}

CProfile::~CProfile() {
    close();
}

//
// 区分大小写，将szAppName, szKey添加到缓存中
void CProfile::addKeys(cstr_t szAppName, cstr_t szKeyName, cstr_t szValue) {
    ListSections::iterator itApp;
    Section *appSec = nullptr;
    string strKeyNameLower;

    // find AppName
    for (itApp = m_listAppSect.begin(); itApp != m_listAppSect.end(); itApp++) {
        appSec = *itApp;
        if (strcasecmp(appSec->strAppName.c_str(), szAppName) == 0) {
            break;
        }
    }
    if (appSec == nullptr || itApp == m_listAppSect.end()) {
        // append new app secton
        appSec = new Section;
        appSec->strAppName = szAppName;
        m_listAppSect.push_back(appSec);
    }

    strKeyNameLower = szKeyName;
    toLower((char *)strKeyNameLower.c_str());
    appSec->mapKeys[strKeyNameLower] = szValue;
}

bool CProfile::getKey(cstr_t szAppName, cstr_t szKeyName, MapStrings::iterator &it) {
    ListSections::iterator itApp;
    Section *appSec = nullptr;
    string strKeyNameLower;

    strKeyNameLower = szKeyName;
    toLower((char *)strKeyNameLower.c_str());

    // 查找AppName
    for (itApp = m_listAppSect.begin(); itApp != m_listAppSect.end(); itApp++) {
        appSec = *itApp;
        if (strcasecmp(appSec->strAppName.c_str(), szAppName) == 0) {
            break;
        }
    }
    if (appSec == nullptr || itApp == m_listAppSect.end()) {
        return false;
    }

    //
    // 查找KeyName
    it = appSec->mapKeys.find(strKeyNameLower);
    if (it == appSec->mapKeys.end()) {
        return false;
    }

    return true;
}

void CProfile::setKeyDefaultIfNotExist(cstr_t szAppName, cstr_t szKeyName, cstr_t szValue) {
    ListSections::iterator itApp;
    Section *appSec = nullptr;
    string strKeyNameLower;
    MapStrings::iterator it;

    strKeyNameLower = szKeyName;
    toLower((char *)strKeyNameLower.c_str());

    // 查找AppName
    for (itApp = m_listAppSect.begin(); itApp != m_listAppSect.end(); itApp++) {
        appSec = *itApp;
        if (strcasecmp(appSec->strAppName.c_str(), szAppName) == 0) {
            break;
        }
    }
    if (appSec == nullptr || itApp == m_listAppSect.end()) {
        // append new app secton
        appSec = new Section;
        appSec->strAppName = szAppName;
        m_listAppSect.push_back(appSec);
        appSec->mapKeys[strKeyNameLower] = szValue;
        return;
    }

    it = appSec->mapKeys.find(strKeyNameLower);
    if (it == appSec->mapKeys.end()) {
        appSec->mapKeys[strKeyNameLower] = szValue;
    }
}

void CProfile::init(cstr_t szProfile, cstr_t szDefKey) {
    close();

    if (szDefKey == nullptr) {
        m_strDefAppName = "";
    } else {
        m_strDefAppName = szDefKey;
    }

    if (strlen(szProfile) > 3) {
        if (((szProfile[1] == ':') && (szProfile[2] == PATH_SEP_CHAR)) ||
            (szProfile[0] == PATH_SEP_CHAR)) {
            // 如果已经是全路径，则不管
            m_strProfile = szProfile;
            return;
        }
    }

    m_strProfile = szProfile;
    //    m_strProfile = getAppDataDir();
    //    m_strProfile += szProfile;
}

void CProfile::close() {
    m_strDefAppName = "";
    m_strProfile = "";

    ListSections::iterator itApp;
    Section *appSec;

    // 查找AppName
    for (itApp = m_listAppSect.begin(); itApp != m_listAppSect.end(); itApp++) {
        appSec = *itApp;
        delete appSec;
    }
    m_listAppSect.clear();
}


void CProfile::doCache() {
    string str;
    cstr_t szBeg, szEnd;
    string strAppName, strKey, strValue;

    if (!readFile(m_strProfile.c_str(), str)) {
        return;
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
        strAppName = "";
        strAppName.append(szBeg, szEnd);
        trimStr(strAppName);

        szBeg = szEnd + 1;

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
            strKey = "";
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
            strValue = "";
            strValue.append(szBeg, szEnd);

            addKeys(strAppName.c_str(), strKey.c_str(), strValue.c_str());

            szBeg = szEnd;
        }
    }
}

void CProfile::setDefaultIfNotExist(cstr_t szAppName, cstr_t szKeyName, cstr_t szDefault) {
    setKeyDefaultIfNotExist(szAppName, szKeyName, szDefault);
}

void CProfile::setDefaultIfNotExist(cstr_t szAppName, cstr_t szKeyName, int nDefault) {
    char szDefault[64];
    itoa(nDefault, szDefault);

    setKeyDefaultIfNotExist(szAppName, szKeyName, szDefault);
}

cstr_t CProfile::getString(cstr_t szAppName, cstr_t szKeyName, cstr_t szDefault) {
    assert(!m_strProfile.empty());

    if (szKeyName[0] == '\0') {
        return "";
    }

    MapStrings::iterator itKey;

    if (!getKey(szAppName, szKeyName, itKey)) {
        char szReturnedString[1024];
        getPrivateProfileStringUtf8(szAppName, szKeyName, szDefault, szReturnedString, 1024, m_strProfile.c_str());
        addKeys(szAppName, szKeyName, szReturnedString);
        if (!getKey(szAppName, szKeyName, itKey)) {
            assert(0 && "getKey Can't be FAILED!");
            return "";
        }
    }

    return (*itKey).second.c_str();
}

bool CProfile::writeString(cstr_t szAppName, cstr_t szKeyName, cstr_t szValue) {
    assert(!m_strProfile.empty());

    uint32_t dwRet;
    MapStrings::iterator itKey;

    dwRet = writePrivateProfileStringUtf8(szAppName, szKeyName, szValue, m_strProfile.c_str());

    if (!getKey(szAppName, szKeyName, itKey)) {
        addKeys(szAppName, szKeyName, szValue);
    } else {
        (*itKey).second = szValue;
    }

    return dwRet;
}


uint32_t CProfile::getInt(cstr_t szAppName, cstr_t szKeyName, int nDefault) {
    assert(!m_strProfile.empty());

    if (szKeyName[0] == '\0') {
        return nDefault;
    }

    char szDefault[64];
    itoa(nDefault, szDefault);

    cstr_t value = getString(szAppName, szKeyName, szDefault);

    return atoi(value);
}

bool CProfile::writeInt(cstr_t szAppName, cstr_t szKeyName, int value) {
    assert(!m_strProfile.empty());

    char szValue[64];

    itoa(value, szValue);

    return writeString(szAppName, szKeyName, szValue);
}


cstr_t CProfile::getFile() {
    return m_strProfile.c_str();
}

cstr_t CProfile::getDefAppName() {
    return m_strDefAppName.c_str();
}

//
// support encrypt keyname and value
//
void encryptStr(cstr_t str, string &strOut) {
    strOut = base64Encode((uint8_t *)str, strlen(str));
}

void decryptStr(cstr_t str, string &strOut) {
    strOut = base64Decode(str, strlen(str));
}
/*
void testEnDecrypt()
{
    char        szString[] = "012345adjkl;ge;lk561./zx/,.ckl";
    string    str, strOut, strOut2;

    for (int i = 0; i < 20; i++)
    {
        str.clear();
        str.append(szString, i);
        encryptStr(str.c_str(), strOut);
        decryptStr(strOut.c_str(), strOut2);
        assert(strcmp(str.c_str(), strOut2.c_str()) == 0);
    }
}
*/

string CProfile::encryptGetString(cstr_t szAppName, cstr_t szKeyName, cstr_t szDefault) {
    assert(!m_strProfile.empty());

    if (isEmptyString(szKeyName)) {
        return "";
    }

    string strKeyNameEncrypt, strValue;
    encryptStr(szKeyName, strKeyNameEncrypt);

    char szReturnedString[512];
    MapStrings::iterator itKey;
    if (!getKey(szAppName, strKeyNameEncrypt.c_str(), itKey)) {
        encryptStr(szDefault, strValue);
        getPrivateProfileStringUtf8(szAppName, strKeyNameEncrypt.c_str(), strValue.c_str(), szReturnedString, CountOf(szReturnedString), m_strProfile.c_str());
        addKeys(szAppName, strKeyNameEncrypt.c_str(), szReturnedString);
    } else {
        strcpy_safe(szReturnedString, CountOf(szReturnedString), (*itKey).second.c_str());
    }
    decryptStr(szReturnedString, strValue);
    strcpy_safe(szReturnedString, CountOf(szReturnedString), strValue.c_str());

    return szReturnedString;
}

bool CProfile::encryptWriteString(cstr_t szAppName, cstr_t szKeyName, cstr_t szValue) {
    assert(!m_strProfile.empty());

    uint32_t dwRet;
    MapStrings::iterator itKey;
    string strKeyNameEncrypt, strValueEncrypt;

    encryptStr(szKeyName, strKeyNameEncrypt);
    encryptStr(szValue, strValueEncrypt);

    dwRet = writePrivateProfileStringUtf8(szAppName, strKeyNameEncrypt.c_str(), strValueEncrypt.c_str(), m_strProfile.c_str());

    if (!getKey(szAppName, strKeyNameEncrypt.c_str(), itKey)) {
        addKeys(szAppName, strKeyNameEncrypt.c_str(), szValue);
    } else {
        (*itKey).second = strValueEncrypt;
    }

    return dwRet;
}

int CProfile::encryptGetInt(cstr_t szKeyName, int value) {
    string str = encryptGetString(szKeyName, "");
    if (str.empty()) {
        return value;
    } else {
        return atoi(str.c_str());
    }
}

void CProfile::encryptWriteInt(cstr_t szKeyName, int value) {
    encryptWriteString(szKeyName, stringPrintf("%d", value).c_str());
}
