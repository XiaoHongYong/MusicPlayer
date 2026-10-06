#include "MPlayerApp.h"
#include "MPSkinWnd.h"
#include "MPCommonCmdHandler.h"
#include "MPPlaylistCmdHandler.h"
#include "LyricShowObj.h"
#include "LyricShowTextEditObj.h"
#include "LyricShowAgentObj.h"
#include "../Skin/SkinRateCtrl.h"


/*
class IUIStateUpdater
{
public:
    IUIStateUpdater(void *pObj) { m_hObj = (HANDLE)pObj; }
    virtual ~IUIStateUpdater() { }
    virtual void enable(bool bEnable) = 0;
    virtual void setCheck(bool bCheck) = 0;

    HANDLE            m_hObj;

};

class CUIStateUpdaterButton : public IUIStateUpdater
{
public:
    CUIStateUpdaterButton(void *pObj) : IUIStateUpdater(pObj) { }
    void enable(bool bEnable)
    {
        CUIObject    *pObj;
        pObj = (CUIObject *)m_hObj;
        if (pObj->isEnable() != bEnable)
        {
            pObj->setEnable(bEnable);
            pObj->invalidate();
        }
    }
    void setCheck(bool bCheck)
    {
        CSkinButton    *pButton;
        pButton = (CSkinButton *)m_hObj;
        if (pButton->isCheck() != bCheck)
        {
            pButton->setCheck(bCheck);
        }
    }

};


class LIST_UISTATE_UPDATER : public list<IUIStateUpdater *>
{
public:
    void add(IUIStateUpdater *pUpdater)
    {
        iterator    it, itEnd;
        itEnd = end();
        for (it = begin(); it != itEnd; ++it)
        {
            IUIStateUpdater    *pItem = *it;
            if (pItem->m_hObj = pUpdater->m_hObj)
                return;
        }

        push_back(pUpdater);
    }
    void del(HANDLE hObj)
    {
        iterator    it, itEnd;
        itEnd = end();
        for (it = begin(); it != itEnd; ++it)
        {
            IUIStateUpdater    *pItem = *it;
            if (pItem->m_hObj = hObj)
            {
                erase(it);
                return;
            }
        }
        assert(0);
    }
};

class CUIPlayerStatusHandler : public IEventHandler
{
public:
    virtual void onEvent(const IEvent *pEvent)
    {
        if (pEvent->eventType == ET_PLAYER_STATUS_CHANGED)
        {
            CEventPlayerStatusChanged    *pPlayerStatus = (CEventPlayerStatusChanged *)pEvent;
            // if ()
            {
                LIST_UISTATE_UPDATER::iterator    it, itEnd;

                itEnd = m_listStatePlaypause.end();
                for (it = m_listStatePlaypause.begin(); it != itEnd; ++it)
                {
                    IUIStateUpdater        *pUpdater = *it;
                    pUpdater->setCheck(pPlayerStatus->status == PS_PLAYING);
                }
            }
        }
        else if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED)
        {

        }
    }

    virtual void addPlayPauseUpdater(IUIStateUpdater *pUpdater)
    {
        if (m_listStatePlaypause.empty())
            MPlayerApp::getEventsDispatcher()->registerHandler(ET_PLAYER_STATUS_CHANGED, this);
        m_listStatePlaypause.add(pUpdater);
    }

    virtual void delPlayPauseUpdater(HANDLE hObj)
    {
        m_listStatePlaypause.del(hObj);
        if (m_listStatePlaypause.empty())
            MPlayerApp::getEventsDispatcher()->unRegisterHandler(ET_PLAYER_STATUS_CHANGED, this);
    }

    LIST_UISTATE_UPDATER        m_listStatePlaypause;

};


CUIPlayerStatusHandler        g_UIPlayerStatusHandler;*/

