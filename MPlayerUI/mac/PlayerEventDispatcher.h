#pragma once


class CPlayerEventDispatcher {
public:
    CPlayerEventDispatcher();
    ~CPlayerEventDispatcher();

    void init() {
    }

    void quit();

    void startLyrDrawUpdate();

    void stopLyrDrawUpdate();

protected:
    int                         m_nTimeOutUpdateLyr;
    void                        *m_pInternal;

};

extern CPlayerEventDispatcher g_playerEventDispatcher;
