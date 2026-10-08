//
//  WindowMacImp.h
//
//  Created by Hongyong Xiao on 11/18/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#pragma once

#import <AppKit/AppKit.h>

typedef map<int, NSTimer *> MapNSTimer;

class Window;

@interface WindowMacImp : NSWindow <NSWindowDelegate> {
    MapNSTimer mMapTimer;
    Window *mBaseWnd;
    float mYScroll, mXScroll;
}

- (void)setTimer:(int)idTimer duration:(int)duration;
- (void)killTimer:(int)idTimer;

- (void)onTimer:(NSTimer*)theTimer;

- (void)setOwnerBaseWnd:(Window*)baseWnd;

- (void)onUserMsg:(NSArray*)msg ;

// 磨砂玻璃容器（皮肤 <Window Glass="true"/> 时显示在皮肤绘制层之下，
// 让窗口透明区域透出系统 blur）。容器是普通 NSView，layer 裁圆角会连同内部
// 磨砂材质一起裁剪（见 Window::setGlassEffect）。懒构建，未开启时为空。
@property (nonatomic, strong) NSView *glassContainer;

// 被装进磨砂容器的原件（皮肤自绘视图 ViewMacImp）。切换皮肤/关闭磨砂时，
// 用它把容器拆掉、恢复为窗口 contentView，避免残留磨砂容器影响下一个皮肤。
@property (nonatomic, strong) NSView *glassSkinView;

@end
