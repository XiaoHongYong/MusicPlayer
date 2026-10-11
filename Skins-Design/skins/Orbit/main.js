// Orbit：首次切换到本皮肤时自动打开列表/歌词副窗；之后由 LatestOpenedSkinWnds 恢复

var CID_OPEN_PLAYLIST = document.getCommandID('CID_OPEN_PLAYLIST');
var CID_OPEN_LYRICS = document.getCommandID('CID_OPEN_LYRICS');

if (profile.getInt('Orbit-AutoOpened', 0) == 0) {
    document.postCommand(CID_OPEN_PLAYLIST);
    document.postCommand(CID_OPEN_LYRICS);
    profile.writeInt('Orbit-AutoOpened', 1);
}
