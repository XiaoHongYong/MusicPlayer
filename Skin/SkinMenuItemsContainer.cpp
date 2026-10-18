//
//  SkinMenuItemsContainer.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/12.
//

#include "SkinTypes.h"
#include "Skin.h"
#include "SkinMenuItemsContainer.hpp"
#include "SkinWnd.h"
#include "SkinFactory.h"


const int32_t MIN_ITEM_WIDTH = 100;
// 菜单标题与右侧快捷键之间的间距
const int32_t SHORTCUT_KEY_SEP = 32;

const int IDX_UP_ARROW = -3;
const int IDX_DOWN_ARROW = -2;
const int IDX_INVALID = -1;

UIOBJECT_CLASS_NAME_IMP(SkinMenuItemsContainer, "MenuItemsContainer")

SkinMenuItemsContainer::SkinMenuItemsContainer() {
    m_msgNeed = UO_MSG_WANT_MOUSEMOVE | UO_MSG_WANT_LBUTTON;
    m_layoutParams |= LAYOUT_HEIGHT_WRAP_CONENT;

    m_childMenu = nullptr;
    m_parentMsgReceiver = nullptr;

    m_xPadding = 20;
    m_sepYPadding = 4;
    m_itemHeight = 25;

    m_selectedItem = -1;

    m_clrHoverText.set(RGB(0, 0, 0));
    m_clrHoverBg.set(RGB(0x55, 0x8f, 0xf3));
    m_clrInactiveBg.set(RGB(0xE0, 0xE0, 0xE0));

    m_penFrame.createSolidPen(1, RGB(0xA0, 0xA0, 0xA0));
}

SkinMenuItemsContainer::~SkinMenuItemsContainer() {
}

void SkinMenuItemsContainer::onCreate() {
    CUIObject::onCreate();

    m_font.setParent(m_pSkin);
}

void SkinMenuItemsContainer::onMeasureSizeByContent() {
    CRawGraph *canvas = m_pContainer->getMemGraph();

    if (m_font.isGood()) {
        canvas->setFont(m_font.getFont());
    }

    m_hasChecked = false;
    m_hasSubmenu = false;
    m_isShowArrow = false;
    m_maxShortcutWidth = 0;

    int maxTextSize = 0;
    int expectedHeight = m_sepYPadding * 2;

    // 统计期望的高度和宽度
    for (auto &item : m_items) {
        if (item.isChecked) { m_hasChecked = true; }
        if (item.isSubmenu) { m_hasSubmenu = true; }

        if (item.isSeparator) {
            expectedHeight += m_sepYPadding * 2;
        } else {
            expectedHeight += m_itemHeight;
        }

        CSize size;

        if (!item.text.empty()) {
            if (canvas->getTextExtentPoint32(item.text.c_str(), item.text.size(), &size)
                && size.cx > maxTextSize) {
                maxTextSize = size.cx;
            }
        }

        if (!item.shortcutKey.empty()) {
            if (canvas->getTextExtentPoint32(item.shortcutKey.c_str(), item.shortcutKey.size(), &size)
                && size.cx > m_maxShortcutWidth) {
                m_maxShortcutWidth = size.cx;
            }
        }
    }

    // 计算宽度：当有 check 和 submenu 时需要额外的空间来显示对应的图标
    int expectedWidth = maxTextSize
        + (m_maxShortcutWidth > 0 ? SHORTCUT_KEY_SEP + m_maxShortcutWidth : 0)
        + m_xPadding * 3;
    if (m_hasChecked) { expectedWidth += m_itemHeight / 2; }
    if (m_hasSubmenu && m_maxShortcutWidth == 0) { expectedWidth += m_itemHeight / 2; }

    // 计算和调整最大能够显示的大小
    CRect rcWnd = m_pSkin->getBoundBox();
    int minTop = m_parentMsgReceiver ? m_parentMsgReceiver->getPopupTopY() : 0;
    int maxBottom = rcWnd.height() - 1;
    int maxRight = rcWnd.width();
    if (expectedHeight + minTop > maxBottom) {
        m_isShowArrow = true;
        m_rcObj.top = minTop;
        expectedHeight = maxBottom - minTop;
    } else if (expectedHeight + m_rcObj.top > maxBottom) {
        m_rcObj.top = maxBottom - expectedHeight;
    }

    if (expectedWidth >= maxRight) {
        m_rcObj.left = 0;
    } else if (m_rcObj.left + expectedWidth >= maxRight) {
        // 右边越界了
        m_rcObj.left = maxRight - expectedWidth;
        if (m_rcObj.left < 0) {
            m_rcObj.left = 0;
            expectedWidth = maxRight - 1;
        }
    }

    m_rcObj.bottom = m_rcObj.top + expectedHeight;
    m_rcObj.right = m_rcObj.left + max(expectedWidth, MIN_ITEM_WIDTH);

    if (m_isShowArrow) {
        // 计算能够滚动到的最大 m_firstItem
        int height = m_sepYPadding * 2 + m_itemHeight * 2;
        for (int i = (int)m_items.size() - 1; i >= 0; i--) {
            if (m_items[i].isSeparator) {
                height += m_sepYPadding * 2;
            } else {
                height += m_itemHeight;
            }
            if (height >= m_rcObj.height()) {
                m_firstItemMax = i + 1;
                break;
            }
        }
    }
}