class CPlayerShuffleEventHandler : public IEventHandler {
public:
    CPlayerShuffleEventHandler(CSkinNStatusButton *pShuffle) {
        m_pShuffle = pShuffle;
        if (g_player.isShuffle() != tobool(m_pShuffle->getStatus())) {
            pShuffle->setStatus(g_player.isShuffle() ? 1 : 0);
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_SETTING_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_SETTING_CHANGED) {
            CEventPlayerSettingChanged *pSettingEvt = (CEventPlayerSettingChanged *)pEvent;
            if (pSettingEvt->settingType == IMPEvent::MPS_SHUFFLE) {
                if (tobool(pSettingEvt->value) != tobool(m_pShuffle->getStatus())) {
                    m_pShuffle->setStatus(g_player.isShuffle() ? 1 : 0);
                }
            }
        }
    }

protected:
    CSkinNStatusButton          *m_pShuffle;

};

class CPlayerLoopEventHandler : public IEventHandler {
public:
    CPlayerLoopEventHandler(CSkinNStatusButton *pLoopBt) {
        m_pLoop = pLoopBt;
        if ((int)g_player.getLoop() != pLoopBt->getStatus()) {
            m_pLoop->setStatus(g_player.getLoop());
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_SETTING_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_SETTING_CHANGED) {
            CEventPlayerSettingChanged *pSettingEvt = (CEventPlayerSettingChanged *)pEvent;

            if (pSettingEvt->settingType == IMPEvent::MPS_LOOP) {
                if (pSettingEvt->value != m_pLoop->getStatus()) {
                    m_pLoop->setStatus(pSettingEvt->value);
                }
            }
        }
    }

protected:
    CSkinNStatusButton          *m_pLoop;

};

class CPlayerVolumeEventHandler : public IEventHandler {
public:
    CPlayerVolumeEventHandler(CSkinSeekCtrl *pVolume) {
        m_pVolume = pVolume;
        m_pVolume->setScrollInfo(0, MP_VOLUME_MAX, 0, g_player.getVolume());
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_SETTING_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_SETTING_CHANGED) {
            CEventPlayerSettingChanged *pSettingEvt = (CEventPlayerSettingChanged *)pEvent;
            if (pSettingEvt->settingType == IMPEvent::MPS_VOLUME) {
                if (pSettingEvt->value != m_pVolume->getScrollPos()) {
                    m_pVolume->setScrollPos(pSettingEvt->value);
                }
            }
        }
    }

protected:
    CSkinSeekCtrl               *m_pVolume;

};

class CPlayerSeekEventHandler : public IEventHandler {
public:
    CPlayerSeekEventHandler(CSkinSeekCtrl *pSeekCtrl) {
        m_pSeekCtrl = pSeekCtrl;
        pSeekCtrl->setScrollInfo(0, g_player.getMediaLength(), 0, 0);
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_MEDIA_CHANGED, ET_PLAYER_POS_UPDATE, ET_PLAYER_STATUS_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_POS_UPDATE || pEvent->eventType == ET_PLAYER_SEEK) {
            m_pSeekCtrl->setScrollPos(g_player.getPlayPos());
        } else if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED) {
            m_pSeekCtrl->setScrollInfo(0, g_player.getMediaLength(), 0, 0);
        } else if (pEvent->eventType == ET_PLAYER_STATUS_CHANGED) {
            if (g_player.getPlayerState() == PS_STOPPED) {
                m_pSeekCtrl->setScrollPos(0);
            }
        }
    }

protected:
    CSkinSeekCtrl               *m_pSeekCtrl;

};

class CPlayerPlayPauseBtHandler : public IEventHandler {
public:
    CPlayerPlayPauseBtHandler(CSkinNStatusButton *pButton) {
        int bCheck;

        bCheck = g_player.getPlayerState() == PS_PLAYING;

        m_pButtn = pButton;
        if (m_pButtn->getStatus() != bCheck) {
            m_pButtn->setStatus(bCheck);
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_STATUS_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_STATUS_CHANGED) {
            int bCheck;

            bCheck = g_player.getPlayerState() == PS_PLAYING;
            if (m_pButtn->getStatus() != bCheck) {
                m_pButtn->setStatus(bCheck);
            }
        }
    }

protected:
    CSkinNStatusButton          *m_pButtn;

};

