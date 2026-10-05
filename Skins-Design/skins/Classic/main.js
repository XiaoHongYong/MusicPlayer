var CID_HIDE_PLAYLIST = document.getCommandID('CID_HIDE_PLAYLIST');
var CID_HIDE_BOTTOM = document.getCommandID('CID_HIDE_BOTTOM');
var CID_BODY = document.getCommandID('CID_BODY');
var CID_BODY_BG = document.getCommandID('CID_BODY_BG');
var CID_AM_HIDE_LYR_TB = document.getCommandID('CID_AM_HIDE_LYR_TB');
var CID_AM_SHOW_LYR_TB = document.getCommandID('CID_AM_SHOW_LYR_TB');

var lyricsElement = document.getElementById('CID_LYRICS');

document.oncommand = function(cmd) {
}

lyricsElement.onmouseenter = function() {
    document.stopAnimation(CID_AM_HIDE_LYR_TB);
    document.startAnimation(CID_AM_SHOW_LYR_TB);
}

lyricsElement.onmouseleave = function() {
    document.stopAnimation(CID_AM_SHOW_LYR_TB);
    document.startAnimation(CID_AM_HIDE_LYR_TB);
}
