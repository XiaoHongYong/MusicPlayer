﻿

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT	0x0500
#include <windows.h>
#include <time.h>
#include <olectl.h>
#include "../MPlayerApp.h"


void analyseProxySetting(cstr_t szProxySetting, char szServer[], int nMaxSize, int &nPort);

void execute(Window *pWnd, cstr_t szExe, cstr_t szParam) {
    ShellExecuteW(pWnd != nullptr ? pWnd->getWndHandle() : nullptr, L"open",
        utf8ToUCS2(szExe).c_str(),
        utf8ToUCS2(szParam).c_str(), nullptr, SW_SHOWNORMAL);
}

bool setClipboardText(Window *pWnd, cstr_t szText) {
    return tobool(copyTextToClipboard(szText));
}

bool SHDeleteFile(cstr_t file, Window *pWndParent) {
    SHFILEOPSTRUCTW sop;
    utf16string fn = utf8ToUCS2(file);
    fn.push_back('\0');

    sop.hwnd = pWndParent->getWndHandle();
    sop.wFunc = FO_DELETE;
    sop.pFrom = fn.c_str();
    sop.pTo = nullptr;
    sop.fFlags = FOF_ALLOWUNDO;
    sop.fAnyOperationsAborted = false;
    sop.hNameMappings = nullptr;
    sop.lpszProgressTitle = L"";

    return (SHFileOperationW(&sop) == 0 && !sop.fAnyOperationsAborted);
}

bool SHCopyFile(cstr_t srcFile, cstr_t targFile, Window *pWndParent) {
    SHFILEOPSTRUCTW sop;
    utf16string fnSrc = utf8ToUCS2(srcFile); fnSrc.push_back('\0');
    utf16string fnTarg = utf8ToUCS2(targFile); fnTarg.push_back('\0');

    sop.hwnd = pWndParent->getWndHandle();
    sop.wFunc = FO_COPY;
    sop.pFrom = fnSrc.c_str();
    sop.pTo = fnTarg.c_str();
    sop.fFlags = FOF_ALLOWUNDO;
    sop.fAnyOperationsAborted = false;
    sop.hNameMappings = nullptr;
    sop.lpszProgressTitle = L"";

    return (SHFileOperationW(&sop) == 0 && !sop.fAnyOperationsAborted);
}

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#endif

bool setFileNoReadOnly(cstr_t file) {
    auto fn = utf8ToUCS2(file);
    uint32_t dwAttr;
    dwAttr = GetFileAttributesW(fn.c_str());

    if (dwAttr != INVALID_FILE_ATTRIBUTES) {
        if (isFlagSet(dwAttr, FILE_ATTRIBUTE_READONLY)) {
            return SetFileAttributes(fn.c_str(), dwAttr & ~FILE_ATTRIBUTE_READONLY);
        } else {
            return true;
        }
    } else {
        return false;
    }
}

bool loadProxySvrFromIE(bool &bUseProxy, string &strSvr, int &nPort) {
    // 取得IE 的代理设置
    // [HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings]
    // REG_SZ: "ProxyServer"="127.1.1.1:80"
    // ftp=192.168.1.12:80;gopher=192.168.1.13:80;http=192.168.1.1:80;https=192.168.1.11:80
    HKEY hKeyInet;
    bool bRet = true;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0, KEY_QUERY_VALUE, &hKeyInet) == ERROR_SUCCESS) {
        char szBuff[256];

        // ProxyEnable?
        DWORD dwType = REG_DWORD, dwEnable, nSize = sizeof(szBuff);
        if (RegQueryValueExA(hKeyInet, "ProxyEnable", nullptr, &dwType, (uint8_t *)&dwEnable, &nSize) != ERROR_SUCCESS) {
            bRet = false;
            bUseProxy = false;
        } else {
            bUseProxy = dwEnable != 0;
        }

        // ProxyServer ?
        dwType = REG_SZ;
        nSize = sizeof(szBuff);
        if (RegQueryValueExA(hKeyInet, "ProxyServer", nullptr, &dwType, (uint8_t *)szBuff, &nSize) != ERROR_SUCCESS) {
            bRet = false;
        } else {
            string http;
            cstr_t szBeg = strstr(szBuff, "http=");
            if (szBeg) {
                szBeg += 5;
                cstr_t szEnd = strchr(szBeg, ';');
                if (szEnd) {
                    http.assign(szBeg, (int)(szEnd - szBeg));
                } else {
                    http = szBeg;
                }
            } else {
                http = szBuff;
            }

            char szServer[256] = { 0 };
            analyseProxySetting(http.c_str(), szServer, CountOf(szServer), nPort);
            strSvr = szServer;
        }

        RegCloseKey(hKeyInet);
    } else {
        bRet = false;
    }

    return bRet;
}

void getNotepadEditor(string &strEditor) {
    char szBuffer[MAX_PATH] = { 0 };

    GetWindowsDirectoryA(szBuffer, MAX_PATH);
    
    strEditor = dirStringJoin(szBuffer, "notepad.exe");
    if (isFileExist(strEditor.c_str())) {
        return;
    }

    strEditor = dirStringJoin(szBuffer, "system32\\notepad.exe");
    if (isFileExist(strEditor.c_str())) {
        return;
    }

    strEditor = dirStringJoin(szBuffer, "system\\notepad.exe");
    if (isFileExist(strEditor.c_str())) {
        return;
    }
}