class CPlayerPlayPauseToolbarHandler : public IEventHandler {
public:
    CPlayerPlayPauseToolbarHandler(CSkinToolbar *pToolbar) {
        bool bCheck;

        bCheck = g_player.getPlayerState() == PS_PLAYING;

        m_pToolbar = pToolbar;
        if (m_pToolbar->isCheck(ID_PLAYPAUSE) != bCheck) {
            m_pToolbar->setCheck(ID_PLAYPAUSE, bCheck, false);
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_STATUS_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_STATUS_CHANGED) {
            bool bCheck;

            bCheck = g_player.getPlayerState() == PS_PLAYING;
            if (m_pToolbar->isCheck(ID_PLAYPAUSE) != bCheck) {
                m_pToolbar->setCheck(ID_PLAYPAUSE, bCheck);
            }
        }
    }

protected:
    CSkinToolbar                *m_pToolbar;

};

class CPlayerShuffleRepeatToolbarHandler : public IEventHandler {
public:
    CPlayerShuffleRepeatToolbarHandler(CSkinToolbar *pToolbar) {
        m_pToolbar = pToolbar;

        if (g_player.isShuffle() != tobool(pToolbar->getBtStatus(ID_SHUFFLE))) {
            pToolbar->setBtStatus(ID_SHUFFLE, g_player.isShuffle() ? 1 : 0, false);
        }

        if (g_player.getLoop() != m_pToolbar->getBtStatus(ID_LOOP)) {
            m_pToolbar->setBtStatus(ID_LOOP, g_player.getLoop(), false);
        }

        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_SETTING_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_SETTING_CHANGED) {
            CEventPlayerSettingChanged *pSettingEvt = (CEventPlayerSettingChanged *)pEvent;
            if (pSettingEvt->settingType == IMPEvent::MPS_SHUFFLE) {
                if (pSettingEvt->value != m_pToolbar->getBtStatus(ID_SHUFFLE)) {
                    m_pToolbar->setBtStatus(ID_SHUFFLE, g_player.isShuffle() ? 1 : 0, true);
                }
            } else if (pSettingEvt->settingType == IMPEvent::MPS_LOOP) {
                if (pSettingEvt->value != m_pToolbar->getBtStatus(ID_LOOP)) {
                    m_pToolbar->setBtStatus(ID_LOOP, g_player.getLoop(), true);
                }
            }
        }
    }

protected:
    CSkinToolbar                *m_pToolbar;

};

//
// Player controls
//

// Media
class CMediaStereoStatusEventHandler : public IEventHandler {
public:
    CMediaStereoStatusEventHandler(CUIObject *pCtrl) {
        m_pStereoStatusCtrl = pCtrl;
        auto media = g_player.getCurrentMedia();
        if (media) {
            if (media->channels >= 2) {
                m_pStereoStatusCtrl->setPropertyInt("CurStatus", 1);
            } else {
                m_pStereoStatusCtrl->setPropertyInt("CurStatus", 0);
            }
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_MEDIA_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED) {
            auto media = g_player.getCurrentMedia();
            if (media) {
                if (media->channels >= 2) {
                    m_pStereoStatusCtrl->setPropertyInt("CurStatus", 1);
                } else {
                    m_pStereoStatusCtrl->setPropertyInt("CurStatus", 0);
                }
            }
        }
    }

protected:
    CUIObject                   *m_pStereoStatusCtrl;;

};

class CPlayerMediaChangedRateEventHandler : public IEventHandler {
public:
    CPlayerMediaChangedRateEventHandler(CSkinRateCtrl *pRateCtrl) {
        m_pRateCtrl = pRateCtrl;
        auto media = g_player.getCurrentMedia();
        if (media) {
            m_pRateCtrl->setRating(media->rating);
        }
        registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_MEDIA_CHANGED, ET_PLAYER_CUR_MEDIA_INFO_CHANGED);
    }

    virtual void onEvent(const IEvent *pEvent) {
        if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED || pEvent->eventType == ET_PLAYER_CUR_MEDIA_INFO_CHANGED) {
            auto media = g_player.getCurrentMedia();
            if (media) {
                m_pRateCtrl->setRating(media->rating);
            }
        }
    }

