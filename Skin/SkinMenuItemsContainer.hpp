//
//  SkinMenuItemsContainer.hpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/12.
//

#ifndef SkinMenuItemsContainer_hpp
#define SkinMenuItemsContainer_hpp

#include "UIObject.h"
#include "SkinButton.h"


/**
 * @brief 由弹出的子菜单发送通知消息给上一级菜单.
 * 
 */
class IMenuEventNotify {
public:
    // 当子菜单被关闭，会逐级触发关闭所有弹出的父菜单
    virtual void onPopupMenuClosed() = 0;

    // 子菜单未处理的 KeyDown 事件，交由上一级菜单处理
    // 主要是 VK_LEFT, VK_RIGHT，进行菜单的切换.
    virtual void onPopupKeyDown(uint32_t nChar, uint32_t nFlags) = 0;

    // 返回要弹出菜单的最小的 Y 值.
    virtual int getPopupTopY() = 0;

};

/**
 * @brief 弹出的菜单组件，可支持弹出多级菜单.
 *
 */
class SkinMenuItemsContainer : public CUIObject, public IMenuEventNotify {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    SkinMenuItemsContainer();
    virtual ~SkinMenuItemsContainer();

    void onCreate() override;
    void onMeasureSizeByContent() override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    bool onLButtonUp(uint32_t nFlags, CPoint point) override;
    bool onMouseMove(CPoint point) override;
    void onMouseWheel(int nWheelDistance, int nMkeys, CPoint pt) override;
    bool onKeyDown(uint32_t nChar, uint32_t nFlags) override;

    void draw(CRawGraph *canvas) override;
    bool setProperty(cstr_t szProperty, cstr_t szValue) override;
    void setVisible(bool bVisible, bool bRedraw = false) override;

    bool isDealingMouseMove(CPoint pt);
    void setMenu(const CMenu &menu);
    void setParentMsgReceiver(IMenuEventNotify *parentMsgReceiver)
        { m_parentMsgReceiver = parentMsgReceiver; }

    void onPopupMenuClosed() override;
    void onPopupKeyDown(uint32_t nChar, uint32_t nFlags) override;

    void activateMenu(bool isActive = true, bool redraw = false);
    int getPopupTopY() override;

protected:
    int hitTestMenuItem(const CPoint point);
    int getItemsMinSumHeight();

    void onMenuItemSelected();

    bool isChildMenuVisible();
    int getItemY(int subMenu);
    void offsetFirstVisibleMenuItem(int offset);

protected:
    // 弹出的子菜单
    SkinMenuItemsContainer      *m_childMenu;

    // 创建此菜单的父控件，需要接收鼠标等消息
    IMenuEventNotify            *m_parentMsgReceiver;

    bool                        m_isMenuActive = false;
    bool                        m_isShowArrow = false;
    bool                        m_hasChecked = false;
    bool                        m_hasSubmenu = false;
    int                         m_maxShortcutWidth = 0;
    uint32_t                    m_xPadding;
    uint32_t                    m_sepYPadding;
    uint32_t                    m_itemHeight;
    CMenu                       m_menu;

    CSkinFontProperty           m_font;
    CColor                      m_clrHoverText;
    CColor                      m_clrHoverBg;
    CColor                      m_clrInactiveBg;
    CColor                      m_clrTextDisabled;

    CSFImage                    m_imageChecked;
    CSFImage                    m_imageSubmenu;
    CSFImage                    m_imageUpArrow, m_imageDownArrow;
    CRawPen                     m_penFrame;

    // 当前选择的菜单项
    int                         m_selectedItem;

    // 第一个菜单项，当 m_topArrow 为 true 时有效.
    int                         m_firstItem = 0;
    int                         m_lastVisbleItem = 0;
    int                         m_firstItemMax = 0;

    vector<MenuItemInfo>        m_items;

};

#endif /* SkinMenuItemsContainer_hpp */
