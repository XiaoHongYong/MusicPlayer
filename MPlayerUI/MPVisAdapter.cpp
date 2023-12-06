﻿#include "MPlayerApp.h"
#include "../MPlayer/Player.h"
#include "MPVisAdapter.h"


CMPVisAdapter::CMPVisAdapter() {
    OBJ_REFERENCE_INIT
}

CMPVisAdapter::~CMPVisAdapter() {

}

ResultCode CMPVisAdapter::init(IMPlayer *pPlayer) {
    return ERR_OK;
}

ResultCode CMPVisAdapter::quit() {
    return ERR_OK;
}

int CMPVisAdapter::render(VisParam *visParam) {
    CEventVisDrawUpdate *pEvent = new CEventVisDrawUpdate;

    // memcpy(&m_visParam, visParam, sizeof(m_visParam));

    pEvent->pVisParam = visParam; // &m_visParam;
    pEvent->eventType = ET_VIS_DRAW_UPDATE;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);

    return ERR_OK;
}
