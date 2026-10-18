#pragma once

#include <climits>
#include <list>


class CEventsDispatcher : public CEventsDispatcherBase, public Window {
public:
    int init() override;
    void quit() override;

    void dispatchSyncEventByNoUIThread(IEvent *pEvent) override;
    void dispatchUnsyncEvent(IEvent *pEvent) override;
    void dispatchUnsyncEventDelayed(IEvent* pEvent, int delayInMs) override;

    void postExecInUIThread(std::function<void()> f) override;
    void postExecInUIThreadDelayed(std::function<void()> f, int delayInMs) override;

protected:
    virtual LRESULT wndProc(uint32_t message, WPARAM wParam, LPARAM lParam);

    void processExecItems();

    struct ExecItem {
        std::function<void()>       func;
        int64_t                     execTime = 0;
    };

    using ListExecItems = std::list<ExecItem>;

    std::mutex                      _mutex;
    ListExecItems                   _execItems;
    int64_t                         _nextTime = INT64_MAX;

};
