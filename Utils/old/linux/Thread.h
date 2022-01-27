// Thread.h: interface for the CThread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREAD_H__5ECE071C_BE98_469F_808C_6C8AFCB2A55A__INCLUDED_)
#define AFX_THREAD_H__5ECE071C_BE98_469F_808C_6C8AFCB2A55A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


typedef void (*FUNThread)(void * lpParam);

#define THREAD_BASE_PRIORITY_LOWRT  15  // value that gets a thread to LowRealtime-1
#define THREAD_BASE_PRIORITY_MAX    2   // maximum thread base priority boost
#define THREAD_BASE_PRIORITY_MIN    (-2)  // minimum thread base priority boost
#define THREAD_BASE_PRIORITY_IDLE   (-15) // value that gets a thread to idle

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_ERROR_RETURN    (MAXLONG)

#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

class CThread
{
public:
    CThread();
    virtual ~CThread();

    bool create(FUNThread function, void* lpData);
    void destroy();
    void suspend();
    void resume();
    void join();

    uint32_t getPriority() const;
    uint32_t setPriority(uint32_t priority);

    bool isRunning();

protected:
    static void *threadFunction(void *lpParam);

protected:
    FUNThread        m_funThead;
    void            *m_lpData;

    pthread_t        m_threadID;

};

#endif // !defined(AFX_THREAD_H__5ECE071C_BE98_469F_808C_6C8AFCB2A55A__INCLUDED_)
