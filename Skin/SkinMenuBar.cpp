#include "SkinTypes.h"
#include "Skin.h"
#include "SkinMenuBar.h"


#define SPACE_SUBMENUS        (m_font.getHeight()   / 2 + 5)

UIOBJECT_CLASS_NAME_IMP(CSkinMenuBar, "MenuBar")

CSkinMenuBar::CSkinMenuBar() {
    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON | UO_MSG_WANT_MENU_KEY
        | UO_MSG_WANT_MOUSEWHEEL | UO_MSG_WANT_ALL_KEYS;

    m_isShowingMenu = false;

    m_nSelSubMenu = -1;

    m_bOutlinedHover = false;
    m_bOutlinedPressed = false;

    m_clrPressed.set(0);
    m_clrBgPressed.set(0);
    m_clrBgPressed.setAlpha(128);

    m_pMenu = nullptr;
    m_ptLast.x = m_ptLast.y = 0;

    m_popupMenu = nullptr;
}

CSkinMenuBar::~CSkinMenuBar() {
}

void CSkinMenuBar::onCreate() {
    CUIObject::onCreate();

    m_font.setParent(m_pSkin);

    m_popupMenu = (SkinMenuItemsContainer *)m_pSkin->createUIObject("MenuItemsContainer", m_pSkin->getRootContainer());
    assert(m_popupMenu);
    m_popupMenu->setParentMsgReceiver(this);
    m_pSkin->getRootContainer()->addUIObject(m_popupMenu);
    m_popupMenu->setVisible(false, false);

    m_cursorArrow.loadStdCursor(Cursor::C_ARROW);

    onLanguageChanged();
}

void CSkinMenuBar::draw(CRawGraph *canvas) {
    if (m_font.isGood()) {
        canvas->setFont(m_font.getFont());
    }

    CRect rcItem = m_rcObj;
    int x = m_rcObj.left;

    for (int i = 0; i < (int)m_vSubMenus.size(); i++) {
        SubMenu &item = m_vSubMenus[i];
        CSize size;

        canvas->getTextExtentPoint32(item.text.c_str(), item.text.size(), &size);
        item.nWidth = size.cx + SPACE_SUBMENUS;

        if (x + item.nWidth > m_rcObj.right) {
            break;
        }

        rcItem.left = x;
        rcItem.right = x + item.nWidth;

        CColor clrText, clrOutlined;
        bool bOutlined;

        if (i == m_nSelSubMenu && m_isShowingMenu) {
            clrText = m_clrPressed;
            clrOutlined = m_clrOutlinedPressed;
            bOutlined = m_bOutlinedPressed;
            canvas->fillRect(rcItem, m_clrBgPressed, BPM_OP_BLEND | BPM_CHANNEL_RGB);
        } else {
            clrText = m_font.getTextColor(m_enable);
            clrOutlined = m_font.getColorOutlined();
            bOutlined = m_font.isOutlined();
        }

        if (bOutlined) {
            canvas->drawTextOutlined(item.text.c_str(), item.text.size(),
                rcItem, clrText, clrOutlined, DT_VCENTER | DT_CENTER | DT_PREFIX_TEXT | DT_SINGLELINE);
        } else {
            canvas->setTextColor(clrText);
            canvas->drawText(item.text.c_str(), item.text.size(),
                rcItem, DT_VCENTER | DT_CENTER | DT_PREFIX_TEXT | DT_SINGLELINE);
        }

        x += item.nWidth;
    }

    if (isMenuPopup()) {
        // Need mouse wheel message only when child menu is popup.
        m_msgNeed |= UO_MSG_WANT_MOUSEWHEEL;
    } else {
        m_msgNeed &= ~UO_MSG_WANT_MOUSEWHEEL;
    }
}

