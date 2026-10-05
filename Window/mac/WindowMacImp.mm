//
//  WindowMacImp.m
//
//  Created by Hongyong Xiao on 11/18/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "../WindowTypes.h"
#import "WindowMacImp.h"
#import "Window.h"
#import "ViewMacImp.h"
#include <math.h>

static void flushWheelAxis(Window *wnd, float *accum, float delta, uint32_t keys, CPoint pt) {
    *accum += delta;
    if (fabsf(*accum) >= 1.f) {
        int ticks = (int)*accum;
        wnd->onMouseWheel(ticks, keys, pt);
        *accum -= ticks;
    }
}

@implementation WindowMacImp

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle
backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag {
    self = [super initWithContentRect:contentRect
        styleMask:aStyle
        backing:NSBackingStoreBuffered
        defer:NO];
    if (self != nil) {
        //[self setAlphaValue:1.0];
        [self setBackgroundColor:[NSColor clearColor]];
        [self setOpaque:NO];
        [self setDelegate: self];
    }

    mBaseWnd = nullptr;
    mYScroll = 0;
    mXScroll = 0;

    return self;
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    CGFloat scaleFactor = [self backingScaleFactor];
    if (mBaseWnd->m_scaleFactor != scaleFactor) {
        DBG_LOG1("ScaleFactorChanged to: %f\n", scaleFactor);
        mBaseWnd->m_scaleFactor = scaleFactor;
        mBaseWnd->onScaleFactorChanged(scaleFactor);
    }
}

- (void)scrollWheel:(NSEvent *)theEvent {
    CPoint pt = NSPointToCPoint([theEvent locationInWindow], [self frame].size.height);

    // 触摸板 / Magic Mouse：scrollingDelta 是像素；普通鼠标滚轮是「行」（一格约为 ±1）。
    // 原先一律 / 20，鼠标一格只积 0.05，要约 20 格才滚一行。
    const float kPixelsPerTick = 20.f;

    uint32_t modifierFlags = [theEvent modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

    float dx = -[theEvent scrollingDeltaX];
    float dy = -[theEvent scrollingDeltaY];
    if ([theEvent hasPreciseScrollingDeltas]) {
        dx /= kPixelsPerTick;
        dy /= kPixelsPerTick;
    }

    flushWheelAxis(mBaseWnd, &mXScroll, dx, modifierFlags | MK_SHIFT, pt);
    flushWheelAxis(mBaseWnd, &mYScroll, dy, modifierFlags, pt);
}

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (void)setTimer:(int)idTimer duration:(int)duration {
    [self killTimer:idTimer];

    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:((double)duration) / 1000
        target:self
        selector:@selector(onTimer:)
        userInfo:[NSNumber numberWithInt:idTimer]
        repeats:YES];

    mMapTimer[idTimer] = timer;
}

- (void)killTimer:(int)idTimer {
    MapNSTimer::iterator it = mMapTimer.find(idTimer);

    if (it != mMapTimer.end()) {
        NSTimer *timer = (*it).second;
        [timer invalidate];
        mMapTimer.erase(it);
    }
}

- (void)onTimer:(NSTimer*)theTimer {
    NSNumber *idTimer = [theTimer userInfo];

    mBaseWnd->onTimer([idTimer intValue]);
}

- (void)setOwnerBaseWnd:(Window*)baseWnd {
    mBaseWnd = baseWnd;
}

- (void)dealloc {
    for (MapNSTimer::iterator it = mMapTimer.begin(); it != mMapTimer.end(); ++it) {
        NSTimer *timer = (*it).second;
        [timer invalidate];
    }
    mMapTimer.clear();

    [super dealloc];
}

- (void)onUserMsg:(NSArray*)msg {
    NSNumber *msgId = [msg objectAtIndex:0];
    NSNumber *param = [msg objectAtIndex:1];
    mBaseWnd->onUserMessage([msgId intValue], (LPARAM)[param longValue]);
}

////////////////////////////////////////////////////////////////////////////////////////
// NSWindowDelegate messages

- (BOOL)windowShouldClose:(id)sender {
    if (mBaseWnd->onClose()) {
        return YES;
    }
    return NO;
}

