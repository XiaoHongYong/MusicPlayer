#include "../WindowLib.h"
#include "../../TinyJS/utils/CharEncoding.h"


bool toLocalMenu(HMENU hMenu) {
    if (g_LangTool.isEnglish() && !g_LangTool.hasMacro()) {
        return true;
    }

    // don't use get menu item count, under win ce, it will cost a lot time
    for (int i = 0; i < 256; i++) {
        MENUITEMINFOW info;
        utf16_t text[256];
        utf16_t szAccKey[256];

        memset(&info, 0, sizeof(info));

        info.cbSize = sizeof(info);
        info.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_DATA;
        info.dwTypeData = text;
        info.cch = CountOf(text);
        text[0] = '\0';

        // get menu item by pos.
        if (!GetMenuItemInfo(hMenu, i, true, &info)) {
            break;
        }

        if (info.hSubMenu != nullptr) {
            toLocalMenu(info.hSubMenu);
        }

        if (text[0] == '\0') {
            continue;
        }

        //
        // translate menu string
        //
        auto utf8Text = ucs2ToUtf8(text);
        string label, accText;
        if (!strSplit(utf8Text.c_str(), '\t', label, accText)) {
            label = utf8Text;
        }

        utf8Text = _TL(label.c_str());
        if (!accText.empty()) {
            utf8Text.push_back('\t');
            utf8Text.append(accText);
        }

        auto u16Text = utf8ToUCS2(utf8Text.c_str(), utf8Text.size());
        info.dwTypeData = (utf16_t *)u16Text.c_str();
        info.cch = u16Text.size() + 1;
        info.fMask = MIIM_DATA | MIIM_TYPE;
        if (!SetMenuItemInfoW(hMenu, i, true, &info)) {
            ERR_LOG1("SetMenuItemInfo: %s", getLastSysErrorDesc().c_str());
            continue;
        }
    }

    return true;
}

bool toLocalMenu(CMenu *pMenu) {
    return toLocalMenu(pMenu->getHandle());
}

//////////////////////////////////////////////////////////////////////

CMenu::CMenu() {
}

CMenu::CMenu(const CMenu &src) {
    m_hMenu = src.m_hMenu;
    m_nSubMenu = src.m_nSubMenu;
    m_bFree = false;
}

CMenu::~CMenu() {
    destroy();
}

bool CMenu::isValid() const {
    return m_hMenu != nullptr;
}

bool CMenu::loadMenu(int nID) {
    destroy();

    m_hMenu = ::LoadMenu(getAppInstance(), MAKEINTRESOURCE(nID));
    assert(m_hMenu);
    m_nSubMenu = -1;

    toLocalMenu(this);

    if (m_hMenu != nullptr) {
        onLoadMenu();
    }

    return m_hMenu != nullptr;
}

bool CMenu::createPopupMenu() {
    destroy();

    m_hMenu = ::CreatePopupMenu();

    return m_hMenu != nullptr;
}

void CMenu::destroy() {
    if (m_hMenu && m_bFree) {
        DestroyMenu(m_hMenu);
        m_hMenu = nullptr;
    }
}

void CMenu::trackPopupMenu(int x, int y, Window *pWnd, CRect *prcNotOverlap) {
    assert(pWnd);

    updateMenuStatus(pWnd);

    TPMPARAMS tpmParam;

    if (prcNotOverlap) {
        tpmParam.cbSize = sizeof(tpmParam);
        tpmParam.rcExclude = *prcNotOverlap;
    }

    TrackPopupMenuEx(getHandle(), TPM_LEFTALIGN, x, y, pWnd->getWndHandle(), prcNotOverlap ? &tpmParam : nullptr);
}

void CMenu::trackPopupSubMenu(int x, int y, int nSubMenu, Window *pWnd, CRect *prcNotOverlap) {
    assert(pWnd);

    updateMenuStatus(pWnd);

    TPMPARAMS tpmParam;

    if (prcNotOverlap) {
        tpmParam.cbSize = sizeof(tpmParam);
        tpmParam.rcExclude = *prcNotOverlap;
    }

    TrackPopupMenuEx(getSubMenuHandle(nSubMenu), TPM_LEFTALIGN | TPM_VERTICAL, x, y, pWnd->getWndHandle(), prcNotOverlap ? &tpmParam : nullptr);
}

