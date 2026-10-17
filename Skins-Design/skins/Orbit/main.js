// Orbit：首次切换皮肤时自动打开列表/歌词；音量浮动条显隐

var CID_OPEN_PLAYLIST = document.getCommandID('CID_OPEN_PLAYLIST');
var CID_OPEN_LYRICS = document.getCommandID('CID_OPEN_LYRICS');
var CID_TOGGLE_VOLUME = document.getCommandID('CID_TOGGLE_VOLUME');
var CID_VOLUME_POPUP = document.getCommandID('CID_VOLUME_POPUP');
var ID_VOLUME = document.getCommandID('ID_VOLUME');

var volumePopup = document.getElementById('CID_VOLUME_POPUP');
var volumeSlider = document.getElementById('ID_VOLUME');

function hideVolumePopup() {
    if (volumePopup && volumePopup.visible) {
        volumePopup.visible = false;
    }
}

function showVolumePopup() {
    if (!volumePopup) {
        return;
    }
    volumePopup.visible = true;
    if (volumeSlider && volumeSlider.setFocus) {
        volumeSlider.setFocus();
    }
}

function toggleVolumePopup() {
    if (!volumePopup) {
        return;
    }
    if (volumePopup.visible) {
        hideVolumePopup();
    } else {
        showVolumePopup();
    }
}

function isVolumeTarget(target) {
    if (!target) {
        return false;
    }
    var id = target.id;
    return id == ID_VOLUME || id == CID_VOLUME_POPUP;
}

document.oncommand = function(cmd) {
    if (cmd == CID_TOGGLE_VOLUME) {
        toggleVolumePopup();
    }
};

// 点击音量条以外区域：松手时收起，并 return true 吃掉 Click（避免再触发底下按钮的 command）
document.onpointerup = function(target) {
    if (!volumePopup || !volumePopup.visible) {
        return;
    }
    if (isVolumeTarget(target)) {
        return;
    }
    hideVolumePopup();
    return true;
};

if (profile.getInt('Orbit-AutoOpened', 0) == 0) {
    document.postCommand(CID_OPEN_PLAYLIST);
    document.postCommand(CID_OPEN_LYRICS);
    profile.writeInt('Orbit-AutoOpened', 1);
}