bool SkinMenuItemsContainer::onLButtonDown(uint32_t nFlags, CPoint point) {
    return m_rcObj.ptInRect(point);
}

bool SkinMenuItemsContainer::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (!m_rcObj.ptInRect(point)) {
        onPopupMenuClosed();
        m_pSkin->invalidateRect();

        return false;
    }

    int item = hitTestMenuItem(point);
    if (item == IDX_UP_ARROW && m_firstItem > 0) {
        offsetFirstVisibleMenuItem(-((m_rcObj.height() - m_sepYPadding * 2) / m_itemHeight - 1));
    } else if (item == IDX_DOWN_ARROW) {
        offsetFirstVisibleMenuItem((m_rcObj.height() - m_sepYPadding * 2) / m_itemHeight - 1);
    } else if (item != IDX_INVALID && m_items[item].id != ID_INVALID) {
        onPopupMenuClosed();
        m_pSkin->invalidateRect();
        m_pSkin->onCommand(m_items[item].id);
        return true;
    }

    return item != -1;
}

void SkinMenuItemsContainer::onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) {
    if (isChildMenuVisible()) {
        m_childMenu->onMouseWheel(nWheelDistance, nMkeys, pt);
    } else if (m_isShowArrow) {
        offsetFirstVisibleMenuItem(nWheelDistance);
    }
}

bool SkinMenuItemsContainer::onMouseMove(CPoint point) {
    if (!m_rcObj.ptInRect(point)) {
        return false;
    }

    int item = hitTestMenuItem(point);
    if (item >= IDX_INVALID && item != m_selectedItem) {
        m_selectedItem = item;
        m_isMenuActive = true;
        onMenuItemSelected();
    } else if (!m_isMenuActive) {
        m_isMenuActive = true;
        if (m_childMenu) {
            m_childMenu->activateMenu(false, true);
        }
        invalidate();
    }

    return true;
}

bool SkinMenuItemsContainer::onKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (isChildMenuVisible() && !m_isMenuActive) {
        return m_childMenu->onKeyDown(nChar, nFlags);
    }

    switch (nChar) {
        case VK_UP: {
            if (m_selectedItem > 0) {
                m_selectedItem -= 1;
                if (m_items[m_selectedItem].isSeparator && m_selectedItem > 0) {
                    m_selectedItem--;
                }

                if (m_isShowArrow && m_selectedItem < m_firstItem) {
                    offsetFirstVisibleMenuItem(-1);
                }
                onMenuItemSelected();
            }
            break;
        }
        case VK_DOWN: {
            if (m_selectedItem < (int)m_items.size() - 1) {
                m_selectedItem++;
                if (m_items[m_selectedItem].isSeparator && m_selectedItem > 0) {
                    m_selectedItem++;
                }

                if (m_isShowArrow && m_selectedItem >= m_lastVisbleItem) {
                    offsetFirstVisibleMenuItem(1);
                }
                onMenuItemSelected();
            }
            break;
        }
        case VK_LEFT: {
            if (isChildMenuVisible()) {
                m_childMenu->setVisible(false, false);
                m_pSkin->invalidateRect();
            } else {
                m_parentMsgReceiver->onPopupKeyDown(nChar, nFlags);
            }
            break;
        }
        case VK_RIGHT: {
            if (isChildMenuVisible()) {
                m_isMenuActive = false;
                m_childMenu->m_selectedItem = 0;
                m_childMenu->activateMenu(true, true);
                invalidate();
            } else if (m_selectedItem >= 0 && m_items[m_selectedItem].isSubmenu) {
                onMenuItemSelected();
            } else if (m_parentMsgReceiver) {
                m_parentMsgReceiver->onPopupKeyDown(nChar, nFlags);
            }
            break;
        }
        default:
            return false;
    }

    return true;
}