protected:
    CSkinRateCtrl               *m_pRateCtrl;

};


CMPSkinWnd::CMPSkinWnd() {
}

CMPSkinWnd::~CMPSkinWnd() {

}

void CMPSkinWnd::onPreCreate(bool &bTopmost, bool &bVisible) {
    bTopmost = settingGetTopmost();
}

void CMPSkinWnd::onCreate() {
    CSkinWnd::onCreate();

#ifdef _WIN32
    ::SendMessageA(m_hWnd, WM_SETICON, ICON_BIG, (WPARAM)LoadIconA(getAppInstance(), MAKEINTRESOURCEA(IDI_MPLAYER)));

    // 允许拖放歌词文件
    DragAcceptFiles(m_hWnd, true);
#endif
}

void CMPSkinWnd::onCommand(uint32_t id) {
    switch (id) {
#ifdef _WIN32
    case ID_SET_OPAQUE_100:
    case ID_SET_OPAQUE_90:
    case ID_SET_OPAQUE_80:
    case ID_SET_OPAQUE_70:
    case ID_SET_OPAQUE_60:
    case ID_SET_OPAQUE_50:
    case ID_SET_OPAQUE_40:
    case ID_SET_OPAQUE_30:
    case ID_SET_OPAQUE_20:
    case ID_SET_OPAQUE_10:
        {
            int nPercent = 100;

            if (id == ID_SET_OPAQUE_100)    nPercent = 100;
            else if (id == ID_SET_OPAQUE_100)    nPercent = 100;
            else if (id == ID_SET_OPAQUE_90)    nPercent = 90;
            else if (id == ID_SET_OPAQUE_80)    nPercent = 80;
            else if (id == ID_SET_OPAQUE_70)    nPercent = 70;
            else if (id == ID_SET_OPAQUE_60)    nPercent = 60;
            else if (id == ID_SET_OPAQUE_50)    nPercent = 50;
            else if (id == ID_SET_OPAQUE_40)    nPercent = 40;
            else if (id == ID_SET_OPAQUE_30)    nPercent = 30;
            else if (id == ID_SET_OPAQUE_20)    nPercent = 20;
            else if (id == ID_SET_OPAQUE_10)    nPercent = 10;

            settingSetOpaquePercent(nPercent);
        }
        return;
#endif
    case ID_TOPMOST:
        settingReverseTopmost();
        return;
    case ID_CLICK_THROUGH:
        settingReverseClickThrough();
        return;
    case ID_LDS_MULTI_LINE:
    case ID_LDS_STATIC_TXT:
    case ID_LDS_TWO_LINE:
    case ID_LDS_SINGLE_LINE:
    case ID_LDS_VOBSUB:
        {
            // special lyrics display style
            string name;
            if (id == ID_LDS_MULTI_LINE) {
                name = CLyricShowMultiRowObj::className();
            } else if (id == ID_LDS_STATIC_TXT) {
                name = SZ_TXT_LYR_CONTAINER;
            } else if (id == ID_LDS_TWO_LINE) {
                name = CLyricShowTwoRowObj::className();
            } else if (id == ID_LDS_SINGLE_LINE) {
                name = CLyricShowSingleRowObj::className();
            } else if (id == ID_LDS_VOBSUB) {
                name = CLyricShowVobSub::className();
            } else {
                assert(0);
            }

            CLyricShowAgentObj::setLyrDispStyleSettings(this, name.c_str());
        }
        return;
    default:
        break;
    }

    auto itEnd = m_listSkinCmdHandler.end();
    for (auto it = m_listSkinCmdHandler.begin(); it != itEnd; ++it) {
        ISkinCmdHandler *pHandler = *it;
        if (pHandler->onCommand(id)) {
            return;
        }
    }

    CSkinWnd::onCommand(id);
}

