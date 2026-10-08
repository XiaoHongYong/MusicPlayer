// console.log('Enter main.js (Glass).');

var CID_SHOW_PLAYLIST = document.getCommandID('CID_SHOW_PLAYLIST');
var CID_SHOW_LYRICS = document.getCommandID('CID_SHOW_LYRICS');
var CID_TOGGLE_VISUALIZER = document.getCommandID('CID_TOGGLE_VISUALIZER');
var CID_AM_HIDE_LYR_TB = document.getCommandID('CID_AM_HIDE_LYR_TB');
var CID_AM_SHOW_LYR_TB = document.getCommandID('CID_AM_SHOW_LYR_TB');

var playlistArea = document.getElementById('CID_PLAYLIST');
var lyricsArea = document.getElementById('CID_LYRICS');
var visualizerPanel = document.getElementById('CID_VISUALIZER_PANEL');

function applyView(showLyrics) {
    playlistArea.visible = !showLyrics;
    lyricsArea.visible = showLyrics;
}

function applyVisualizer(show) {
    visualizerPanel.visible = show;
}

// 列表/歌词切换: tab 点击与脚本恢复都走这条命令路径 ——
// 命令先经 CSkinToolbar::onCommand 同步 tab 选中态, 再到这里同步可见性并记住状态
document.oncommand = function(cmd) {
    if (cmd == CID_SHOW_PLAYLIST || cmd == CID_SHOW_LYRICS) {
        var showLyrics = (cmd == CID_SHOW_LYRICS);
        applyView(showLyrics);
        profile.writeInt('Glass-ShowLyrics', showLyrics ? 1 : 0);
    } else if (cmd == CID_TOGGLE_VISUALIZER) {
        var showVis = !visualizerPanel.visible;
        applyVisualizer(showVis);
        profile.writeInt('Glass-ShowVisualizer', showVis ? 1 : 0);
    }
}

// 恢复上次的视图状态(列表 / 歌词)。
// 默认是列表, 与 XML 中 tab 的 Checked="TRUE" 一致, 无需处理;
// 若上次是歌词, 通过 postCommand 走正常命令路径, tab 选中态一并同步
if (profile.getInt('Glass-ShowLyrics', 0) != 0) {
    document.postCommand(CID_SHOW_LYRICS);
}

// 恢复频谱面板显隐；默认显示
if (profile.getInt('Glass-ShowVisualizer', 1) == 0) {
    applyVisualizer(false);
}

// 鼠标悬停歌词区时渐显工具条
lyricsArea.onmouseenter = function() {
    document.stopAnimation(CID_AM_HIDE_LYR_TB);
    document.startAnimation(CID_AM_SHOW_LYR_TB);
}

lyricsArea.onmouseleave = function() {
    document.stopAnimation(CID_AM_SHOW_LYR_TB);
    document.startAnimation(CID_AM_HIDE_LYR_TB);
}