void fillMaskRectCenter(CRawGraph *canvas, int x, int y, CSFImage &image, const CColor &clr) {
    canvas->fillRect(makeRectLTWH(x, y, image.width(), image.height()),
                     image, makeRectLTWH(image.x(), image.y(), image.width(), image.height()),
                     clr);
}

void drawUpDownArrow(CRawGraph *canvas, const CRect &rc, CSFImage &image, bool isGray, const CColor &clrGray) {
    int left = rc.left + (rc.width() - image.width()) / 2;
    int top = rc.top + (rc.height() - image.height()) / 2;
    if (isGray) {
        fillMaskRectCenter(canvas, left, top, image, clrGray);
    } else {
        image.blt(canvas, left, top);
    }
}

void SkinMenuItemsContainer::draw(CRawGraph *canvas) {
    CRect rcObj = m_rcObj;

    m_lastVisbleItem = (int)m_items.size() - 1;
    if (m_isShowArrow) {
        int y = m_rcObj.top + m_sepYPadding + m_itemHeight;
        int latestY = y;
        for (int i = m_firstItem; i < (int)m_items.size(); i++) {
            auto &item = m_items[i];
            int h = item.isSeparator ? m_sepYPadding * 2 : m_itemHeight;
            if (y + h + m_itemHeight > m_rcObj.bottom) {
                // 已经无法再显示一个向下的箭头了
                if (m_rcObj.bottom - y >= m_itemHeight) {
                    // 当前的这个 item 去掉就行
                    rcObj.bottom = y + m_itemHeight;
                    m_lastVisbleItem = i - 1;
                } else {
                    // 必须去掉再前一个 item
                    rcObj.bottom = latestY + m_itemHeight;
                    m_lastVisbleItem = i - 2;
                }
                m_lastVisbleItem = max(m_lastVisbleItem, 0);
                break;
            }

            latestY = y;
            y += h;
        }
    }

    const int SELECTED_MARGIN = 8, RADIUS = 5;

    canvas->fillRoundedRect(rcObj.left, rcObj.top, rcObj.width(), rcObj.height(), RADIUS, m_clrBg);
    canvas->setPen(m_penFrame);
    canvas->roundedRect(rcObj.left, rcObj.top, rcObj.width(), rcObj.height(), RADIUS);

    if (m_font.isGood()) {
        canvas->setFont(m_font.getFont());
    }

    int y = rcObj.top + m_sepYPadding;
    int x = rcObj.left + m_xPadding + (m_hasChecked ? m_itemHeight / 2 : 0);
    int xEnd = rcObj.right - m_xPadding;

    CColor clrText = m_font.getTextColor(true);
    canvas->setTextColor(clrText);

    if (m_isShowArrow) {
        // 菜单空间不足以显示所有项，显示上箭头
        drawUpDownArrow(canvas, makeRectLTWH(x, y, xEnd - x, m_itemHeight),
                        m_imageUpArrow, m_firstItem == 0, m_font.getTextColor(false));
        y += m_itemHeight;
    }

    for (int i = m_firstItem; i <= m_lastVisbleItem; i++) {
        auto &item = m_items[i];
        if (item.isSeparator) {
            // 绘制 separator
            y += m_sepYPadding;
            canvas->line(x, y, xEnd, y);
            y += m_sepYPadding;
            continue;
        }

        if (m_selectedItem == i) {
            // 绘制选择菜单的背景
            CColor clr = m_isMenuActive ? m_clrHoverBg : m_clrInactiveBg;
            canvas->fillRoundedRect(rcObj.left + SELECTED_MARGIN, y,
                rcObj.width() - SELECTED_MARGIN * 2, m_itemHeight, RADIUS, clr);
        }

        if (item.isChecked && m_imageChecked.isValid()) {
            // 绘制 checkbox 的选中图标
            assert(m_hasChecked);
            m_imageChecked.blt(canvas, (rcObj.left + x - m_imageChecked.width()) / 2,
                y + (m_itemHeight - m_imageChecked.height()) / 2);
        }

        CRect rcItem(x, y, xEnd, y + m_itemHeight);
        CRect rcText = rcItem;
        if (m_maxShortcutWidth > 0) {
            // 标题不要画进快捷键列
            rcText.right = xEnd - m_maxShortcutWidth - SHORTCUT_KEY_SEP;
            if (rcText.right < rcText.left) {
                rcText.right = rcText.left;
            }
        }

        if (!item.isEnabled) {
            canvas->setTextColor(m_font.getTextColor(false));
        }

        canvas->drawText(item.text.c_str(), item.text.size(), rcText,
            DT_VCENTER | DT_END_ELLIPSIS | DT_PREFIX_TEXT | DT_SINGLELINE);

        if (!item.shortcutKey.empty()) {
            canvas->drawText(item.shortcutKey.c_str(), item.shortcutKey.size(), rcItem,
                DT_VCENTER | DT_RIGHT | DT_SINGLELINE);
        }

        if (!item.isEnabled) {
            canvas->setTextColor(clrText);
        }

        if (item.isSubmenu && m_imageSubmenu.isValid()) {
            // 绘制子菜单的右向箭头
            m_imageSubmenu.blt(canvas, xEnd - m_imageSubmenu.width() + 8,
                y + (m_itemHeight - m_imageSubmenu.height()) / 2, BPM_BLEND);
        }

        y += m_itemHeight;
    }

    if (m_isShowArrow) {
        // 菜单空间不足以显示所有项，显示下箭头
        drawUpDownArrow(canvas, makeRectLTWH(x, y, xEnd - x, m_itemHeight),
            m_imageDownArrow, m_lastVisbleItem == (int)m_items.size() - 1, m_font.getTextColor(false));
    }
}