void CMPSkinWnd::onUIObjNotify(IUIObjNotify *pNotify) {
    LIST_SKINCMDHANDLER::iterator it, itEnd;

    itEnd = m_listSkinCmdHandler.end();
    for (it = m_listSkinCmdHandler.begin(); it != itEnd; ++it) {
        ISkinCmdHandler *pHandler = *it;
        if (pHandler->onUIObjNotify(pNotify)) {
            return;
        }
    }
}

bool CMPSkinWnd::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (CSkinWnd::onKeyDown(nChar, nFlags)) {
        return true;
    }

    if (nChar == VK_SPACE) {
        g_player.playPause();
        return true;
    } else if (nFlags == MK_COMMAND) {
        if (nChar == VK_LEFT) {
            g_player.prev();
            return true;
        } else if (nChar == VK_RIGHT) {
            g_player.next();
            return true;
        }
    }

    CMPSkinMainWnd *pMainWnd = MPlayerApp::getMainWnd();
    if (pMainWnd) {
        if (MPlayerApp::getHotkey().onKeyDown(this, nChar, nFlags)) {
            return true;
        }
    }

    return false;
}

void CMPSkinWnd::onSizeModeChanged(WndSizeMode sizeMode) {
    CSkinWnd::onSizeModeChanged(sizeMode);

    // fix the bug auto width with lyrics when the windows is Minimized.
    if (sizeMode == WndSizeMode_Normal) {
        CUIObject *pObj = getUIObjectByClassName(CLyricShowObj::className());
        if (pObj && !pObj->isKindOf(CLyricShowTextEditObj::className())) {
            CLyricShowObj *pLyrObj = (CLyricShowObj*)pObj;
            IEvent evt;
            evt.eventType = ET_LYRICS_CHANGED;
            pLyrObj->onEvent(&evt);
        }

        CSkinNStatusButton *pMaximizeBt = (CSkinNStatusButton *)getUIObjectById(ID_MAXIMIZE, CSkinNStatusButton::className());
        if (pMaximizeBt) {
            pMaximizeBt->setStatus(0);
        }
    } else if (sizeMode == WndSizeMode_Minimized) {
        // If it is a tool window, minimize it.
        if (isToolWindow() && isVisible()) {
            hide();
        }
    } else {
        CSkinNStatusButton *pMaximizeBt = (CSkinNStatusButton *)getUIObjectById(ID_MAXIMIZE, CSkinNStatusButton::className());
        if (pMaximizeBt) {
            pMaximizeBt->setStatus(1);
        }
    }
}

void CMPSkinWnd::onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) {
    if (pScrollBar->getID() == ID_VOLUME) {
        // set volume
        g_player.setVolume(pScrollBar->getScrollPos());
        long vol = g_player.getVolume();

        MPlayerApp::getInstance()->dispatchInfoText(stringPrintf("%s %d%%", _TLT("Set volume"), vol).c_str());
    } else if (pScrollBar->getID() == ID_SEEK) {
        int nPos, nPercent, nMediaLength;
        char szPos[256], szLength[256];

        nPos = pScrollBar->getScrollPos();
        nMediaLength = g_player.getMediaLength();
        if (nMediaLength != 0) {
            nPercent = nPos * 100 / nMediaLength;
            formatPlayTime(nPos, szPos);
            formatPlayTime(nMediaLength, szLength);
            MPlayerApp::getInstance()->dispatchInfoText(stringPrintf("%s %s/%s (%d%%)", _TLT("Seek"),
                szPos, szLength, nPercent).c_str(), "seek");
        }

        // seek lyrics too...
        g_player.onUISeekbarSeek(nPos);
    }
}

