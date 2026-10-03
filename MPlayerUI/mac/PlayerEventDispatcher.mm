//
//  PlayerEventDispatcher.m
//
//  Created by Xiao Hongyong on 8/1/13.
//
//
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import "PlayerEventDispatcher.h"
#import "../../LyricsLib/CurrentLyrics.h"


@interface _PlayerEventDispatcherInternal : NSObject
{
    CADisplayLink *displayLink;
    CFAbsoluteTime lastTick;
}

@end

#import "../MPlayer/Player.h"
#import "../MPlayerApp.h"

@implementation _PlayerEventDispatcherInternal

-(void)StartLyrDrawUpdate {
    dispatch_async(dispatch_get_main_queue(), ^{
        // 用 CADisplayLink 替代 NSTimer：回调与 vsync 对齐，削减与显示刷新不同步造成的
        // 参差 CA::commit。节流闸在 onDisplayLink 内保持平均 ~40ms（原有 25fps 观感）。
        displayLink = [NSScreen.mainScreen displayLinkWithTarget:self selector:@selector(onDisplayLink:)];
        [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
        [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSEventTrackingRunLoopMode];
        lastTick = 0;
    });
}

-(void)StopLyrDrawUpdate {
    if (displayLink != nil) {
        [displayLink invalidate];
        displayLink = nil;
    }
}

-(void)onDisplayLink:(CADisplayLink *)sender {
    // 40ms 节流：不升 fps，仅把每次 tick 对齐到 vsync 边界。
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (now - lastTick < 0.040) {
        return;
    }
    lastTick = now;

    if (g_player.isUseSeekTimeAsPlayingTime()) {
        return;
    }

    int nPlayPos = g_player.getPlayPos();
    g_currentLyrics.SetPlayElapsedTime(nPlayPos);

    MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);

    dispatchPlayPosEvent(nPlayPos);
}

@end

CPlayerEventDispatcher::CPlayerEventDispatcher() {
    m_nTimeOutUpdateLyr = 40;
    m_pInternal = [[_PlayerEventDispatcherInternal alloc] init];
}

void CPlayerEventDispatcher::startLyrDrawUpdate() {
    [(_PlayerEventDispatcherInternal*)m_pInternal StartLyrDrawUpdate];
}

void CPlayerEventDispatcher::stopLyrDrawUpdate() {
    [(_PlayerEventDispatcherInternal*)m_pInternal StopLyrDrawUpdate];
}