bool SkinMenuItemsContainer::isDealingMouseMove(CPoint pt) {
    return (isVisible() && isPtIn(pt)) || (m_childMenu && m_childMenu->isDealingMouseMove(pt));
}

void SkinMenuItemsContainer::setMenu(const CMenu &menu) {
    m_items.clear();
    m_selectedItem = -1;
    m_firstItem = 0;
    m_menu = menu;
    m_isMenuActive = false;

    int count = m_menu.getItemCount();
    for (int i = 0; i < count; i++) {
        MenuItemInfo item = { 0 };
        if (m_menu.getMenuItemInfo(i, true, item)) {
            // 兼容仍把加速键拼在标题里（Label\tCtrl+O）的平台
            string label, shortcut;
            if (strSplit(item.text.c_str(), '\t', label, shortcut)) {
                item.text = std::move(label);
                if (item.shortcutKey.empty()) {
                    item.shortcutKey = std::move(shortcut);
                }
            }
            m_items.push_back(item);
        }
    }

    m_lastVisbleItem = (int)m_items.size();

    onMeasureSizeByContent();

    if (isChildMenuVisible()) {
        m_childMenu->setVisible(false, false);
        m_pSkin->invalidateRect();
    } else {
        invalidate();
    }
}

bool SkinMenuItemsContainer::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (m_font.setProperty(szProperty, szValue)) {
        return true;
    } else if (isPropertyName(szProperty, "TextColorHover")) {
        m_clrHoverText = parseColorString(szValue);
    } else if (isPropertyName(szProperty, "BgColorHover")) {
        m_clrHoverBg = parseColorString(szValue);
    } else if (isPropertyName(szProperty, "BgColorInactive")) {
        m_clrInactiveBg = parseColorString(szValue);
    } else if (isPropertyName(szProperty, "ImageChecked")) {
        m_imageChecked.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "ImageSubmenu")) {
        m_imageSubmenu.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "ImageUpArrow")) {
        m_imageUpArrow.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "ImageDownArrow")) {
        m_imageDownArrow.loadFromSRM(m_pSkin, szValue);
    } else if (isPropertyName(szProperty, "FrameColor")) {
        m_penFrame.createSolidPen(1, parseColorString(szValue));
    } else {
        return false;
    }

    return true;
}

void SkinMenuItemsContainer::setVisible(bool bVisible, bool bRedraw) {
    CUIObject::setVisible(bVisible, bRedraw);

    if (!bVisible && m_childMenu) {
        // 连子菜单一起隐藏
        m_childMenu->setVisible(bVisible, bRedraw);
    }
}

