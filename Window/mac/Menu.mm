#import <Cocoa/Cocoa.h>
#include "../WindowLib.h"


@interface MLMenuImp : NSObject {
    Window *mBaseWnd;

}

- (void) onCommand:(NSMenuItem *)item;
- (void) dispatchCommand:(NSNumber *)cmd;
- (void) setBaseWnd:(Window *)pWnd;

@end

@implementation MLMenuImp

- (void) onCommand:(NSMenuItem *)item {
    // 异步派发，避免在 NSMenu 跟踪栈内同步执行 onCommand（开窗/销毁 UI）
    NSNumber *cmd = [NSNumber numberWithUnsignedInt:(uint32_t)[item tag]];
    [self performSelectorOnMainThread:@selector(dispatchCommand:)
                           withObject:cmd
                        waitUntilDone:NO];
}

- (void) dispatchCommand:(NSNumber *)cmd {
    if (mBaseWnd) {
        mBaseWnd->onCommand([cmd unsignedIntValue]);
    }
}

-(BOOL) validateMenuItem:(NSMenuItem *)item {
    return YES;
}

- (void) setBaseWnd:(Window *)pWnd {
    mBaseWnd = pWnd;
}

@end


cstr_t ToMacMenuText(cstr_t szOrg, string &buf) {
    cstr_t p = strchr(szOrg, '&');
    if (p == nullptr) {
        return szOrg;
    }
    if (p[1] == '&') {
        buf = szOrg;
        strrep(buf, "&&", "&");
        return buf.c_str();
    }

    if (!isAlpha(p[1])) {
        return szOrg;
    }

    buf = szOrg;
    if (p > szOrg) {
        if (*(p - 1) == '(' && p[2] == ')') {
            buf.erase((int)(p - szOrg) - 1, 4);
            return buf.c_str();
        }
    }

    buf.erase((int)(p - szOrg), 1);
    return buf.c_str();
}

bool ToLocalMenu(CMenu *pMenu) {
    return false;
}

struct MLMenuInfo {
    MLMenuImp                   *menuImp;
    NSMenu                      *menu;
};

//////////////////////////////////////////////////////////////////////

CMenu::CMenu() {
    m_info = new MLMenuInfo();
    m_info->menuImp = nil;
    m_info->menu = nil;
}

CMenu::CMenu(const CMenu &src) {
    m_info = new MLMenuInfo();
    m_info->menuImp = src.m_info->menuImp;
    m_info->menu = src.m_info->menu;
    [m_info->menuImp retain];
    [m_info->menu retain];
}

CMenu::~CMenu() {
    destroy();

    delete m_info;
}

bool CMenu::isValid() const {
    return m_info->menuImp != nil;
}

bool CMenu::createPopupMenu() {
    destroy();

    m_info->menuImp = [MLMenuImp alloc];
    m_info->menu = [[NSMenu alloc] initWithTitle:@"Menu"];
    [m_info->menu setAutoenablesItems:NO];

    return true;
}


void CMenu::destroy() {
    if (m_info->menu != nil) {
        [m_info->menu release];
        m_info->menu = nil;
    }

    if (m_info->menuImp != nil) {
        [m_info->menuImp release];
        m_info->menuImp = nil;
    }
}

void CMenu::trackPopupMenu(int x, int y, Window *pWnd, CRect *prcNotOverlap) {
    assert(pWnd);

    updateMenuStatus(pWnd);

    NSPoint pt = { float(x), float(y) };
    [m_info->menuImp setBaseWnd:pWnd];
    @try {
        [m_info->menu popUpMenuPositioningItem: [m_info->menu itemAtIndex:0]
            atLocation: pt
            inView: nil];
    }
    @catch (NSException *exception) {
    }
}

void CMenu::trackPopupSubMenu(int x, int y, int nSubMenu, Window *pWnd, CRect *prcNotOverlap) {
    assert(pWnd);

    updateMenuStatus(pWnd);

    NSPoint pt = { float(x), float(y) };
    [m_info->menuImp setBaseWnd:pWnd];
    NSMenuItem *item = [m_info->menu itemAtIndex:nSubMenu];
    if (item == nil || [item hasSubmenu] != YES) {
        return;
    }

    [[item submenu] popUpMenuPositioningItem: [[item submenu] itemAtIndex:0]
        atLocation: pt
        inView: nil];
}

void CMenu::enableItem(int nID, bool bEnable) {
    NSMenuItem *item = [m_info->menu itemWithTag:nID];
    if (item != nil) {
        [item setEnabled:bEnable ? YES : NO];
    }
}

void CMenu::checkItem(int nID, bool bCheck) {
    NSMenuItem *item = [m_info->menu itemWithTag:nID];
    if (item != nil) {
        [item setState:bCheck ? NSControlStateValueOn : NSControlStateValueOff];
    }
}

void CMenu::checkRadioItem(int nID, int nStartID, int nEndID) {
    for (int i = nStartID; i <= nEndID; i++) {
        checkItem(i, i == nID);
    }
}

int CMenu::getItemCount() {
    return (int)[m_info->menu numberOfItems];
}

void CMenu::appendItem(uint32_t nID, cstr_t szText, cstr_t szShortcutKey) {
    string buf;
    NSString *title = [NSString stringWithUTF8String:ToMacMenuText(szText, buf)];
    NSMenuItem *item = [m_info->menu addItemWithTitle:title
        action:@selector(onCommand:)
        keyEquivalent:[NSString stringWithUTF8String:szShortcutKey]];
    [item setTarget:m_info->menuImp];
    [item setTag:nID];
}

void CMenu::appendSeperator() {
    [m_info->menu addItem:[NSMenuItem separatorItem]];
}

