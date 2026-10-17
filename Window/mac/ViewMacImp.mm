//
//  ViewMacImp.m
//
//  Created by Hongyong Xiao on 11/18/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <Carbon/Carbon.h>
#import "../WindowTypes.h"
#import "ViewMacImp.h"
#import "Window.h"
#import "../Cursor.h"


@implementation ViewMacImp

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    mBaseWnd = nil;
    mCursorToSet = nullptr;
    mCursorDefault = [NSCursor arrowCursor];
    mIsTextInputMode = false;
    mIsMarkedText = false;
    mMagnification = 0;

    return self;
}

- (void)dealloc {
    [super dealloc];
}

- (void)drawRect:(NSRect)rect {
    // Clear the drawing rect.
    // [[NSColor clearColor] set];
    // NSRectFill(rect);



    //    NSRect r = [self frame];
    //    printf("Frame: x: %f, y: %f, width: %f, height: %f", r.origin.x, r.origin.y, r.size.width, r.size.height);
    //    printf("ClipRect: x: %f, y: %f, width: %f, height: %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    // A boolean tracks the previous shape of the window. If the shape changes, it's necessary for the
    // window to recalculate its shape and shadow.
    /*    bool shouldDisplayWindow = NO;
    // If the window transparency is > 0.7, draw the circle, otherwise, draw the pentagon. 
    if ([[self window] alphaValue] > 0.7) {
        shouldDisplayWindow = (showingPentagon == YES);
        showingPentagon = NO;
        [circleImage compositeToPoint:NSZeroPoint operation:NSCompositeSourceOver];
    } else {
        shouldDisplayWindow = (showingPentagon == NO);
        showingPentagon = YES;
        [pentagonImage compositeToPoint:NSZeroPoint operation:NSCompositeSourceOver];
    }
    // reset the window shape and shadow.
    if (shouldDisplayWindow) {
        [[self window] display];
        [[self window] setHasShadow:NO];
        [[self window] setHasShadow:YES];
    }*/

    CRect rcClip;
    NSRectToRect(rect, rcClip);

    // 需要将 mac 下的坐标体系转换一下.
    int h = rcClip.height();
    rcClip.top = [self frame].size.height - rcClip.bottom;
    rcClip.bottom = rcClip.top + h;

    mBaseWnd->onPaint(mBaseWnd->getMemGraphics(), &rcClip);
}

- (void)setOwnerBaseWnd:(Window*)baseWnd {
    mBaseWnd = baseWnd;
}


// 在编辑框开始/结束编辑时需要调用
- (void)startTextInput {
    mIsTextInputMode = true;
    mCaretPosition = CPoint(0, 0);
    mIsMarkedText = false;
}

- (void)endTextInput {
    mIsTextInputMode = false;
}

// 编辑控件的光标位置改变后需要调用此函数
- (void)caretPositionChanged:(CPoint)point {
    mCaretPosition = point;
}

- (void) keyDown:(NSEvent *)theEvent {
    const uint32_t NEEDED_FLAGS = NSEventModifierFlagShift | NSEventModifierFlagControl
        | NSEventModifierFlagOption | NSEventModifierFlagCommand | NSEventModifierFlagFunction;

    uint32_t modifierFlags = [theEvent modifierFlags] & NEEDED_FLAGS;
    int code = [theEvent keyCode];

    if (mIsTextInputMode) {
        if (mIsMarkedText || (code != kVK_Return && code != kVK_Escape && code < kVK_Delete &&
            code != VK_TAB && (modifierFlags & ~NSEventModifierFlagShift) == 0))
        {
            // 输入法来处理键盘输入
            if ([[self inputContext] handleEvent:theEvent]) {
                return;
            }
        }
    }

    mBaseWnd->onKeyDown(code, modifierFlags);
}