void SkinMenuItemsContainer::onPopupMenuClosed() {
    setVisible(false, false);

    if (m_parentMsgReceiver) {
        m_parentMsgReceiver->onPopupMenuClosed();
    }
}

void SkinMenuItemsContainer::onPopupKeyDown(uint32_t nChar, uint32_t nFlags) {
    if (nChar == VK_LEFT) {
        if (isChildMenuVisible()) {
            m_childMenu->setVisible(false, false);
            m_isMenuActive = true;
            m_pSkin->invalidateRect();
            return;
        }
    }

    if (m_parentMsgReceiver) {
        m_parentMsgReceiver->onPopupKeyDown(nChar, nFlags);
    }
}

void SkinMenuItemsContainer::activateMenu(bool isActive, bool redraw) {
    m_isMenuActive = isActive;
    if (redraw && isVisible()) {
        invalidate();
    }
}

int SkinMenuItemsContainer::getPopupTopY() {
    if (m_parentMsgReceiver) {
        return m_parentMsgReceiver->getPopupTopY();
    }

    return m_rcObj.top;
}

int SkinMenuItemsContainer::hitTestMenuItem(const CPoint point) {
    assert(m_rcObj.ptInRect(point));

    int y = m_rcObj.top + m_sepYPadding;

    if (m_isShowArrow) {
        y += m_itemHeight;
        if (point.y < y) {
            return IDX_UP_ARROW;
        }
    }

    int i = m_firstItem;
    assert(m_lastVisbleItem <= (int)m_items.size());
    m_lastVisbleItem = min((int)m_items.size() - 1, m_lastVisbleItem);
    for (; i <= m_lastVisbleItem; i++) {
        if (m_items[i].isSeparator) {
            y += m_sepYPadding * 2;
            if (point.y < y) {
                return IDX_INVALID;
            }
        } else {
            y += m_itemHeight;
            if (point.y < y) {
                return i;
            }
        }
    }

    return IDX_DOWN_ARROW;
}

void SkinMenuItemsContainer::onMenuItemSelected() {
    if (m_selectedItem == -1) {
        invalidate();
        return;
    }

    if (m_items[m_selectedItem].isSubmenu) {
        // 显示子菜单
        if (m_childMenu == nullptr) {
            m_childMenu = (SkinMenuItemsContainer *)m_pSkin->createUIObject("MenuItemsContainer", m_pSkin->getRootContainer());
            assert(m_childMenu);
            m_childMenu->setParentMsgReceiver(this);
            m_pSkin->getRootContainer()->addUIObject(m_childMenu);
        }

        CMenu menu = m_menu.getSubmenu(m_selectedItem);
        m_childMenu->m_rcObj.left = m_rcObj.right - 5;
        m_childMenu->m_rcObj.top = getItemY(m_selectedItem);
        m_childMenu->setMenu(menu);
        m_childMenu->setVisible(true, false);
        m_pSkin->invalidateRect();
    } else {
        if (!isChildMenuVisible()) {
            invalidate();
            return;
        }

        m_childMenu->setVisible(false, false);
        m_pSkin->invalidateRect();
    }
}

bool SkinMenuItemsContainer::isChildMenuVisible() {
    return m_childMenu && m_childMenu->isVisible();
}

int SkinMenuItemsContainer::getItemY(int subMenu) {
    int y = m_rcObj.top + m_sepYPadding;

    if (m_isShowArrow) {
        y += m_itemHeight;
    }

    for (int i = m_firstItem; i < (int)m_items.size(); i++) {
        if (subMenu == i) {
            return y;
        }

        auto &item = m_items[i];
        if (item.isSeparator) {
            y += m_sepYPadding * 2;
        } else {
            y += m_itemHeight;
        }
    }

    return 0;
}

void SkinMenuItemsContainer::offsetFirstVisibleMenuItem(int offset) {
    int orgFirstItem = m_firstItem;

    m_firstItem += offset;
    if (m_firstItem > m_firstItemMax) {
        m_firstItem = m_firstItemMax;
    }

    if (m_firstItem < 0) {
        m_firstItem = 0;
    }

    if (orgFirstItem != m_firstItem) {
        invalidate();
    }
}
