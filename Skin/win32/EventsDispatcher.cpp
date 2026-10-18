#include "../SkinTypes.h"
#include "../EventsDispatcherBase.h"
#include "EventsDispatcher.h"

#include <algorithm>
#include <climits>
#include <utility>


#define DISPATCH_EVENT_LPARAM   1212

#define MPWM_DISPATCH        (WM_USER + 1)
#define MPWM_QUIT            (WM_USER + 2)
#define MPWM_EXEC_TASK       (WM_USER + 3)

#define TIMER_EXEC_TASK     100

//////////////////////////////////////////////////////////////////////

int CEventsDispatcher::init() {
    if (!Window::createEx("MPMsg", "MPMsg", 0, 0, 1, 1, nullptr, 0)) {
        return ERR_FALSE;
    }

    return CEventsDispatcherBase::init();
}

void CEventsDispatcher::quit() {
    CEventsDispatcherBase::quit();

    Window::destroy();
}

void CEventsDispatcher::dispatchSyncEventByNoUIThread(IEvent *pEvent) {
    ::SendMessage(m_hWnd, MPWM_DISPATCH, (WPARAM)pEvent, DISPATCH_EVENT_LPARAM);
}

void CEventsDispatcher::dispatchUnsyncEvent(IEvent *pEvent) {
    ::PostMessage(m_hWnd, MPWM_DISPATCH, (WPARAM)pEvent, DISPATCH_EVENT_LPARAM);
}

void CEventsDispatcher::dispatchUnsyncEventDelayed(IEvent* pEvent, int delayInMs) {
    postExecInUIThreadDelayed([this, pEvent]() {
        ::PostMessage(m_hWnd, MPWM_DISPATCH, (WPARAM)pEvent, DISPATCH_EVENT_LPARAM);
    }, delayInMs);
}

void CEventsDispatcher::postExecInUIThread(std::function<void()> f) {
    {
        std::lock_guard autolock(_mutex);
        _execItems.push_back({std::move(f), 0});
    }
    // 必须唤醒 UI 消息泵；仅入队不会触发 scheduleInvalidate 等回调。
    ::PostMessage(m_hWnd, MPWM_EXEC_TASK, 0, 0);
}

void CEventsDispatcher::postExecInUIThreadDelayed(std::function<void()> f, int delayInMs) {
    if (delayInMs <= 0) {
        postExecInUIThread(std::move(f));
        return;
    }

    const int64_t now = (int64_t)getTickCount();
    const int64_t execTime = now + delayInMs;

    std::lock_guard autolock(_mutex);
    _execItems.push_back({std::move(f), execTime});
    if (execTime < _nextTime) {
        _nextTime = execTime;
        ::SetTimer(m_hWnd, TIMER_EXEC_TASK, (UINT)delayInMs, nullptr);
    }
}

void CEventsDispatcher::processExecItems() {
    while (true) {
        std::function<void()> func;

        {
            std::lock_guard autolock(_mutex);
            const int64_t now = (int64_t)getTickCount();
            for (auto it = _execItems.begin(); it != _execItems.end(); ++it) {
                if (it->execTime <= now) {
                    func = std::move(it->func);
                    _execItems.erase(it);
                    break;
                }
            }

            if (!func) {
                // 没有到期任务：为最近的延迟任务重设定时器。
                int64_t nextTime = INT64_MAX;
                for (const auto &item : _execItems) {
                    if (item.execTime < nextTime) {
                        nextTime = item.execTime;
                    }
                }
                _nextTime = nextTime;
                if (nextTime != INT64_MAX) {
                    const UINT delayMs = (UINT)std::max<int64_t>(1, nextTime - now);
                    ::SetTimer(m_hWnd, TIMER_EXEC_TASK, delayMs, nullptr);
                } else {
                    ::KillTimer(m_hWnd, TIMER_EXEC_TASK);
                }
                break;
            }
        }

        func();
    }
}

LRESULT CEventsDispatcher::wndProc(uint32_t message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case MPWM_DISPATCH:
        {
            if (lParam == DISPATCH_EVENT_LPARAM) {
                IEvent *pEvent = (IEvent *)wParam;
                dispatchSyncEvent(pEvent);
                return 0;
            }
            break;
        }
    case MPWM_EXEC_TASK:
        processExecItems();
        return 0;
    case WM_TIMER:
        {
            if (wParam == TIMER_EXEC_TASK) {
                processExecItems();
            }
            break;
        }
    }

    return Window::wndProc(message, wParam, lParam);
}