- (void) keyUp:(NSEvent *)theEvent {
    uint32_t modifierFlags = [theEvent modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
    int code = [theEvent keyCode];

    mBaseWnd->onKeyUp(code, modifierFlags);
}

// 失焦时第一次点击也派发到视图，避免只激活窗口、控件无响应
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent {
    return YES;
}

- (void) mouseDown:(NSEvent *)theEvent {
    if (mIsTextInputMode && [[self inputContext] handleEvent:theEvent]) {
        return;
    }

    mInitPt = [theEvent locationInWindow];

    uint32_t flags = (uint32_t)[theEvent modifierFlags];
    CPoint point = NSPointToCPoint(mInitPt, [self frame].size.height);

    flags |= MK_LBUTTON;

    if ([theEvent clickCount] > 1) {
        mBaseWnd->onLButtonDblClk(flags, point);
    } else {
        mBaseWnd->onLButtonDown(flags, point);
    }
}

- (void) mouseUp:(NSEvent *)theEvent {
    if (mIsTextInputMode && [[self inputContext] handleEvent:theEvent]) {
        return;
    }

    mBaseWnd->onLButtonUp((uint32_t)[theEvent modifierFlags],
        NSPointToCPoint([theEvent locationInWindow], [self frame].size.height));
}

- (void) rightMouseDown:(NSEvent *)theEvent {
    uint32_t flags = (uint32_t)[theEvent modifierFlags];
    CPoint point = NSPointToCPoint([theEvent locationInWindow], [self frame].size.height);

    mBaseWnd->onRButtonDown(flags, point);
}

- (void) rightMouseUp:(NSEvent *)theEvent {
    mBaseWnd->onRButtonUp((uint32_t)[theEvent modifierFlags],
        NSPointToCPoint([theEvent locationInWindow], [self frame].size.height));
}

- (void) setCursor:(Cursor *)cursor {
    mCursorToSet = cursor;
}

- (void) mouseMoved:(NSEvent *)theEvent {
    bool isCursorChanged = mCursorToSet != nullptr;
    mCursorToSet = nullptr;

    mBaseWnd->onMouseMove(NSPointToCPoint([theEvent locationInWindow], [self frame].size.height));

    if (mCursorToSet) {
        mCursorToSet->set();
    } else if (isCursorChanged) {
        [mCursorDefault set];
    }
}

- (void) mouseDragged:(NSEvent *)theEvent {
    if (mIsTextInputMode && [[self inputContext] handleEvent:theEvent]) {
        return;
    }

    NSWindow *window = [self window];
    uint32_t flags = (uint32_t)[theEvent modifierFlags];
    flags |= MK_LBUTTON;

    bool isCursorChanged = mCursorToSet != nullptr;
    mCursorToSet = nullptr;

    mBaseWnd->onMouseDrag(flags,
        NSPointToCPoint([theEvent locationInWindow], [window frame].size.height));

    if (mCursorToSet) {
        mCursorToSet->set();
    } else if (isCursorChanged) {
        [mCursorDefault set];
    }

/* 拖动窗口位置.
    NSRect screenVisibleFrame = [[NSScreen mainScreen] visibleFrame];
    NSRect windowFrame = [window frame];
    NSPoint newOrigin = windowFrame.origin;

    // Get the mouse location in window coordinates.
    NSPoint currentLocation = [theEvent locationInWindow];
    // Update the origin with the difference between the new mouse location and the old mouse location.
    newOrigin.x += (currentLocation.x - mInitPt.x);
    newOrigin.y += (currentLocation.y - mInitPt.y);

    // Don't let window get dragged up under the menu bar
    if ((newOrigin.y + windowFrame.size.height) > (screenVisibleFrame.origin.y + screenVisibleFrame.size.height)) {
        newOrigin.y = screenVisibleFrame.origin.y + (screenVisibleFrame.size.height - windowFrame.size.height);
    }

    // Move the window to the new location
    [window setFrameOrigin:newOrigin];*/
}

- (void)magnifyWithEvent:(NSEvent *)event {
    CGFloat magnification = [event magnification];
    mMagnification += magnification * 20;
    while (abs(mMagnification) > 1) {
        int flag = mMagnification > 0 ? 1 : -1;
        mBaseWnd->onMagnify(flag);
        mMagnification -= flag;
    }
}

///////////////////////////////////////////////////////////////////////////
// View messages

/*- (void)viewWillStartLiveResize {
    NSRect rc = [self frame];
    mBaseWnd->onSize(rc.size.width, rc.size.height);
}*/

- (void)viewDidEndLiveResize {
    //    NSRect rc = [self frame];
    //    mBaseWnd->onSize(rc.size.width, rc.size.height);
}

//
// 输入法需要的接口
//

- (void)doCommandBySelector:(SEL)aSelector {
    [super doCommandBySelector:aSelector]; // NSResponder's implementation will do nicely
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange {
    mIsMarkedText = false;

    if ([aString length] == 0) {
        mBaseWnd->onInputText("");
    } else {
        if ([aString isKindOfClass:[NSString class]]) {
            mBaseWnd->onInputText([aString cStringUsingEncoding: NSUTF8StringEncoding]);
        } else if ([aString isKindOfClass:[NSAttributedString class]]) {
            mBaseWnd->onInputText([[aString string] cStringUsingEncoding: NSUTF8StringEncoding]);
        }
    }

    [self unmarkText];
    [[self inputContext] invalidateCharacterCoordinates]; // recentering
    [self setNeedsDisplay:YES];
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)newSelection replacementRange:(NSRange)replacementRange {

    if ([aString length] == 0) {
        mBaseWnd->onInputText("");
        mIsMarkedText = false;
    } else {
        mIsMarkedText = true;
        if ([aString isKindOfClass:[NSString class]]) {
            mBaseWnd->onInputMarkedText([aString cStringUsingEncoding: NSUTF8StringEncoding]);
        } else if ([aString isKindOfClass:[NSAttributedString class]]) {
            mBaseWnd->onInputMarkedText([[aString string] cStringUsingEncoding: NSUTF8StringEncoding]);
        }
    }

    [[self inputContext] invalidateCharacterCoordinates];
}

- (void)unmarkText {
    mIsMarkedText = false;
    [[self inputContext] discardMarkedText];
}

- (NSRange)selectedRange {
    return NSMakeRange(0, 0);
}

- (NSRange)markedRange {
    return NSMakeRange(0, 0);
}

- (BOOL)hasMarkedText {
    return mIsMarkedText;
}

/* Returns attributed string specified by range. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc).
*/
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange {
    return nil;
}

/* Returns an array of attribute names recognized by the receiver.
*/
- (NSArray *)validAttributesForMarkedText {
    // We only allow these attributes to be set on our marked text (plus standard attributes)
    // NSMarkedClauseSegmentAttributeName is important for CJK input, among other uses
    // NSGlyphInfoAttributeName allows alternate forms of characters
    return [NSArray arrayWithObjects:NSMarkedClauseSegmentAttributeName, NSGlyphInfoAttributeName, nil];
}

- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange {

    NSPoint pt = NSMakePoint(mCaretPosition.x, mCaretPosition.y);

    // Skin 的 Y 坐标体系和 mac 下的是颠倒的
    pt.y = [[self window] frame].size.height - pt.y;

    // 将 window 内的坐标转换为 screen 的
    pt = [[self window] convertPointToScreen: pt];

    // 返回
    NSRect rc;
    rc.origin = pt;
    rc.size = NSMakeSize(40, 20);
    return rc;
}

/* Returns the index for character that is nearest to point. point is in the screen coordinate system.
*/
- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
    return 0;
}

- (NSInteger)windowLevel {
    // This method is optional but easy to implement
    return [[self window] level];
}

//
// 输入法接口结束
//

@end