void CMenu::enableItem(int nID, bool bEnable) {
    HMENU hMenu = getHandle();

    EnableMenuItem(hMenu, nID, bEnable ? MF_ENABLED : MF_GRAYED);
}

void CMenu::checkItem(int nID, bool bCheck) {
    HMENU hMenu = getHandle();

    CheckMenuItem(hMenu, nID, bCheck ? MF_CHECKED : MF_UNCHECKED);
}

void CMenu::checkRadioItem(int nID, int nStartID, int nEndID) {
    HMENU hMenu = getHandle();

    CheckMenuRadioItem(hMenu, nStartID, nEndID, nID, MF_BYCOMMAND);
}

int CMenu::getItemCount() {
    return ::GetMenuItemCount(getHandle());
}

void CMenu::appendItem(uint32_t nID, cstr_t text, cstr_t shortcutKey) {
    utf16string u16Text = utf8ToUCS2(text);

    if (!isEmptyString(shortcutKey)) {
        u16Text.push_back('\t');
        u16Text.append(utf8ToUCS2(shortcutKey));
    }

    ::AppendMenuW(getHandle(), MF_STRING, nID, u16Text.c_str());
}

void CMenu::appendSeperator() {
    AppendMenu(getHandle(), MF_SEPARATOR, 0, nullptr);
}

void CMenu::insertItem(int nPos, uint32_t nID, cstr_t text, cstr_t shortcutKey) {
    utf16string u16Text = utf8ToUCS2(text);

    if (!isEmptyString(shortcutKey)) {
        u16Text.push_back('\t');
        u16Text.append(utf8ToUCS2(shortcutKey));
    }

    ::InsertMenuW(getHandle(), nPos, MF_BYPOSITION, nID, u16Text.c_str());
}

HMENU findMenuPos(HMENU hMenu, UINT nIDMenu) {
    MENUITEMINFO mii;

    // don't use get menu item count, under win ce, it will cost a lot time
    for (int i = 0; i < 256; i++) {
        memset(&mii, 0, sizeof(mii));

        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_ID;
        mii.dwTypeData = NULL;
        mii.cch = 0;

        // get menu item by pos.
        if (!::GetMenuItemInfo(hMenu, i, TRUE, &mii)) {
            break;
        }

        if (mii.hSubMenu != NULL) {
            HMENU h = findMenuPos(mii.hSubMenu, nIDMenu);
            if (h != NULL) {
                return h;
            }
        } else {
            if (mii.wID == nIDMenu) {
                return hMenu;
            }
        }
    }

    return NULL;
}

void CMenu::insertItemByID(uint32_t nPosID, uint32_t nID, cstr_t text, cstr_t shortcutKey) {
    HMENU hMenu = findMenuPos(getHandle(), nPosID);
    assert(hMenu);
    if (hMenu == nullptr) {
        return;
    }

    utf16string u16Text = utf8ToUCS2(text);

    if (!isEmptyString(shortcutKey)) {
        u16Text.push_back('\t');
        u16Text.append(utf8ToUCS2(shortcutKey));
    }

    ::InsertMenu(hMenu, nPosID, MF_BYCOMMAND, nID, u16Text.c_str());
}

void CMenu::insertSeperator(int nPos) {
    ::InsertMenu(getHandle(), nPos, MF_SEPARATOR, 0, nullptr);
}

CMenu CMenu::appendSubmenu(cstr_t text) {
    HMENU hSubMenu = ::CreatePopupMenu();
    ::AppendMenuW(getHandle(), MF_POPUP, (UINT_PTR)hSubMenu, utf8ToUCS2(text).c_str());

    CMenu menu;
    menu.attach(hSubMenu, false);

    return menu;
}

CMenu CMenu::insertSubmenu(int nPos, cstr_t text) {
    HMENU hSubMenu = ::CreatePopupMenu();
    ::InsertMenuW(getHandle(), nPos, MF_BYPOSITION | MF_POPUP,
        (UINT_PTR)hSubMenu, utf8ToUCS2(text).c_str());

    CMenu menu;
    menu.attach(hSubMenu, false);

    return menu;
}


