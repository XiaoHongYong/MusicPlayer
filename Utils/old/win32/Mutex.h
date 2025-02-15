﻿#pragma once

#ifndef Utils_old_win32_Mutex_h
#define Utils_old_win32_Mutex_h


#define WAIT_FOREVER        INFINITE

class Mutex {
public:
    Mutex();
    virtual ~Mutex();

    bool acquire(uint32_t nTimeOut = WAIT_FOREVER);
    void release();

private:
    HANDLE                      m_hMutex;

};

class MutexAutolock {
public:
    MutexAutolock(Mutex &mutex) : m_pMutex(&mutex) { m_pMutex->acquire(); }
    ~MutexAutolock() { if (m_pMutex) m_pMutex->release(); }

    void unlock() { if (m_pMutex) { m_pMutex->release(); m_pMutex = nullptr; } }

    Mutex                       *m_pMutex;
};

#endif // !defined(Utils_old_win32_Mutex_h)
