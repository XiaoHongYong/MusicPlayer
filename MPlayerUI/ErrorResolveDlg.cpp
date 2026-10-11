#include "MPlayerApp.h"
#include "ErrorResolveDlg.h"
#include "Helper.h"
#include "PreferenceDlg.h"


void showInetErrorDlg(Window *pWnd, int nError) {
    string strError;
    char szErrorId[256];

    snprintf(szErrorId, CountOf(szErrorId), "%s %d", _TLT("Error Code:"), nError);
    strError = szErrorId;
    strError += "  ";
    strError += ERROR2STR_LOCAL(nError);

    pWnd->messageOut(strError.c_str(), MB_ICONINFORMATION | MB_OK);
}
