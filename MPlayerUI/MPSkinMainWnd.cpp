// MPSkinMainWnd.cpp: implementation of the CMPSkinMainWndBase class.
//
//////////////////////////////////////////////////////////////////////

#include "MPlayerApp.h"
#include "MPSkinMainWnd.h"
#include "DlgAbout.h"
#include "VersionUpdate.h"

//////////////////////////////////////////////////////////////////////

CMPSkinMainWndBase::CMPSkinMainWndBase()
{
}

CMPSkinMainWndBase::~CMPSkinMainWndBase()
{
}

void CMPSkinMainWndBase::onCreate()
{
    CMPSkinWnd::onCreate();

    updateCaptionText();
}

void CMPSkinMainWndBase::onDestroy()
{
    IEventHandler::unregisterHandler();

#ifdef _WIN32
    // save iconic status
    g_profile.writeInt("Minimized", isIconic());
#endif

    CMPSkinWnd::onDestroy();
}

void CMPSkinMainWndBase::onEvent(const IEvent *pEvent)
{
    if (pEvent->eventType == ET_UI_SETTINGS_CHANGED)
    {
        if (isPropertyName(pEvent->name.c_str(), "topmost"))
            CMPlayerAppBase::getMPSkinFactory()->topmostAll(isTRUE(pEvent->strValue.c_str()));
        else
            CMPSkinWnd::onEvent(pEvent);
    }
    else
        CMPSkinWnd::onEvent(pEvent);
}

void CMPSkinMainWndBase::updateCaptionText()
{
    cstr_t szAppName;
    if (CMPlayerApp::getInstance()->getCurrentAppMode() == SA_IPOD_LYRICS_DOWNLOADER)
        szAppName = _TLM("Lyrics Downloader for iPod");
    else
        szAppName = SZ_APP_NAME;

    setCaptionText(_TL(szAppName));
}
