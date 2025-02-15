﻿#include "../WindowLib.h"
#include <commdlg.h>


// #define FLAG_BOLD        1
// #define FLAG_ITALIC        (0x1 << 1)
//


CDlgChooseFont::CDlgChooseFont() {
}

CDlgChooseFont::~CDlgChooseFont() {

}

int CDlgChooseFont::doModal(Window *pWndParent, cstr_t szFontFaceName, int nFontSize, int nWeight, int nItalic) {
    LOGFONT lgfont = { 0 };
    CHOOSEFONTW choosefont = { 0 };

    m_strFontFaceName = szFontFaceName;
    m_nFontSize = nFontSize;
    m_weight = nWeight;
    m_nItalic = nItalic;

    lgfont.lfHeight = nFontSize;
    lgfont.lfWeight = nWeight;
    lgfont.lfItalic = nItalic;

    utf16string u16FaceName = utf8ToUCS2(szFontFaceName);
    wcscpy_s(lgfont.lfFaceName, CountOf(lgfont.lfFaceName), u16FaceName.c_str());

    choosefont.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCALABLEONLY | CF_SCREENFONTS | CF_NOVERTFONTS;// | CF_EFFECTS;
    choosefont.lpLogFont = &lgfont;
    choosefont.hwndOwner = pWndParent->getWndHandle();
    choosefont.lStructSize = sizeof(choosefont);
    if (ChooseFontW(&choosefont)) {
        m_strFontFaceName = ucs2ToUtf8(lgfont.lfFaceName);
        m_nFontSize = lgfont.lfHeight;
        m_weight = lgfont.lfWeight;
        m_nItalic = lgfont.lfItalic;

        return IDOK;
    }

    return IDCANCEL;
}

cstr_t CDlgChooseFont::getFaceName() {
    return m_strFontFaceName.c_str();
}

int CDlgChooseFont::getSize() {
    return m_nFontSize;
}

int CDlgChooseFont::getWeight() {
    return m_weight;
}

int CDlgChooseFont::getItalic() {
    return m_nItalic;
}
