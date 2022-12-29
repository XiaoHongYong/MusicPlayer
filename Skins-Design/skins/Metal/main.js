console.log('Enter main.js.');

var CID_HIDE_PLAYLIST = document.getCommandID('CID_HIDE_PLAYLIST');
var CID_HIDE_BOTTOM = document.getCommandID('CID_HIDE_BOTTOM');
var CID_BODY = document.getCommandID('CID_BODY');
var CID_BODY_BG = document.getCommandID('CID_BODY_BG');
var CID_AM_HIDE_LYR_TB = document.getCommandID('CID_AM_HIDE_LYR_TB');
var CID_AM_SHOW_LYR_TB = document.getCommandID('CID_AM_SHOW_LYR_TB');

var elementPlaylistLyrics = document.getElementById('CID_PLAYLIST_LYRICS');
var body = document.getElementById('CID_BODY');
var bodyBg = document.getElementById('CID_BODY_BG');

var STATUS_HIDE_NONE = 0,
    STATUS_HIDE_LYRICS = 1, 
    STATUS_HIDE_PLAYLIST = 2;

var plStatus = profile.getInt('MetalUI', 'PlaylistLyricsHideStatus', STATUS_HIDE_NONE);
if (plStatus != STATUS_HIDE_NONE) {
    doHidePlaylistLyrics();
}

var orgHeight;

function doHidePlaylistLyrics() {
    if (plStatus == STATUS_HIDE_LYRICS) {
        elementPlaylistLyrics.mode = 'left';
    } else if (plStatus == STATUS_HIDE_PLAYLIST) {
        elementPlaylistLyrics.mode = 'right';
    } else {
        elementPlaylistLyrics.mode = 'all';
    }
}

function doShowHideBody() {
    if (orgHeight) {
        // 显示 body
        document.height = orgHeight;
        orgHeight = '';
        body.visible = true;
        bodyBg.visible = false;
    } else {
        orgHeight = document.height;
        document.height = 120;
        body.visible = false;
        bodyBg.visible = true;
    }
}

document.oncommand = function(cmd) {
    console.log('oncommand: ', cmd);
    if (cmd == CID_HIDE_PLAYLIST) {
        plStatus++;
        if (plStatus > 2) {
            plStatus = 0;
        }
        profile.writeInt('MetalUI', 'PlaylistLyricsHideStatus', plStatus);
        doHidePlaylistLyrics();
    } else if (cmd == CID_HIDE_BOTTOM) {
        console.log('oncommand: ', 'CID_HIDE_BOTTOM', orgHeight);
        doShowHideBody();
    }
}

document.onsize = function(w, h) {
    if (h < 170) {
        body.visible = false;
        bodyBg.visible = true;
    } else {
        body.visible = true;
        bodyBg.visible = false;
        orgHeight = '';
    }
}

document.onmouseactivate = function(isActivate) {
    if (isActivate) {
        document.stopAnimation(CID_AM_HIDE_LYR_TB);
        document.startAnimation(CID_AM_SHOW_LYR_TB);
    } else {
        document.stopAnimation(CID_AM_SHOW_LYR_TB);
        document.startAnimation(CID_AM_HIDE_LYR_TB);
    }
}