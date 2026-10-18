#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT	0x0500
#include <windows.h>

#include <crtdbg.h>
#include <stdio.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <time.h>
#include <cstdarg>
#include "Utils.h"
#include "Log.h"

/*

static HWND _hDebugWnd;
#include <tchar.h>


#define SENDDEBUGMESSAGE    19771212
#define    DEBUGCLASSNAME        "Debug Tracer Class"
#define    DEBUGWNDNAME        "Debug Tracer"

void logTrace(void *lpData, int nSize)
{
    COPYDATASTRUCT data;

    data.dwData = SENDDEBUGMESSAGE;
    data.lpData = (void *)lpData;
    data.cbData = nSize;

    if (_hDebugWnd == nullptr)
    _hDebugWnd = FindWindow(DEBUGCLASSNAME, DEBUGWNDNAME);

    if (_hDebugWnd)
    ::sendMessage(_hDebugWnd, WM_COPYDATA, nullptr, (LPARAM)&data);
}


void DBG_LOG0(cstr_t szData)
{
    char        szBuffer[1024];
    SYSTEMTIME    LocalTime;
    int        nLen;
    getLocalTime(&LocalTime);
    nLen = wsprintf(szBuffer, "|T%d-%d %d:%d:%d|M%s", LocalTime.wMonth, LocalTime.wDay,
    LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, szData);

    char        szBuff[1024];
    WideCharToMultiByte(CP_ACP, 0, szBuffer, nLen, szBuff, CountOf(szBuff), 0, nullptr);

    logTrace((void *)szBuff, strlen(szBuff));
}

    DBG_LOG0("|MCWinamp2Player::quit()");
*/

// determine number of elements in an array (not  uint8_ts)

// 日志文件的最大长度为1MB
#define MAX_MSG_LEN             (1024 * 10)
#define MAX_LOG_FILE_LEN        (1024 * 1024)

#ifdef _WIN32
#define SENDDEBUGMESSAGE    19771212
#define    DEBUGCLASSNAME   "Debug Tracer Class"
#define    DEBUGWNDNAME     "Debug Tracer"

static HWND _hDebugWnd;

void logTrace(void *lpData, int nSize) {
    COPYDATASTRUCT data;

    data.dwData = SENDDEBUGMESSAGE;
    data.lpData = (void *)lpData;
    data.cbData = strlen((cstr_t)lpData) + 1;

    if (_hDebugWnd == nullptr) {
        _hDebugWnd = FindWindowA(DEBUGCLASSNAME, DEBUGWNDNAME);
    }

    if (_hDebugWnd) {
        ::SendMessage(_hDebugWnd, WM_COPYDATA, NULL, (LPARAM)&data);
    }
}
#else


void logTraceToPipe(void *lpData, int nSize) {
    static int _fd = -1;
    if (_fd == -1) {
        cstr_t SZ_PIPE_NAME = "/tmp/debug.pipe";
        int nRet = mkfifo(SZ_PIPE_NAME, S_IRWXO | S_IRWXG | S_IRWXU);
        if (nRet != 0 && errno != EEXIST) {
            return;
        }

        _fd = ::open(SZ_PIPE_NAME, O_NONBLOCK | O_WRONLY);
        if (_fd == -1) {
            return;
        }
    }

    write(_fd, lpData, nSize);
    write(_fd, "\n", 1);
}

void logTrace(void *lpData, int nSize) {
    printf("%s\n", (const char*)lpData);

    logTraceToPipe(lpData, nSize);
}

#endif


/*
|F%s|L%d|E%d|M%s
D: Date
F: File
L: Line
E: lEvel  (Error)
M: Message
*/

//////////////////////////////////////////////////////////////////////


static cstr_t ignoreRootDir(cstr_t szFile, cstr_t szRootDir) {
    cstr_t szRet = szFile;

    while (*szRet && *szRet == *szRootDir) {
        szRootDir++;
        szRet++;
    }

    if (*szRootDir == '\0') {
        return szRet;
    } else {
        return szFile;
    }
}


CLog::CLog() {
    m_fpLog = nullptr;
}

CLog::~CLog() {
    if (m_fpLog) {
        fclose(m_fpLog);
    }
}

void CLog::logDumpStr(uint8_t byLevel, cstr_t szFile, int nLine, cstr_t szStr, int nStrLen) {
    char *msg = new char[nStrLen + 1];
    assert(msg);

    memcpy(msg, szStr, nStrLen);
    msg[nStrLen] = '\0';

    writeLog(byLevel, szFile, nLine, "%s", msg);

    delete[] msg;
}

