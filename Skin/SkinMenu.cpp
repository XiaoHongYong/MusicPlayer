#include "SkinTypes.h"
#include "Skin.h"
#include "SkinMenu.h"


//////////////////////////////////////////////////////////////////////////

int CSkinMenu::Item::fromXML(CSkinFactory *pSkinFactory, SXNode *pNode) {
    // <Item Action="append" Type="item" Name="Playlist Editor..." IDCmd="dyncmd_openclose_playlist_wnd" CanCheck="true" IsCheckedCmd="dyncmd_is_mediainfo_bar_visible"/>
    cstr_t szValue;

    szValue = pNode->getPropertySafe("Type");
    if (strcasecmp(szValue, "item") == 0) {
        type = IT_ITEM;
    } else if (strcasecmp(szValue, "popup") == 0) {
        type = IT_POPUP;
    } else if (strcasecmp(szValue, "seperator") == 0) {
        type = IT_SEPERATOR;
    } else {
        type = IT_ITEM;
    }

    szValue = pNode->getPropertySafe("Action");
    if (strcasecmp(szValue, "append") == 0 || strcasecmp(szValue, "InsertByID") == 0) {
        if (strcasecmp(szValue, "append") == 0) {
            action = A_APPEND;
        } else {
            action = A_INSERT_BY_ID;
            nPos = pSkinFactory->getIDByName(pNode->getPropertySafe("Position"));
        }

        name = pNode->getPropertySafe("Name");
        bCanCheck = isTRUE(pNode->getPropertySafe("CanCheck"));
        nIDCmd = pSkinFactory->getIDByName(pNode->getPropertySafe("IDCmd"));
        nIDCmdIsChecked = pSkinFactory->getIDByName(pNode->getPropertySafe("IsCheckedCmd"));
    } else if (strcasecmp(szValue, "remove") == 0) {
        action = A_REMOVE;
        nPos = pNode->getPropertyInt("Position");
    } else {
        action = A_UNKNOWN;
    }

    return ERR_OK;
}

CSkinMenu::CSkinMenu() {
    m_nOrgAppenPos = 0;
    m_nAppendPosition = 0;
}

CSkinMenu::~CSkinMenu() {

}

int CSkinMenu::loadMenu(const rapidjson::Value &items) {
    createPopupMenu();

    loadMenu(items, *this);

    onLoadMenu();

    return ERR_OK;
}

void CSkinMenu::loadMenu(const rapidjson::Value &items, CMenu &menu) {
    if (!items.IsArray()) {
        return;
    }

    for (int i = 0; i < items.Size(); i++) {
        auto &item = items[i];
        if (!item.IsArray() || item.Empty() || !item[0].IsString()) {
            continue;
        }

        if (item.Size() == 1) {
            menu.appendSeperator();
            continue;
        }

        if (item[1].IsArray()) {
            CMenu subMenu = menu.appendSubmenu(_TL(item[0].GetString()));
            loadMenu(item[1], subMenu);
            continue;
        }

        string name = item[0].GetString(), shortcut;
        int id = ID_INVALID;
        if (item[1].IsString()) {
            id = CSkinApp::getInstance()->getSkinFactory()->getIDByName(item[1].GetString());
        } else if (item[1].IsInt()) {
            id = item[1].GetInt();
        }
        if (id == ID_INVALID) {
            continue;
        }
        getShortcutKey(id, shortcut);
        menu.appendItem(id, _TL(name.c_str()), shortcut.c_str());
    }
}

void CSkinMenu::updateMenuStatus(Window *window) {
    CSkinFactory *pSkinFactory = CSkinApp::getInstance()->getSkinFactory();

    for (Item &item : m_listItems) {
        if (item.nIDCmdIsChecked != ID_INVALID && item.bCanCheck) {
            int ret = pSkinFactory->onDynamicCmd(item.nIDCmdIsChecked, (CSkinWnd *)window);
            checkItem(item.nIDCmd, ret == ERR_OK);
        }
    }
}

int CSkinMenu::fromXML(SXNode *pNodeMenu, int nAppendPos) {
    if (!pNodeMenu) {
        return ERR_NOT_FOUND;
    }

    assert(strcasecmp(pNodeMenu->name.c_str(), "Menu") == 0);

    if (!isValid()) {
        createPopupMenu();
    }

    m_nAppendPosition = nAppendPos;

    SXNode::iterator it, itEnd;
    itEnd = pNodeMenu->listChildren.end();
    for (it = pNodeMenu->listChildren.begin(); it != itEnd; ++it) {
        SXNode *pNode = *it;

        // OK, found the control
        if (strcasecmp(pNode->name.c_str(), "Item") == 0) {
            Item item;
            item.fromXML(CSkinApp::getInstance()->getSkinFactory(), pNode);

            m_listItems.push_back(item);
            if (item.action == A_APPEND) {
                insertItem(m_nAppendPosition, item.nIDCmd,
                    _TL(item.name.c_str()));
                m_nAppendPosition++;
            } else if (item.action == A_REMOVE) {
                removeItem(item.nPos);
                if (item.nPos < m_nAppendPosition) {
                    m_nAppendPosition--;
                }
            } else  if (item.action == A_INSERT_BY_ID) {
                insertItemByID(item.nPos, item.nIDCmd,
                    _TL(item.name.c_str()));
            }
        }
    }

    return ERR_OK;
}

int CSkinMenu::getSubMenuPos(int nSubMenu) const {
    if (nSubMenu > m_nOrgAppenPos) {
        return nSubMenu + m_nAppendPosition - m_nOrgAppenPos;
    } else {
        return nSubMenu;
    }
}
