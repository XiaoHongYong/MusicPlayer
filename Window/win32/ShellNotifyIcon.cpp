﻿#include "../WindowLib.h"
#include "ShellNotifyIcon.h"


CShellNotifyIcon::CShellNotifyIcon() {

}

CShellNotifyIcon::~CShellNotifyIcon() {

}

bool CShellNotifyIcon::addIcon(Window *pWnd, uint32_t nID, cstr_t szTip, HICON hIcon, uint32_t nCallbackMessage) {
    NOTIFYICONDATAW nif;
    
    nif.cbSize = sizeof(nif);
    nif.hWnd = pWnd->getWndHandle();
    nif.hIcon = hIcon;
    wcscpy_s(nif.szTip, CountOf(nif.szTip), utf8ToUCS2(szTip).c_str());
    nif.uID = nID;
    nif.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nif.uCallbackMessage = nCallbackMessage;

    return tobool(::Shell_NotifyIconW(NIM_ADD, &nif));
}

bool CShellNotifyIcon::delIcon(Window *pWnd, uint32_t nID) {
    NOTIFYICONDATA nif;

    nif.cbSize = sizeof(nif);
    nif.uID = nID;
    nif.hWnd = pWnd->getWndHandle();
    nif.uFlags = 0;

    return tobool(::Shell_NotifyIcon(NIM_DELETE, &nif));
}

bool CShellNotifyIcon::modifyIcon(Window *pWnd, uint32_t nID, cstr_t szTip, HICON hIcon) {
    NOTIFYICONDATAW nif;

    nif.cbSize = sizeof(nif);
    nif.hWnd = pWnd->getWndHandle();
    if (szTip) {
        wcscpy_s(nif.szTip, CountOf(nif.szTip), utf8ToUCS2(szTip).c_str());
    }
    nif.uID = nID;
    nif.hIcon = hIcon;
    nif.uFlags = 0;
    if (szTip) {
        nif.uFlags |= NIF_TIP;
    }
    if (hIcon) {
        nif.uFlags |= NIF_ICON;
    }

    return tobool(::Shell_NotifyIcon(NIM_MODIFY, &nif));
}
