
#include "../../GfxRaw/win32/GdiplusGraphicsLite.h"
#include "../../Window/IWindow.h"
#include "../MPlayerApp.h"
#include "../VersionUpdate.h"
#include "../OnlineSearch.h"
#include "../AutoProcessEmbeddedLyrics.h"
#include "../MPFloatingLyrWnd.h"
#include "../LyricShowObj.h"
#include "../../Utils/App.h"
#include "../../Utils/win32/MiniDump.h"
#include "MPMsg.h"
#include "ProcessEnum.h"


// #define _MEM_LEAK_CHECK

#if defined(_DEBUG) && defined(_MEM_LEAK_CHECK)
//
// Go link below to check, how to use this
// http://www.codeproject.com/KB/applications/leakfinder.aspx
//
#include "Stackwalker.h"
#endif


bool terminateAnotherInstance() {
    VecInts processes = listAllProcesses();

    WCHAR selfNameUcs2[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, selfNameUcs2, CountOf(selfNameUcs2));
    uint32_t dwCurrentPID = GetCurrentProcessId();

    auto selfName = ucs2ToUtf8(selfNameUcs2, -1);

    for (auto pid : processes) {
        if (pid == dwCurrentPID) {
            continue;
        }

        auto name = getProcessFileName(pid);
        if (name == selfName) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, pid);
            if (hProcess != nullptr) {
                ERR_LOG0("Terminate another running instance.");
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
                return true;
            }
        }
    }

    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *lpCmdLine, int nCmdShow) {
    // 须在创建任何窗口前启用，否则高 DPI 下整窗被系统拉伸导致字体发糊
    enableProcessDpiAwareness();

    setAppInstance(hInstance);

    initBaseFramework(0, nullptr, "MusicPlayer.log", "MusicPlayer.ini", "MusicPlayer");
    setFileNoReadOnly(g_profile.getFile());

    LOG1(LOG_LVL_INFO, "start %s.", getAppNameLong().c_str());

    MiniDumper::init(getAppDataFile(SZ_APP_NAME "_crash.dmp").c_str(), [](cstr_t dumpFileSaved) {
        ERR_LOG0("Crash dump saved.");
    });

    if (!MPlayerApp::getInstance()->init()) {
        ERR_LOG0("Failed to init MPlayerApp.");
        return 1;
    }

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    MPlayerApp::getInstance()->quit();

    return msg.wParam;
}

bool MPlayerApp::isAnotherInstanceRunning() {
    assert(!m_hMutexRuning);
    HANDLE hMutexRuning = CreateMutexA(nullptr, true, SZ_MUTEX_RUNNING);
    int nLastErr = GetLastError();

    CloseHandle(hMutexRuning);

    return nLastErr == ERROR_ALREADY_EXISTS;
}

bool MPlayerApp::setRunningFlag() {
    // Is Product already running?
    assert(!m_hMutexRuning);
    m_hMutexRuning = CreateMutexA(nullptr, true, SZ_MUTEX_RUNNING);
    if (GetLastError() == ERROR_ALREADY_EXISTS ) {
        CloseHandle(m_hMutexRuning);
        m_hMutexRuning = nullptr;
        return false;
    }

    return true;
}

cstr_t getAppIniFile() {
    //
    // init Base frame, save log and settings in app dir
    return SZ_PROFILE_NAME;
}

bool MPlayerApp::init() {

    auto cmdLine = ucs2ToUtf8(GetCommandLineW());

    if (isAnotherInstanceRunning()) {
        HWND hWndMsg = FindWindowA(MSG_WND_CLASS_NAME, nullptr);
        if (hWndMsg) {
            sendActivateMainWnd(hWndMsg);

            if (!cmdLine.empty()) {
                DBG_LOG0("Another Instance is running, send command line to it.");
                sendCommandLine(hWndMsg, cmdLine.c_str());
            }
            return false;
        } else {
            terminateAnotherInstance();
        }
    }

    // init Winsock, Ole, Common Controls, gdiplus
    WSADATA WsaData = { 0 };
    if (WSAStartup(0x0101, &WsaData) != 0) {
        ERR_LOG1("WSAStartup FAILED: %s", getLastSysErrorDesc().c_str());
    }

    CGdiplusGraphicsLite::startup();

    return _init();
}

void MPlayerApp::quit() {
    if (!m_hMutexRuning) {
        return;
    }

    //
    // 先设置此标志表示退出，其他地方会用到根据此标志判断
    CloseHandle(m_hMutexRuning);
    m_hMutexRuning = nullptr;

    _quit();

    CGdiplusGraphicsLite::shutdown();
}