void CMenu::insertItem(int nPos, uint32_t nID, cstr_t szText, cstr_t szShortcutKey) {
    string buf;
    NSString *title = [NSString stringWithUTF8String:ToMacMenuText(szText, buf)];
    NSMenuItem *item = [m_info->menu insertItemWithTitle:title
        action:@selector(onCommand:)
        keyEquivalent:[NSString stringWithUTF8String:szShortcutKey]
        atIndex:nPos];
    [item setTarget:m_info->menuImp];
    [item setTag:nID];
}

void CMenu::insertItemByID(uint32_t nPosID, uint32_t nID, cstr_t szText, cstr_t szShortcutKey) {
    int pos = (int)[m_info->menu indexOfItemWithTag:nID];
    if (pos == -1) {
        appendItem(nID, szText, szShortcutKey);
    } else {
        insertItem(pos, nID, szText, szShortcutKey);
    }
}

void CMenu::insertSeperator(int nPos) {
    [m_info->menu insertItem: [NSMenuItem separatorItem] atIndex: nPos];
}

CMenu CMenu::appendSubmenu(cstr_t szText) {
    string buf;
    NSString *title = [NSString stringWithUTF8String:ToMacMenuText(szText, buf)];
    NSMenuItem *item = [m_info->menu addItemWithTitle:title
        action:@selector(onCommand:)
        keyEquivalent:@""];
    [item setTarget:m_info->menuImp];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:@""];
    [item setSubmenu:submenu];

    CMenu menu;
    menu.m_info->menu = submenu;
    menu.m_info->menuImp = m_info->menuImp;
    [menu.m_info->menuImp retain];

    return menu;
}

CMenu CMenu::insertSubmenu(int nPos, cstr_t szText) {
    string buf;
    NSString *title = [NSString stringWithUTF8String:ToMacMenuText(szText, buf)];
    NSMenuItem *item = [m_info->menu insertItemWithTitle:title
        action:@selector(onCommand:)
        keyEquivalent:@""
        atIndex:nPos];
    [item setTarget:m_info->menuImp];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:@""];
    [item setSubmenu:submenu];

    CMenu menu;
    menu.m_info->menu = submenu;
    menu.m_info->menuImp = m_info->menuImp;
    [menu.m_info->menuImp retain];

    return menu;
}

void CMenu::removeItem(int nPos) {
    [m_info->menu removeItemAtIndex:nPos];
}

void CMenu::removeAllItems() {
    [m_info->menu removeAllItems];
}

bool CMenu::getMenuItemText(uint32_t index, string &strText, bool byPosition) {
    MenuItemInfo info;

    if (getMenuItemInfo(index, byPosition, info)) {
        strText = info.text;
        return true;
    }

    return false;
}

bool CMenu::getMenuItemInfo(uint32_t index, bool byPosition, MenuItemInfo &itemOut) {
    NSMenuItem *item = nil;
    @try {
        if (byPosition) {
            item = [m_info->menu itemAtIndex:index];
        } else {
            item = [m_info->menu itemWithTag:index];
        }
    }
    @catch (NSException *exception) {
        return false;
    }
    if (item == nil) {
        return false;
    }

    itemOut.text = [[item title] UTF8String];
    itemOut.id = (int)[item tag];
    itemOut.isSubmenu = [item hasSubmenu] == YES;
    itemOut.isSeparator = [item isSeparatorItem];
    itemOut.isEnabled = [item isEnabled];
    itemOut.isChecked = [item state] == NSControlStateValueOn;
    itemOut.shortcutKey = [[item keyEquivalent] UTF8String];

    return true;
}

bool CMenu::isSeparator(int pos) {
    @try {
        NSMenuItem *item = [m_info->menu itemAtIndex:pos];
        if (item) {
            return [item isSeparatorItem];
        }
    }
    @catch (NSException *exception) {
        return false;
    }
    return false;
}

string CMenu::getShortcutKeyText(int pos) {
    @try {
        NSMenuItem *item = [m_info->menu itemAtIndex:pos];
        if (item) {
            return [[item keyEquivalent] UTF8String];
        }
    }
    @catch (NSException *exception) {
    }
    return "";
}

bool CMenu::hasItem(int nPos) {
    return nPos >= 0 && nPos < getItemCount();
}


bool CMenu::hasSubmenu(int nPos) {
    if (!hasItem(nPos)) {
        return false;
    }

    @try {
        NSMenuItem *item = [m_info->menu itemAtIndex:nPos];
        if (item == nil) {
            return false;
        }

        return [item hasSubmenu] == YES;
    }
    @catch (NSException *exception) {
        return false;
    }
}

CMenu CMenu::getSubmenu(int nPos) {
    CMenu menu;

    NSMenuItem *item = [m_info->menu itemAtIndex:nPos];
    if (item == nil) {
        return menu;
    }

    menu.m_info->menu = [item submenu];
    [menu.m_info->menu retain];
    menu.m_info->menuImp = m_info->menuImp;
    [menu.m_info->menuImp retain];

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

CMenu &CMenu::operator = (const CMenu &menu) {
    m_info->menu = menu.m_info->menu;
    m_info->menuImp = menu.m_info->menuImp;

    [m_info->menu retain];
    [m_info->menuImp retain];

    return *this;
}

void *CMenu::getHandle(Window *window) {
    [m_info->menuImp setBaseWnd:window];
    return m_info->menu;
}

void CMenu::attachHandle(void *handle) {
    [m_info->menu release];
    m_info->menu = (NSMenu *)handle;

    if (handle) {
        [m_info->menu retain];

        onLoadMenu();
    }
}