void CMenu::removeItem(int nPos) {
    ::DeleteMenu(getHandle(), nPos, MF_BYPOSITION);
}

void CMenu::removeAllItems() {
    int count = getItemCount();
    for (count--; count >= 0; count--) {
        removeItem(count);
    }
}

bool CMenu::getMenuItemText(uint32_t index, string &text, bool byPosition) {
    MenuItemInfo info = { 0 };

    if (!getMenuItemInfo(index, byPosition, info)) {
        return false;
    }
    text = info.text;

    return true;
}

bool CMenu::getMenuItemInfo(uint32_t index, bool byPosition, MenuItemInfo &itemOut) {
    MENUITEMINFOW info = { 0 };
    utf16_t text[256];

    info.cbSize = sizeof(info);
    info.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_DATA | MIIM_ID | MIIM_STATE;
    info.dwTypeData = text;
    info.cch = CountOf(text);
    text[0] = '\0';

    // get menu item by pos.
    if (!::GetMenuItemInfoW(getHandle(), index, byPosition, &info)) {
        return false;
    }

    auto state = GetMenuState(getHandle(), index, MF_BYPOSITION);

    // Win32 菜单把加速键写在标题里：Label\tCtrl+O
    string utf8Text = ucs2ToUtf8(text);
    string label, shortcut;
    if (strSplit(utf8Text.c_str(), '\t', label, shortcut)) {
        itemOut.text = std::move(label);
        itemOut.shortcutKey = std::move(shortcut);
    } else {
        itemOut.text = std::move(utf8Text);
        itemOut.shortcutKey.clear();
    }
    itemOut.id = info.wID;
    itemOut.isSubmenu = info.hSubMenu != nullptr;
    itemOut.isSeparator = state & MF_SEPARATOR;
    itemOut.isChecked = state & MF_CHECKED;
    itemOut.isEnabled = !(state & MF_DISABLED);

    return true;
}

bool CMenu::isSeparator(int pos) {
    auto state = GetMenuState(getHandle(), pos, MF_BYPOSITION);
    return state & MF_SEPARATOR;
}

bool CMenu::loadPopupMenu(int nID, int nSubMenu) {
    destroy();

    m_hMenu = ::LoadMenu(getAppInstance(), MAKEINTRESOURCE(nID));
    assert(m_hMenu);
    m_nSubMenu = nSubMenu;

    toLocalMenu(::GetSubMenu(m_hMenu, m_nSubMenu));

    if (m_hMenu != nullptr) {
        onLoadMenu();
    }

    return m_hMenu != nullptr;
}

bool CMenu::hasItem(int nPos) {
    MENUITEMINFO mii;

    memset(&mii, 0, sizeof(mii));

    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_TYPE;
    mii.dwTypeData = nullptr;
    mii.cch = 0;

    // get menu item by pos.
    if (!::GetMenuItemInfo(getHandle(), nPos, true, &mii)) {
        return false;
    }

    return true;
}

bool CMenu::hasSubmenu(int nPos) {
    return getSubMenuHandle(nPos) != nullptr;
}

CMenu CMenu::getSubmenu(int nPos) {
    CMenu menu;

    menu.attach(getSubMenuHandle(nPos), false);

    return menu;
}

CMenu CMenu::getSubmenuByPlaceHolderID(uint32_t id) {
    int count = getItemCount();
    for (int i = 0; i < count; i++) {
        if (hasSubmenu(i)) {
            CMenu menu = getSubmenu(i);
            MenuItemInfo info;
            if (menu.getMenuItemInfo(0, true, info) && info.id == id) {
                return menu;
            }
        }
    }

    return CMenu();
}

HMENU CMenu::getSubMenuHandle(int nPos) {
    return ::GetSubMenu(getHandle(), nPos);
}

HMENU CMenu::getHandle() {
    if (m_nSubMenu == -1) {
        return m_hMenu;
    } else {
        return ::GetSubMenu(m_hMenu, m_nSubMenu);
    }
}

CMenu & CMenu::operator = (const CMenu &menu) {
    m_hMenu = menu.m_hMenu;
    m_nSubMenu = menu.m_nSubMenu;
    m_bFree = false;

    return *this;
}
