﻿#include "../UtilsTypes.h"
#include "Event.h"


Event::Event(bool bManualSet, bool bInitialStat) {
    m_hObject = CreateEvent(nullptr, bManualSet, bInitialStat, nullptr);
}

Event::~Event() {
    CloseHandle(m_hObject);
}

bool Event::set() {
    return SetEvent(m_hObject) != 0;
}

bool Event::reset() {
    return ResetEvent(m_hObject) != 0;
}

bool Event::acquire(uint32_t nTimeOut) {
    switch (WaitForSingleObject(m_hObject, nTimeOut)) {
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
        return true;
    case WAIT_TIMEOUT:
    case WAIT_FAILED:
        break;
    }

    return false;
}