void CLog::writeLog( uint8_t byLevel, cstr_t szFile, int nLine, cstr_t szFormat, ...) {
#ifndef DEBUG
    // 将发行版本中的调试信息去掉
    if (byLevel == LOG_LVL_DEBUG) {
        return;
    }
#endif    // DEBUG

    szFile = ignoreRootDir(szFile, m_srcRootDir.c_str());

    va_list args;

    char szBuffer[MAX_MSG_LEN] = "";
    int nLen = 0;

    auto time = DateTime::localTime();

    {
        nLen = snprintf(szBuffer, CountOf(szBuffer) - 1, "|T%d-%d %d:%d:%d|F%s|L%d|E%d|M",
            time.month(), time.day(), time.hour(), time.minute(), time.second(),
            szFile, nLine, byLevel);

        // 格式化日志
        char *szMsg = nullptr;
        va_start(args, szFormat);
        szMsg = szBuffer + nLen;
        int nBuf = vsnprintf(szMsg, CountOf(szBuffer) - nLen, szFormat, args);
        va_end(args);

        // was there an error? was the expanded string too long?
        if (nBuf < 0 || nBuf >= CountOf(szBuffer) - nLen) {
            strcpy(szMsg, "Insufficient log buffer space.");
        } else {
            szMsg[nBuf] = '\0';
        }
    }

    nLen = (int)strlen(szBuffer) + 1;

#if defined(_DEBUG) || defined(DEBUG)
    logTrace(szBuffer, nLen);
#endif

    // 写入日志文件
    MutexAutolock autoLock(m_mutex);
    if (m_fpLog) {
        if (ftell(m_fpLog) >= MAX_LOG_FILE_LEN) {
            fclose(m_fpLog);
            m_fpLog = nullptr;
            openLogFile();
        }
    } else {
        //
        // 没有打开日志文件，试着重新打开日志文件
        if (openLogFile()) {
            writeLog(LOG_LVL_INFO, __FILE__, __LINE__, "log File is NOT opened, open it At %d-%d-%d %d:%d:%d. LOG STARTED:)\n",
                time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second());
        }
    }

    if (m_fpLog) {
        fwrite(szBuffer, 1, nLen - 1, m_fpLog);
        fwrite("\n", 1, strlen("\n"), m_fpLog);
        fflush(m_fpLog);
    } else {
        printf("%s\n", szBuffer);
    }
}

void CLog::init(cstr_t szLogFileName) {
    if (!szLogFileName) {
        return;
    }

    if (strchr(szLogFileName, PATH_SEP_CHAR) != nullptr) {
        // It might be full file path
        m_logFile = szLogFileName;
    } else {
        m_logFile = szLogFileName;
        // getAppDataDir(m_szLogFile);
        // strcat_safe(m_szLogFile, CountOf(m_szLogFile), szLogFileName);
    }

    if (!openLogFile()) {
        return;
    }

    auto time = DateTime::localTime();

    writeLog(LOG_LVL_INFO, __FILE__, __LINE__, "Started log : %s At %d-%d-%d %d:%d:%d. LOG STARTED:)", szLogFileName,
        time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second());
}

void CLog::close() {
    if (m_fpLog) {
        fclose(m_fpLog);
        m_fpLog = nullptr;
    }
}

void CLog::setSrcRootDir(cstr_t szSrcFile, int nCurFileDeep) {
    cstr_t szDirEndPos = nullptr;

    szDirEndPos = szSrcFile + strlen(szSrcFile);

    while (szDirEndPos > szSrcFile) {
        if (*szDirEndPos == PATH_SEP_CHAR) {
            if (nCurFileDeep > 0) {
                nCurFileDeep--;
            } else {
                break;
            }
        }
        szDirEndPos--;
    }

    m_srcRootDir.assign(szSrcFile, int(szDirEndPos - szSrcFile));
}

bool CLog::openLogFile() {
    if (m_logFile.empty()) {
        return false;
    }

    if (m_fpLog) {
        fclose(m_fpLog);
        m_fpLog = nullptr;
    }

    uint64_t nFileLength;
    if (getFileLength(m_logFile.c_str(), nFileLength) && nFileLength >= MAX_LOG_FILE_LEN) {
        deleteFile(m_logFile.c_str());
    }

    m_fpLog = fopenUtf8(m_logFile.c_str(), "a+");

    return (m_fpLog != nullptr);
}
