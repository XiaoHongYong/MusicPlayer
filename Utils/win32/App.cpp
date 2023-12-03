﻿#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT	0x0500
#include <windows.h>
#include <ShlObj_core.h>
#include "../App.h"
#include "../../TinyJS/utils/FileApi.h"


string _getAppResourceDir() {
    char path[MAX_PATH] = {0};
    GetModuleFileName(nullptr, path, CountOf(path));

    return path;
}

string _getAppDataDir(cstr_t szDefAppName) {
    assert(szDefAppName != nullptr);

    bool bInUserDir = false;

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionEx(&osvi)) {
        if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 5) {
            bInUserDir = true;
        }
    }
    if (bInUserDir) {
        //
        // PerUserSettings?
        //
        string fn = _getAppResourceDir();
        fn += "install.ini";
        if (isFileExist(fn.c_str())) {
            CProfile file;
            file.init(fn.c_str(), "install");
            bInUserDir = file.getBool("PerUserSettings", true);
            if (!bInUserDir) {
                // To verify that it is writable.
                if (!isDirWritable(getAppResourceDir().c_str())) {
                    bInUserDir = true;
                }
            }
        }
    }

    if (bInUserDir) {
        char path[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetSpecialFolderPath(nullptr, path, CSIDL_APPDATA, false))) {
            string dataDir = dirStringJoin(path, szDefAppName);
            if (!isDirExist(dataDir.c_str())) {
                createDirectory(dataDir.c_str());
            }

            return dataDir;
        }
    }

    return _getAppResourceDir();
}

/*
static void _invalid_parameter(
    const WCHAR * expression,
    const WCHAR * function,
    const WCHAR * file,
    unsigned int line,
    uintptr_t pReserved
    ) {
}*/

bool initBaseFramework(int argc, const char *argv[], cstr_t logFile, cstr_t profileName, cstr_t defAppName) {

    setAppResourceDir(_getAppResourceDir().c_str());
    setAppDataDir(_getAppDataDir(defAppName).c_str());

    g_profile.init(getAppDataFile(profileName).c_str(), defAppName);
    g_log.init(getAppDataFile(logFile).c_str());

    // _set_invalid_parameter_handler(_invalid_parameter);

    return true;
}