bool CSkinMenuBar::onLButtonDown(uint32_t nFlags, CPoint point) {
    int index = hitTestSubMenu(point);
    if (index == -1) {
        if (m_popupMenu && m_popupMenu->isDealingMouseMove(point)) {
            // 鼠标在子菜单中，不关闭子菜单
            return false;
        }

        onMenuItemSelected(index);
        return false;
    }

    if (m_vSubMenus[index].isSubmenu) {
        if (index == m_nSelSubMenu) {
            // 相同的 menu 再点击一次，则隐藏菜单
            hideSubMenu();
        } else {
            onMenuItemSelected(index);
        }
    } else {
        m_pSkin->postCustomCommandMsg(m_vSubMenus[index].id);
        hideSubMenu();
    }

    return index != -1;
}

bool CSkinMenuBar::onMouseMove(CPoint point) {
#ifdef WIN32
    // 在 windows 下，当 setCatpureMouse 后，鼠标指针无法被正常更新
    setCursor(m_cursorArrow);
#endif

    if (point == m_ptLast) {
        return false;
    }
    m_ptLast = point;

    int index = hitTestSubMenu(point);
    if (m_isShowingMenu && index != m_nSelSubMenu) {
        if (index == -1) {
            // 不关闭子菜单
            return false;
        }

        onMenuItemSelected(index);
    }

    return index != -1;
}

void CSkinMenuBar::onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) {
    if (isMenuPopup()) {
        m_popupMenu->onMouseWheel(nWheelDistance, nMkeys, pt);
    }
}

bool CSkinMenuBar::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (m_nSelSubMenu == -1) {
        return false;
    }

    if (nChar == VK_ESCAPE && isMenuPopup()) {
        hideSubMenu();
        return true;
    }

    if (isMenuPopup()) {
        return m_popupMenu->onKeyDown(nChar, nFlags);
    }

    switch (nChar) {
        case VK_LEFT:
            onMenuItemSelected(m_nSelSubMenu > 0 ? m_nSelSubMenu  - 1 : (int)m_vSubMenus.size() - 1);
            return true;
        case VK_RIGHT:
            onMenuItemSelected(m_nSelSubMenu >= (int)m_vSubMenus.size() - 1 ? 0 : m_nSelSubMenu + 1);
            return true;
    }

    return false;
}

void CSkinMenuBar::onPopupMenuClosed() {
    m_isShowingMenu = false;
    m_nSelSubMenu = -1;
    m_pSkin->releaseCaptureMouse(this);
    m_pSkin->setSkinMenuPopup(false);
}

void CSkinMenuBar::onPopupKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (nChar == VK_LEFT) {
        onMenuItemSelected(m_nSelSubMenu > 0 ? m_nSelSubMenu  - 1 : (int)m_vSubMenus.size() - 1);
    } else if (nChar == VK_RIGHT) {
        onMenuItemSelected(m_nSelSubMenu >= (int)m_vSubMenus.size() - 1 ? 0 : m_nSelSubMenu + 1);
    }
}

int CSkinMenuBar::getPopupTopY() {
    return m_rcObj.bottom;
}

bool CSkinMenuBar::onMenuKey(uint32_t nChar, uint32_t nFlags) {
    for (int i = 0; i < (int)m_vSubMenus.size(); i++) {
        if (toUpper(m_vSubMenus[i].chMenuKey) == toUpper(nChar)) {
            if (m_vSubMenus[i].isSubmenu) {
                onMenuItemSelected(i);
            } else {
                m_nSelSubMenu = i;
                m_pSkin->postCustomCommandMsg(m_vSubMenus[m_nSelSubMenu].id);
            }
            return true;
        }
    }

    return false;
}

void CSkinMenuBar::onKillFocus() {
    onMenuItemSelected(-1);
}