void CMPSkinWnd::onSkinLoaded() {
    CSkinWnd::onSkinLoaded();

    // load new skin command handler
    vector<string> vHandler;
    strSplit(m_strCmdHandler.c_str(), ',', vHandler);
    for (int i = 0; i < (int)vHandler.size(); i++) {
        trimStr(vHandler[i]);
        addCmdHandler(vHandler[i].c_str());
    }
    if (vHandler.empty()) {
        addCmdHandler("ch_common");
    }

    // 半透明值
    int nOpaque = settingGetOpaquePercent();
    setTransparent(opaquePercentToAlpha(nOpaque), settingGetClickThrough());

    CSkinNStatusButton *pBt;

    // maximize button status
    pBt = (CSkinNStatusButton *)getUIObjectById(ID_MAXIMIZE, CSkinNStatusButton::className());
    if (pBt && isZoomed()) {
        pBt->setStatus(1);
    }

    // topmost button status
    pBt = (CSkinNStatusButton *)getUIObjectById(ID_TOPMOST, CSkinNStatusButton::className());
    if (pBt && isTopmost()) {
        pBt->setStatus(1);
    }
}

void CMPSkinWnd::onAddUIObj(CUIObject *pObj) {
    CSkinWnd::onAddUIObj(pObj);

    int nID = pObj->getID();
    if (nID == ID_PLAYPAUSE) {
        if (pObj->isKindOf(CSkinNStatusButton::className())) {
            CPlayerPlayPauseBtHandler *pHandler = new CPlayerPlayPauseBtHandler((CSkinNStatusButton*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_SEEK) {
        if (pObj->isKindOf(CSkinSeekCtrl::className())) {
            CPlayerSeekEventHandler *pHandler = new CPlayerSeekEventHandler((CSkinSeekCtrl*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_VOLUME) {
        if (pObj->isKindOf(CSkinSeekCtrl::className())) {
            CPlayerVolumeEventHandler *pHandler = new CPlayerVolumeEventHandler((CSkinSeekCtrl*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_SHUFFLE) {
        if (pObj->isKindOf(CSkinNStatusButton::className())) {
            CPlayerShuffleEventHandler *pHandler = new CPlayerShuffleEventHandler((CSkinNStatusButton*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_LOOP) {
        if (pObj->isKindOf(CSkinNStatusButton::className())) {
            CPlayerLoopEventHandler *pHandler = new CPlayerLoopEventHandler((CSkinNStatusButton*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_RATE) {
        if (pObj->isKindOf(CSkinRateCtrl::className())) {
            CPlayerMediaChangedRateEventHandler *pHandler = new CPlayerMediaChangedRateEventHandler((CSkinRateCtrl*)pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else if (nID == ID_STEREO_STAT) {
        if (pObj->isKindOf(CSkinNStatusImage::className())) {
            CMediaStereoStatusEventHandler *pHandler = new CMediaStereoStatusEventHandler(pObj);
            m_mapEventHandlers[pObj] = pHandler;
        }
    } else {
        if (pObj->isKindOf(CSkinToolbar::className())) {
            CSkinToolbar *pToolBar = (CSkinToolbar*)pObj;

            if (pToolBar->isButtonExist(ID_PLAYPAUSE)) {
                CPlayerPlayPauseToolbarHandler *pHandler = new CPlayerPlayPauseToolbarHandler(pToolBar);
                m_mapEventHandlers[pObj] = pHandler;
            }

            if (pToolBar->isButtonExist(ID_SHUFFLE) || pToolBar->isButtonExist(ID_LOOP)) {
                CPlayerShuffleRepeatToolbarHandler *pHandler = new CPlayerShuffleRepeatToolbarHandler(pToolBar);
                m_mapEventHandlers[pObj] = pHandler;
            }
        }
    }
}

void CMPSkinWnd::onRemoveUIObj(CUIObject *pObj) {
    MAP_EVENT_HANDLER::iterator it;
    it = m_mapEventHandlers.find(pObj);
    if (it != m_mapEventHandlers.end()) {
        IEventHandler *pHandler = (*it).second;
        pHandler->unregisterHandler();
        m_mapEventHandlers.erase(it);
        delete pHandler;
    }

    CSkinWnd::onRemoveUIObj(pObj);
}

bool CMPSkinWnd::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (strcasecmp(szProperty, "CmdHandler") == 0) {
        m_strCmdHandler = szValue;
    } else {
        return CSkinWnd::setProperty(szProperty, szValue);
    }

    return true;
}

void CMPSkinWnd::closeSkin() {
    if (!m_bSkinOpened) {
        return;
    }

    CSkinWnd::closeSkin();

    LIST_SKINCMDHANDLER::iterator it, itEnd;

    itEnd = m_listSkinCmdHandler.end();
    for (it = m_listSkinCmdHandler.begin(); it != itEnd; ++it) {
        ISkinCmdHandler *pHandler = *it;
        delete pHandler;
    }
    m_listSkinCmdHandler.clear();
    m_strCmdHandler.resize(0);
}

// IUICheckStatus interface
bool CMPSkinWnd::getChecked(uint32_t nID, bool &bChecked) {
    switch (nID) {
    case ID_TOPMOST:
        bChecked = isTopmost();
        break;
    case ID_CLICK_THROUGH:
        bChecked = m_bClickThrough;
        break;
    default:
        {
            LIST_SKINCMDHANDLER::iterator it, itEnd;

            itEnd = m_listSkinCmdHandler.end();
            for (it = m_listSkinCmdHandler.begin(); it != itEnd; ++it) {
                ISkinCmdHandler *p = *it;
                if (p->getChecked(nID, bChecked)) {
                    return true;
                }
            }
        }
        return false;
    }

    return true;
}

bool CMPSkinWnd::getRadioChecked(vector<uint32_t> &vIDs, uint32_t &nIDChecked) {
    assert(!vIDs.empty());
    if (vIDs.empty()) {
        return false;
    }

    if (vIDs[0] == ID_SET_OPAQUE_100) {
        int nPercent = settingGetOpaquePercent();
        if (nPercent >= 95) nIDChecked = ID_SET_OPAQUE_100;
        else if (nPercent >= 85) nIDChecked = ID_SET_OPAQUE_90;
        else if (nPercent >= 75) nIDChecked = ID_SET_OPAQUE_80;
        else if (nPercent >= 65) nIDChecked = ID_SET_OPAQUE_70;
        else if (nPercent >= 55) nIDChecked = ID_SET_OPAQUE_60;
        else if (nPercent >= 45) nIDChecked = ID_SET_OPAQUE_50;
        else if (nPercent >= 35) nIDChecked = ID_SET_OPAQUE_40;
        else if (nPercent >= 25) nIDChecked = ID_SET_OPAQUE_30;
        else if (nPercent >= 15) nIDChecked = ID_SET_OPAQUE_20;
        else nIDChecked = ID_SET_OPAQUE_10;

        return true;
    } else if (vIDs[0] == ID_LDS_MULTI_LINE) {
        string name;

        CLyricShowAgentObj::getLyrDispStyleSettings(this, name);

        if (isPropertyName(name.c_str(), CLyricShowMultiRowObj::className())) {
            nIDChecked = ID_LDS_MULTI_LINE;
        } else if (isPropertyName(name.c_str(), SZ_TXT_LYR_CONTAINER)) {
            nIDChecked = ID_LDS_STATIC_TXT;
        } else if (isPropertyName(name.c_str(), CLyricShowTwoRowObj::className())) {
            nIDChecked = ID_LDS_TWO_LINE;
        } else if (isPropertyName(name.c_str(), CLyricShowSingleRowObj::className())) {
            nIDChecked = ID_LDS_SINGLE_LINE;
        } else if (isPropertyName(name.c_str(), CLyricShowVobSub::className())) {
            nIDChecked = ID_LDS_VOBSUB;
        } else {
            nIDChecked = ID_LDS_MULTI_LINE;
        }

        return true;
    } else {
        LIST_SKINCMDHANDLER::iterator it, itEnd;

        itEnd = m_listSkinCmdHandler.end();
        for (it = m_listSkinCmdHandler.begin(); it != itEnd; ++it) {
            ISkinCmdHandler *p = *it;
            if (p->getRadioChecked(vIDs, nIDChecked)) {
                return true;
            }
        }
    }

    return false;
}

void CMPSkinWnd::postCustomCommandMsg(int nId) {
    CSkinWnd::postCustomCommandMsg(nId);
}

void CMPSkinWnd::postShortcutKeyCmd(int nId) {
    CSkinWnd::postShortcutKeyCmd(nId);
}

void CMPSkinWnd::addCmdHandler(cstr_t szName) {
    ISkinCmdHandler *pHandler = nullptr;

    if (strcasecmp(szName, "ch_common") == 0) {
        pHandler = new CMPCommonCmdHandler();
    } else if (strcasecmp(szName, "ch_playlist") == 0) {
        pHandler = new CMPPlaylistCmdHandler();
    // } else if (strcasecmp(szName, "ch_medialib") == 0) {
    //     pHandler = new CMPMediaLibCmdHandler();
    // }
    //    else if (strcasecmp(szName, "ch_mediaguide") == 0)
    //        pHandler = new CMPCmdHandlerOfMediaGuide;
    } else if (strcasecmp(szName, "ch_floating_lyr") == 0) {
        pHandler = new CMPCommonCmdHandler(true);
    } else {
        ERR_LOG1("Unknow command handler: %s", szName);
        return;
    }

    pHandler->init(this);
    m_listSkinCmdHandler.push_back(pHandler);
}

#ifdef _WIN32
void CMPSkinWnd::onDropFiles(HDROP hDrop) {
    WCHAR szFile[MAX_PATH];
    int n = DragQueryFileW(hDrop, (uint32_t)-1, szFile, CountOf(szFile));

    if (n == 1) {
        DragQueryFileW(hDrop, 0, szFile, CountOf(szFile));

        auto fn = ucs2ToUtf8(szFile);
        if (fileIsExtSame(fn.c_str(), ".lrc") || fileIsExtSame(fn.c_str(), ".txt")) {
            g_LyricSearch.associateLyrics(g_player.getMediaKey().c_str(), fn.c_str());
            MPlayerApp::getInstance()->dispatchResearchLyrics();

            DragFinish(hDrop);
            return;
        }
    }

    g_player.clearNowPlaying();

    for (int i = 0; i < n; ++i) {
        DragQueryFileW(hDrop, i, szFile, CountOf(szFile));

        auto fn = ucs2ToUtf8(szFile);
        if (isDirExist(fn.c_str())) {
            g_player.addDirToNowPlaying(fn.c_str());
        } else {
            g_player.addToNowPlaying(fn.c_str());
        }
    }

    g_player.play();

    DragFinish(hDrop);
}
#endif

bool CMPSkinWnd::settingGetTopmost() {
    return g_profile.getBool(SZ_SECT_UI, "topmost", true);
}

int CMPSkinWnd::settingGetOpaquePercent() {
    return g_profile.getInt(SZ_SECT_UI, "WindowOpaquePercent", 100);
}

bool CMPSkinWnd::settingGetClickThrough() {
    return MPlayerApp::getMPSkinFactory()->getClickThrough();
}

void CMPSkinWnd::settingReverseTopmost() {
    bool bTopmost = !isTopmost();

    g_profile.writeInt(SZ_SECT_UI, "topmost", bTopmost);
    MPlayerApp::getMPSkinFactory()->topmostAll(bTopmost);
}

void CMPSkinWnd::settingSetOpaquePercent(int nPercent) {
    g_profile.writeInt(SZ_SECT_UI, "WindowOpaquePercent", nPercent);
    MPlayerApp::getMPSkinFactory()->allUpdateTransparent();
}

void CMPSkinWnd::settingReverseClickThrough() {
    MPlayerApp::getMPSkinFactory()->setClickThrough(!MPlayerApp::getMPSkinFactory()->getClickThrough());
}
