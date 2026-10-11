// Mini：播放区始终可见；sys / 歌词背景仅 focus·hover 时 FadeIn/Out
console.log('Mini main.js loaded');

var CID_TOGGLE_LYRICS = document.getCommandID('CID_TOGGLE_LYRICS');
var CID_AM_SHOW_SYS = document.getCommandID('CID_AM_SHOW_SYS');
var CID_AM_HIDE_SYS = document.getCommandID('CID_AM_HIDE_SYS');
var CID_AM_SHOW_LYR_BG = document.getCommandID('CID_AM_SHOW_LYR_BG');
var CID_AM_HIDE_LYR_BG = document.getCommandID('CID_AM_HIDE_LYR_BG');

var lyricsArea = document.getElementById('CID_LYRICS');
var lyricsBg = document.getElementById('CID_LYRICS_BG');

// 左固定139 + 右固定(200-166)=34 → 173
var WIDTH_COMPACT = 173;
var WIDTH_MINI = 250;
var WIDTH_FULL = 480;
var lastExpandedWidth = profile.getInt('Mini-FullWidth', WIDTH_FULL);
if (lastExpandedWidth <= WIDTH_MINI) {
    lastExpandedWidth = WIDTH_FULL;
}

var wndActive = false;
var mouseActive = false;
var chromeVisible = false;

function setWindowWidth(w) {
    if (w < WIDTH_MINI) {
        w = WIDTH_COMPACT;
    }
    document.width = w;
}

function rememberExpandedWidth(w) {
    if (w > WIDTH_MINI) {
        lastExpandedWidth = w;
        profile.writeInt('Mini-FullWidth', lastExpandedWidth);
    }
}

function isShowLyrics() {
    return document.width > WIDTH_MINI;
}

function fadeShow(showAnim, hideAnim) {
    document.stopAnimation(hideAnim);
    document.startAnimation(showAnim);
}

function fadeHide(showAnim, hideAnim) {
    document.stopAnimation(showAnim);
    document.startAnimation(hideAnim);
}

function updateChrome() {
    var show = wndActive || mouseActive;
    if (show != chromeVisible) {
        if (show) {
            fadeShow(CID_AM_SHOW_SYS, CID_AM_HIDE_SYS);
        } else {
            fadeHide(CID_AM_SHOW_SYS, CID_AM_HIDE_SYS);
        }
        chromeVisible = show;
    }

    // 仅歌词背景随 focus/hover 渐显隐；播放区与扩展键不变
    var showLyrBg = show && isShowLyrics() && lyricsArea.visible;
    if (showLyrBg) {
        if (!lyricsBg.visible) {
            fadeShow(CID_AM_SHOW_LYR_BG, CID_AM_HIDE_LYR_BG);
        }
    } else {
        if (lyricsBg.visible) {
            fadeHide(CID_AM_SHOW_LYR_BG, CID_AM_HIDE_LYR_BG);
        }
    }
}

function toggleLyrics() {
    if (isShowLyrics()) {
        rememberExpandedWidth(document.width);
        lyricsArea.visible = false;
        setWindowWidth(WIDTH_COMPACT);
    } else {
        lyricsArea.visible = true;
        setWindowWidth(lastExpandedWidth > WIDTH_MINI ? lastExpandedWidth : WIDTH_FULL);
    }
    updateChrome();
}

document.oncommand = function(cmd) {
    if (cmd == CID_TOGGLE_LYRICS) {
        toggleLyrics();
    }
};

document.onactivate = function(active) {
    wndActive = active;
    updateChrome();
};

document.onmouseactivate = function(active) {
    mouseActive = active;
    updateChrome();
};

document.onsize = function(w, h) {
};

if (!isShowLyrics()) {
    lyricsArea.visible = false;
}
updateChrome();