bool CSkinMenuBar::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (isPropertyName(szProperty, "Menu")) {
        m_strMenu = szValue;
    } else if (m_font.setProperty(szProperty, szValue)) {
        return true;
    } else if (isPropertyName(szProperty, "TextColorPressed")) {
        getColorValue(m_clrPressed, szValue);
    } else if (isPropertyName(szProperty, "TextOutlinedColorPressed")) {
        getColorValue(m_clrOutlinedPressed, szValue);
        m_bOutlinedPressed = true;
    } else if (isPropertyName(szProperty, "BgColorPressed")) {
        getColorValue(m_clrBgPressed, szValue);
    } else if (isPropertyName(szProperty, "BgPressedAlpha")) {
        m_clrBgPressed.setAlpha(atoi(szValue));
    } else {
        return false;
    }

    return true;
}


#ifdef _SKIN_EDITOR_
void CSkinMenuBar::enumProperties(CUIObjProperties &listProperties) {
    CUIObject::enumProperties(listProperties);
}
#endif // _SKIN_EDITOR_


void CSkinMenuBar::onLanguageChanged() {
    m_vSubMenus.clear();

    if (m_strMenu.size()) {
        m_pMenu = m_pSkin->getSkinFactory()->loadMenu(m_pSkin, m_strMenu.c_str());
    } else if (!isEmptyString(m_pSkin->getMenuName())) {
        m_pMenu = m_pSkin->getSkinFactory()->loadMenu(m_pSkin,
            m_pSkin->getMenuName());
    }

    if (m_pMenu) {
        int count = m_pMenu->getItemCount();
        for (int i = 0; i < count; i++) {
            SubMenu item;

            if (m_pMenu->getMenuItemInfo(i, true, item)) {
                item.chMenuKey = getMenuKey(item.text.c_str());
                m_vSubMenus.push_back(item);
            }
        }
    }
}

int CSkinMenuBar::hitTestSubMenu(const CPoint point) {
    if (!m_rcObj.ptInRect(point)) {
        return -1;
    }

    int x = m_rcObj.left;

    for (int i = 0; i < (int)m_vSubMenus.size(); i++) {
        x += m_vSubMenus[i].nWidth;
        if (x > m_rcObj.right) {
            return -1;
        }

        if (point.x < x) {
            return i;
        }
    }

    return -1;
}


int CSkinMenuBar::getItemX(int n) {
    int x = m_rcObj.left;

    if (n >= (int)m_vSubMenus.size()) {
        n = (int)m_vSubMenus.size() - 1;
    }
    for (int i = 0; i < n; i++) {
        x += m_vSubMenus[i].nWidth;
        if (x > m_rcObj.right) {
            return x;
        }
    }

    return x;
}

void CSkinMenuBar::onMenuItemSelected(int index) {
    if (m_nSelSubMenu == index) {
        // 相同未变
        return;
    }

    m_nSelSubMenu = index;

    if (index == -1) {
        hideSubMenu();
        return;
    }

    if (m_vSubMenus[m_nSelSubMenu].isSubmenu) {
        // 弹出子 menu
        m_isShowingMenu = true;
        if (m_pSkin->getCaptureMouse() != this) {
            // 弹出菜单，需要一直关注鼠标输入.
            m_pSkin->setCaptureMouse(this);
            m_pSkin->setSkinMenuPopup(true);
        }

        m_pMenu->updateMenuStatus(m_pSkin);
        CMenu menu = m_pMenu->getSubmenu(m_nSelSubMenu);
        m_popupMenu->m_rcObj.left = getItemX(m_nSelSubMenu);
        m_popupMenu->m_rcObj.top = m_rcObj.bottom;
        m_popupMenu->setMenu(menu);
        m_popupMenu->setVisible(true, false);
        m_popupMenu->activateMenu();
        m_pSkin->invalidateRect();
    } else {
        hideSubMenu();
    }
}

void CSkinMenuBar::hideSubMenu() {
    if (m_isShowingMenu) {
        onPopupMenuClosed();
        m_popupMenu->setVisible(false, false);
        m_pSkin->invalidateRect();
    }
}

bool CSkinMenuBar::isMenuPopup() {
    return m_popupMenu && m_popupMenu->isVisible();
}
