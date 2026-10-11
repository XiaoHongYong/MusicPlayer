// Orbit 歌词副窗：悬停显示同步工具条

var CID_AM_HIDE_LYR_TB = document.getCommandID('CID_AM_HIDE_LYR_TB');
var CID_AM_SHOW_LYR_TB = document.getCommandID('CID_AM_SHOW_LYR_TB');

var lyricsArea = document.getElementById('CID_LYRICS');
if (lyricsArea) {
    lyricsArea.onmouseenter = function() {
        document.stopAnimation(CID_AM_HIDE_LYR_TB);
        document.startAnimation(CID_AM_SHOW_LYR_TB);
    }
    lyricsArea.onmouseleave = function() {
        document.stopAnimation(CID_AM_SHOW_LYR_TB);
        document.startAnimation(CID_AM_HIDE_LYR_TB);
    }
}