// - (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize;
// - (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)newFrame;
// - (bool)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame;
// - (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window;
// - (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect;
/* If a window has a representedURL, the window will by default show a path popup menu for a command-click on a rectangle containing the window document icon button and the window title.  The window delegate may implement -window:shouldPopupDocumentPathMenu: to override NSWindow's default behavior for path popup menu.  A return of NO will prevent the menu from being shown.  A return of YES will cause the window to show the menu passed to this method, which by default will contain a menuItem for each path component of the representedURL.  If the representedURL has no path components, the menu will have no menu items.  Before returning YES, the window delegate may customize the menu by changing the menuItems.  menuItems may be added or deleted, and each menuItem title, action, or target may be modified.
 */
- (BOOL)window:(NSWindow *)window shouldPopUpDocumentPathMenu:(NSMenu *)menu NS_AVAILABLE_MAC(10_5) {
    return NO;
}

/* The window delegate may implement -window:shouldDragDocumentWithEvent:from:withPasteboard: to override NSWindow document icon's default drag behavior.  The delegate can prohibit the drag by returning NO.  Before returning NO, the delegate may implement its own dragging behavior using -[NSWindow dragImage:at:offset:event:pasteboard:source:slideBack:].  Alternatively, the delegate can enable a drag by returning YES, for example to override NSWindow's default behavior of prohibiting the drag of an edited document.  Lastly, the delegate can customize the pasteboard contents before returning YES.
 */
//- (bool)window:(NSWindow *)window shouldDragDocumentWithEvent:(NSEvent *)event from:(NSPoint)dragImageLocation withPasteboard:(NSPasteboard *)pasteboard NS_AVAILABLE_MAC(10_5);

//- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize NS_AVAILABLE_MAC(10_7);

//- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions NS_AVAILABLE_MAC(10_7);

/* The default animation between a window and its fullscreen representation is a crossfade.  With knowledge of the layout of a window before and after it enters fullscreen, an application can do a much better job on the animation.  The following API allows a window delegate to customize the animation by providing a custom window or windows containing layers or other effects.  In order to manage windows on spaces, we need the window delegate to provide a list of windows involved in the animation.  If an application does not do a custom animation, this method can be unimplemented or can return nil.  window:startCustomAnimationToEnterFullScreenWithDuration: will be called only if customWindowsToEnterFullScreenForWindow: returns non-nil.
 */
//- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window NS_AVAILABLE_MAC(10_7);

/* The system has started its animation into fullscreen, including transitioning to a new space.  Start the window fullscreen animation immediately, and perform the animation with the given duration to  be in sync with the system animation.  This method is called only if customWindowToEnterFullScreenForWindow: returned non-nil.
 */
//- (void)window:(NSWindow *)window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration NS_AVAILABLE_MAC(10_7);

/* In some cases, the transition to enter fullscreen will fail, due to being in the midst of handling some other animation or user gesture.  We will attempt to minimize these cases, but believe there is a need for failure handling.  This method indicates that there was an error, and the application should clean up any work it may have done to prepare to enter fullscreen.  This message will be sent whether or not the delegate indicated a custom animation by returning non-nil from  customWindowsToEnterFullScreenForWindow:.
 */
//- (void)windowDidFailToEnterFullScreen:(NSWindow *)window NS_AVAILABLE_MAC(10_7);

/* The window is about to exit fullscreen mode.  The following API allows a window delegate to customize the animation when the window is about to exit fullscreen.  In order to manage windows on spaces, we need the window delegate to provide a list of windows involved in the animation.  If an application does not do a custom animation, this method can be unimplemented or can return nil.  window:startCustomAnimationToExitFullScreenWithDuration: will be called only if customWindowsToExitFullScreenForWindow: returns non-nil.
 */
//- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window NS_AVAILABLE_MAC(10_7);

/* The system has started its animation out of fullscreen, including transitioning back to the desktop space.  Start the window animation immediately, and perform the animation with the given duration to  be in sync with the system animation.  This method is called only if customWindowsToExitFullScreenForWindow: returned non-nil.
 */
//- (void)window:(NSWindow *)window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration NS_AVAILABLE_MAC(10_7);

/* In some cases, the transition to exit fullscreen will fail, due to being in the midst of handling some other animation or user gesture.  We will attempt to minimize these cases, but believe there is a need for failure handling.  This method indicates that there was an error, and the application should clean up any work it may have done to prepare to exit fullscreen.  This message will be sent whether or not the delegate indicated a custom animation by returning non-nil from  customWindowsToExitFullScreenForWindow:.
 */
//- (void)windowDidFailToExitFullScreen:(NSWindow *)window NS_AVAILABLE_MAC(10_7);


/* Windows entering the version browser will be resized to the size returned by this method. If either dimension of the returned size is larger than the maxPreferredFrameSize, the window will also be scaled down to ensure it fits properly in the version browser. Returned sizes larger than maxAllowedSize will be constrained to that size. If this method is not implemented, the version browser will use -window:willUseStandardFrame: to determine the resulting window frame size.
 */
//- (NSSize)window:(NSWindow *)window willResizeForVersionBrowserWithMaxPreferredSize:(NSSize)maxPreferredFrameSize maxAllowedSize:(NSSize)maxAllowedFrameSize NS_AVAILABLE_MAC(10_7);


/* Method called by -[NSWindow encodeRestorableStateWithCoder:] to give the delegate a chance to encode any additional state into the NSCoder.  This state is available in the NSCoder passed to restoreWindowWithIdentifier:state:handler: . See the header NSWindowRestoration.h for more information.
 */
//- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state NS_AVAILABLE_MAC(10_7);

/* Method called by -[NSWindow restoreStateWithCoder:] to give the delegate a chance to restore its own state, which it may decode from the NSCoder. See the header NSWindowRestoration.h for more information.
 */
//- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state NS_AVAILABLE_MAC(10_7);

/* Notifications
 */
- (void)windowDidResize:(NSNotification *)notification {
    NSRect rc = [self frame];
    mBaseWnd->onResized(rc.size.width, rc.size.height);
}

- (void)windowDidExpose:(NSNotification *)notification {
    // DBG_LOG0("windowDidExpose");
}

//- (void)windowWillMove:(NSNotification *)notification;
- (void)windowDidMove:(NSNotification *)notification {
    NSRect rc = [self frame];
    mBaseWnd->onMove((int)rc.origin.x, (int)rc.origin.y);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    // MLTrayIcon onClicked is followed after this event, so delay 0.1 second
    // to make sure m_ignoreOneActivate flag is set by onClick.
    [self performSelector:@selector(postWindowDidBecomeKey) withObject:self afterDelay:0.1];
}

- (void)postWindowDidBecomeKey {
    if (mBaseWnd->m_ignoreOneActivate) {
        // Click menubar button will cause window become key unneccessary, revoke the side effect.
        [self resignKeyWindow];
        mBaseWnd->m_ignoreOneActivate = false;
        return;
    }

    mBaseWnd->onActivate(true);
    mBaseWnd->onSetFocus();
    // [self setStyleMask:NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable];
}

- (void)windowDidResignKey:(NSNotification *)notification {
    mBaseWnd->onKillFocus();
    // [self setStyleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskMiniaturizable];
    mBaseWnd->onActivate(false);
    mBaseWnd->m_bMouseCaptured = false;
}

//- (void)windowDidBecomeMain:(NSNotification *)notification;
//- (void)windowDidResignMain:(NSNotification *)notification;
- (void)windowWillClose:(NSNotification *)notification {
    mBaseWnd->onDestroy();
}

//- (void)windowWillMiniaturize:(NSNotification *)notification;
//- (void)windowDidMiniaturize:(NSNotification *)notification;
- (void)windowDidDeminiaturize:(NSNotification *)notification {
    mBaseWnd->invalidateRect();
}
//- (void)windowDidUpdate:(NSNotification *)notification;
//- (void)windowDidChangeScreen:(NSNotification *)notification;
//- (void)windowDidChangeScreenProfile:(NSNotification *)notification;
//- (void)windowWillBeginSheet:(NSNotification *)notification;
//- (void)windowDidEndSheet:(NSNotification *)notification;
//- (void)windowWillStartLiveResize:(NSNotification *)notification    NS_AVAILABLE_MAC(10_6);
//- (void)windowDidEndLiveResize:(NSNotification *)notification   NS_AVAILABLE_MAC(10_6);
//- (void)windowWillEnterFullScreen:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowDidEnterFullScreen:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowWillExitFullScreen:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowDidExitFullScreen:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowWillEnterVersionBrowser:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowDidEnterVersionBrowser:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowWillExitVersionBrowser:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);
//- (void)windowDidExitVersionBrowser:(NSNotification *)notification   NS_AVAILABLE_MAC(10_7);




@end
