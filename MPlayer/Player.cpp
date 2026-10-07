#include "../MPlayerUI/MPlayerApp.h"
#include "Player.h"
#include "MediaScanner.h"
#include "MetadataInference/MetadataInference.h"
#include "../MPlayerUI/PlayListFile.h"
#include "../MPlayerUI/OnlineSearch.h"
#include "../LyricsLib/CurrentLyrics.h"
#include "../MPlayerEngine/MPlayerCore.h"

#include <algorithm>

#ifdef _WIN32
#include "../MPlayerUI/win32/PlayerEventDispatcher.h"
#endif

#ifdef _MAC_OS
#include "../MPlayerEngine/mac/CoreAVPlayer.h"
#include "../MPlayerUI/mac/PlayerEventDispatcher.h"
#endif


CPlayer g_player;
CPlayerEventDispatcher g_playerEventDispatcher;


string getMediaFormat(cstr_t url) {
    auto ext = urlGetExt(url);
    if (ext[0] == '.') {
        ext++;

        return toLower(ext);
    }

    return "Unknown";
}

void getArtistTitleFromFileName(string &artist, string &title, cstr_t fileName) {
    string fileTitle = fileGetTitle(fileName);

    if (!strSplit(fileTitle.c_str(), " - ", artist, title)
        && !strSplit(fileTitle.c_str(), '-', artist, title)) {
        artist.resize(0);
        title = fileTitle;
    }

    trimStr(artist);
    trimStr(title);
}

string formatMediaTitle(cstr_t artist, cstr_t title) {
    string mediaTitle = artist;
    if (mediaTitle.size() > 0 && !isEmptyString(title)) {
        mediaTitle += " - ";
    }
    mediaTitle += title;

    return mediaTitle;
}

void formatFullTitle(cstr_t szArtist, cstr_t szTitle, cstr_t szUri, string &strFullTitle) {
    strFullTitle = formatMediaTitle(szArtist, szTitle);
    if (strFullTitle.empty()) {
        strFullTitle = urlGetTitle(szUri);
    }
}

string formatDuration(int nTimeSec) {
    char szTime[64];
    int nHour, nMinute, nSec;

    nSec = nTimeSec % 60;
    nMinute = nTimeSec / 60;
    nHour = nMinute / 60;
    nMinute = nMinute % 60;
    if (nHour > 0) {
        snprintf(szTime, CountOf(szTime), "%02d:%02d:%02d", nHour, nMinute, nSec);
    } else {
        snprintf(szTime, CountOf(szTime), "%02d:%02d", nMinute, nSec);
    }

    return szTime;
}

CPlayer::CPlayer() {
    emptyStr(m_szFullTitle);
    emptyStr(m_szArtist);
    emptyStr(m_szTitle);
    emptyStr(m_szSrcMedia);

    m_isUseSeekTimeAsPlayingTime = false;
    m_mediaLength = 0;
    m_state = PS_STOPPED;
    m_mediaLib = nullptr;

    m_currentPlaylist = newPlaylist();
}

CPlayer::~CPlayer() {
    if (m_mediaLib) {
        delete m_mediaLib;
    }
}

void CPlayer::onEndOfPlaying() {
    if (m_isAutoPlayNext) {
        recordPlayHistory(true);

        if (m_loopMode == MP_LOOP_TRACK) {
            m_playHistoryRecorded = false;
            play();
            return;
        }

        auto ret = doNext(false);
        if (ret == ERR_OK) {
            play();
        } else {
            m_state = PS_STOPPED;
        }
    }

    // m_state = m_playerCore->getState();

    notifyPlayStateChanged();
}

void CPlayer::onErrorOccured(const char *errorCode) {
    // TODO: 如果当前播放的歌曲出现错误，自动切换到下一首
}

void CPlayer::onInit() {
    // open title filters
    string strFilter;
    if (readFileByBom(getTitleFilterFile().c_str(), strFilter)) {
        strSplit(strFilter.c_str(), '\n', m_vTitleFilters);
        for (size_t i = 0; i < m_vTitleFilters.size();) {
            if (m_vTitleFilters[i].empty() || startsWith(m_vTitleFilters[i].c_str(), ";")) {
                m_vTitleFilters.erase(m_vTitleFilters.begin() + i);
            } else {
                i++;
            }
        }
        trimStr(m_vTitleFilters, '\r');
        trimStr(m_vTitleFilters, ' ');
    }

#ifdef _MAC_OS
    // Visualizer / 音频分析需要 PCM 旁路；CoreAVPlayer（AVPlayer）无 PCM，改用 MPlayerCore。
    m_playerCore = new MPlayerCore();
    // m_playerCore = new CoreAVPlayer();
#else
    m_playerCore = new MPlayerCore();
#endif
    m_playerCore->setCallback(this);

    m_mediaLib = new CMediaLibrary;
    m_mediaLib->init();

    // get loop mode
    auto szValue = g_profile.getString(SZ_SECT_PLAYER, "repeat", "off");
    if (strcasecmp(szValue, "all") == 0) {
        m_loopMode = MP_LOOP_ALL;
    } else if (strcasecmp(szValue, "track") == 0) {
        m_loopMode = MP_LOOP_TRACK;
    } else {
        m_loopMode = MP_LOOP_OFF;
    }

    // shuffle
    m_isShuffle = g_profile.getBool(SZ_SECT_PLAYER, "shuffle", false);

    // mute
    m_isMute = g_profile.getBool(SZ_SECT_PLAYER, "mute", false);

    // volume
    m_volume = g_profile.getInt(SZ_SECT_PLAYER, "volume", 30);

    m_playerCore->setVolume(m_isMute ? 0 : m_volume);

    // Load current playlist
    m_currentPlaylist = m_mediaLib->getNowPlaying();

    // current playing music
    setCurrentMediaInNowPlaying(g_profile.getInt(SZ_SECT_PLAYER, "NowPlayingIdx", 0));

    g_playerEventDispatcher.init();

    if (g_profile.getBool("StartPlayAtStartup", false)) {
        play();
    }
}

void CPlayer::onQuit() {
    g_playerEventDispatcher.quit();

    saveNowPlaying();

    // save latest playing song file
    g_profile.writeInt(SZ_SECT_PLAYER, "NowPlayingIdx", m_idxCurrentMedia);

    assert(m_playerCore);
    stop();
    delete m_playerCore;
}

// control player
void CPlayer::play() {
    if (m_state == PS_PLAYING) {
        seekTo(0);
    } else if (m_state == PS_STOPPED) {
        if (m_currentMedia) {
            m_state = PS_PLAYING;

            m_isAutoPlayNext = true;
            m_isCurMediaPlayed = true;

            if (m_playerCore->play(m_currentMedia->url.c_str(), m_currentMedia.get())) {
                m_countPlayFailed = 0;
            } else {
                if (!isFileExist(m_currentMedia->url.c_str())) {
                    m_mediaLib->setDeleted(m_currentMedia.get());
                }

                m_countPlayFailed++;
                MPlayerApp::getEventsDispatcher()->postExecInUIThreadDelayed([]() {
                    // Delay play next music
                    g_player.playNextOnFailed();
                }, 300);
            }
        } else {
            next();
            if (m_currentMedia) {
                play();
            }
        }
    } else {
        // paused
        m_state = PS_PLAYING;
        m_playerCore->unpause();
    }

    notifyPlayStateChanged();
}

void CPlayer::pause() {
    if (m_state == PS_PLAYING) {
        m_state = PS_PAUSED;
        m_playerCore->pause();
        notifyPlayStateChanged();
    }
}

void CPlayer::playPause() {
    if (m_state == PS_PLAYING) {
        pause();
    } else {
        play();
    }
}

void CPlayer::stop() {
    if (m_state != PS_STOPPED) {
        m_state = PS_STOPPED;
        m_isAutoPlayNext = false;
        m_playerCore->stop();
        notifyPlayStateChanged();
    }
}

void CPlayer::prev() {
    int nOld = m_idxCurrentMedia;

    {
        RMutexAutolock autolock(m_mutexDataAccess);

        if (m_isShuffle) {
            if (m_idxCurrentShuffleMedia <= 1) {
                // in shuffle mode, only can back to 0
                return;
            }

            assert(m_vShuffleMedia.size() == m_currentPlaylist->getCount());
            m_idxCurrentShuffleMedia--;
            m_idxCurrentMedia = m_vShuffleMedia[m_idxCurrentShuffleMedia - 1];
            // m_idxCurrentMedia = rand() % m_currentPlaylist->getCount();
        } else {
            if (m_idxCurrentMedia > 0) {
                m_idxCurrentMedia--;
            } else if (m_loopMode == MP_LOOP_ALL) {
                m_idxCurrentMedia = m_currentPlaylist->getCount() - 1;
            }
        }
    }

    if (nOld != m_idxCurrentMedia) {
        // Notify changed
        currentMediaChanged();
    }
}

void CPlayer::next() {
    doNext(true);
}

void CPlayer::seekTo(uint32_t nMsPos) {
    m_playerCore->seek(nMsPos);

    notifySeek();
}

// get player state and song info
PlayerState CPlayer::getPlayerState() {
    return m_state;
}

uint32_t CPlayer::getMediaLength() {
    return m_mediaLength;
}

uint32_t CPlayer::getPlayPos() {
    return m_playerCore->getPos();
}

void CPlayer::recordPlayHistory(bool forceEnough) {
    if (m_playHistoryRecorded || !m_mediaLib || !m_currentMedia) {
        return;
    }
    if (m_currentMedia->ID == MEDIA_ID_INVALID) {
        return;
    }
    if (!forceEnough) {
        uint32_t pos = getPlayPos();
        uint32_t dur = m_mediaLength;
        if (pos < 10000 && (dur == 0 || pos * 5 < dur)) {
            return;
        }
    }
    m_playHistoryRecorded = true;
    m_mediaLib->addPlayHistory(m_currentMedia->ID, nullptr);

    CEventPlayHistoryRecorded *evt = new CEventPlayHistoryRecorded();
    evt->eventType = ET_PLAY_HISTORY_RECORDED;
    evt->songId = m_currentMedia->ID;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(evt);
}

void CPlayer::maybeRecordPlayHistory() {
    if (m_state != PS_PLAYING) {
        return;
    }
    recordPlayHistory(false);
}

void CPlayer::setToNextLoopMode() {
    LoopMode loopMode = m_loopMode;

    if (loopMode == MP_LOOP_OFF) {
        loopMode = MP_LOOP_ALL;
    } else if (loopMode == MP_LOOP_ALL) {
        loopMode = MP_LOOP_TRACK;
    } else {
        loopMode = MP_LOOP_OFF;
    }

    setLoop(loopMode);
}

void CPlayer::setLoop(LoopMode loopMode) {
    cstr_t szValue;
    if (loopMode == MP_LOOP_ALL) {
        szValue = "all";
    } else if (loopMode == MP_LOOP_TRACK) {
        szValue = "track";
    } else {
        szValue = "off";
    }

    g_profile.writeString(SZ_SECT_PLAYER, "repeat", szValue);

    m_loopMode = loopMode;
    notifySettingsChanged(IMPEvent::MPS_LOOP, loopMode);
}

void CPlayer::setShuffle(bool value) {
    g_profile.writeBool(SZ_SECT_PLAYER, "shuffle", value);
    m_isShuffle = value;

    notifySettingsChanged(IMPEvent::MPS_SHUFFLE, m_isShuffle);
}

LoopMode CPlayer::getLoop() {
    return m_loopMode;
}

bool CPlayer::isShuffle() {
    return m_isShuffle;
}

int CPlayer::getVolume() {
    return m_volume;
}

void CPlayer::setVolume(int value) {
    assert(value >= 0 && value <= 100);
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }

    g_profile.writeBool(SZ_SECT_PLAYER, "mute", false);
    g_profile.writeInt(SZ_SECT_PLAYER, "volume", value);
    m_volume = value;
    m_isMute = false;

    m_playerCore->setVolume(value);

    notifySettingsChanged(IMPEvent::MPS_VOLUME, m_volume);
    notifySettingsChanged(IMPEvent::MPS_MUTE, m_isMute);
}

bool CPlayer::isMute() {
    return m_isMute;
}

void CPlayer::setMute(bool value) {
    g_profile.writeBool(SZ_SECT_PLAYER, "mute", value);

    m_playerCore->setVolume(0);

    m_isMute = value;
    notifySettingsChanged(IMPEvent::MPS_MUTE, m_isMute);
}

// -100 ~ 100
void CPlayer::setBalance(int balance) {
    assert(balance >= -100 && balance <= 100);
    if (balance < -100) {
        balance = -100;
    } else if (balance > 100) {
        balance = 100;
    }
    m_balance = balance;
    m_playerCore->setBalance(balance);

    notifySettingsChanged(IMPEvent::MPS_BALANCE, m_balance);
}

int CPlayer::getBalance() {
    return m_balance;
}

void CPlayer::setEQ(const EQualizer *eq) {
    m_equalizer = *eq;
    m_playerCore->setEQ(eq);
    notifyEQSettingsChanged(&m_equalizer);
}

void CPlayer::getEQ(EQualizer *eq) {
    *eq = m_equalizer;
}

void CPlayer::clearNowPlaying() {
    setNowPlayingModified(true);
    m_currentPlaylist->clear();
}

void CPlayer::addDirToNowPlaying(cstr_t szDir, bool bIncSubDir) {
    FileFind find;

    if (!find.openDir(szDir)) {
        return;
    }

    string strDir = szDir;
    dirStringAddSep(strDir);

    while (find.findNext()) {
        if (find.isCurDir()) {
            if (bIncSubDir) {
                string strFile = strDir + find.getCurName();
                addDirToNowPlaying(strFile.c_str(), bIncSubDir);
            }
        } else {
            string strFile = strDir + find.getCurName();
            addToNowPlaying(strFile.c_str());
        }
    }
}

void CPlayer::addToNowPlaying(cstr_t szMedia) {
    cstr_t szExt = fileGetExt(szMedia);
    if (isEmptyString(szExt)) {
        return;
    }

    if (isExtPlaylistFile(szExt)) {
        auto playlist = ::loadPlaylist(szMedia);
        m_currentPlaylist->append(playlist.get());
    } if (isExtAudioFile(szExt)) {
        m_currentPlaylist->insertItem(-1, newMedia(szMedia));
    }
}

void CPlayer::addToNowPlaying(const PlaylistPtr &playlist) {
    m_currentPlaylist->append(playlist.get());
}

bool CPlayer::isCurrentMediaFileExist() {
    if (isEmptyString(m_szSrcMedia) || !isFileExist(m_szSrcMedia)) {
        return false;
    }
    return true;
}

bool CPlayer::isCurrentMediaTempPlaying() {
    // 当前播放的歌曲是否为临时播放的？
    return m_currentMedia != m_currentPlaylist->getItem(m_idxCurrentMedia);
}

void CPlayer::playMedia(MediaPtr &media) {
    stop();
    int nIndex = 0;
    if (m_currentPlaylist->getItemIndex(media, nIndex) == ERR_OK) {
        m_idxCurrentMedia = nIndex;
    }
    setCurrentMedia(media);
    play();
}

void CPlayer::playMedia(int index) {
    stop();
    setCurrentMediaInNowPlaying(index);
    play();
}

void CPlayer::getFileOpenDlgExtention(string &strExtentions) const {
    // static char    szSupportedExtion[] = "All supported file\0*.m3u;*.pls;*.wpl;*.mp3;*.mp2;*.mpa;*.mp4;*.m4a;*.wav;*.ogg;*.wma\0Playlist file\0*.pls;*.wpl;*.m3u\0Mp3 file\0*.mp3;*.mp2;*.mpa\0WMA file\0*.wma\0MP4 file\0*.mp4;*.m4a\0Ogg file\0*.ogg\0Wave file\0*.wav\0All Files\0*.*\0\0";
    static char szSupportedExtion[] = "All supported file\0*.m3u;*.pls;*.wpl;*.mp3;*.mp2;*.mpa;*.m4a;*.wav;*.ogg;*.wma\0Playlist file\0*.pls;*.wpl;*.m3u\0Mp3 file\0*.mp3;*.mp2;*.mpa\0WMA file\0*.wma\0MP4 file\0*.m4a\0Ogg file\0*.ogg\0Wave file\0*.wav\0All Files\0*.*\0\0";
    strExtentions.append(szSupportedExtion, CountOf(szSupportedExtion));
}

void CPlayer::getSupportedExtentions(vector<string> &vExtentions) const {
    VecStrings exts;
    strSplit(m_playerCore->getFileExtentions(), '|', exts);

    for (int i = 0; i < (int)exts.size(); i += 2) {
        auto &ext = exts[i];
        trimStr(ext);
        if (!ext.empty()) {
            vExtentions.push_back(ext);
        }
    }

    vExtentions.push_back(".m3u");
    vExtentions.push_back(".pls");
    vExtentions.push_back(".wpl");
}

bool CPlayer::isExtSupported(cstr_t szExt) {
    return isExtAudioFile(szExt) || isExtPlaylistFile(szExt);
}

bool CPlayer::isExtAudioFile(cstr_t szExt) {
    if (m_audioFileExts.empty()) {
        VecStrings exts;
        strSplit(m_playerCore->getFileExtentions(), '|', exts);

        for (int i = 0; i < (int)exts.size(); i += 2) {
            auto &ext = exts[i];
            trimStr(ext);
            if (!ext.empty()) {
                m_audioFileExts.insert(ext);
            }
        }
    }

    string extLowerCase = toLower(szExt);
    return m_audioFileExts.find(extLowerCase) != m_audioFileExts.end();
}

bool CPlayer::isExtPlaylistFile(cstr_t szExt) {
    if (strcasecmp(".m3u", szExt) == 0) {
        return true;
    }
    if (strcasecmp(".wpl", szExt) == 0) {
        return true;
    }
    if (strcasecmp(".pls", szExt) == 0) {
        return true;
    }

    return false;
}


void CPlayer::addDirToMediaLib(cstr_t szDir, bool bIncSubDir) {
    FileFind find;

    if (!find.openDir(szDir)) {
        return;
    }

    string strDir = szDir;
    dirStringAddSep(strDir);

    while (find.findNext()) {
        if (find.isCurDir()) {
            if (bIncSubDir) {
                string strFile = strDir + find.getCurName();
                addDirToMediaLib(strFile.c_str(), bIncSubDir);
            }
        } else {
            string strFile = strDir + find.getCurName();
            addFileToMediaLib(strFile.c_str());
        }
    }
}

void CPlayer::addFileToMediaLib(cstr_t szMedia) {
    auto szExt = fileGetExt(szMedia);
    if (isEmptyString(szExt) || !isExtAudioFile(szExt)) {
        return;
    }

    // do not add repeated item.
    auto media = m_mediaLib->getMediaByUrl(szMedia);
    if (media) {
        return;
    }

    m_mediaLib->add(szMedia);
}

string CPlayer::formatMediaTitle(Media *media) {
    string strFullTitle = ::formatMediaTitle(media->artist.c_str(), media->title.c_str());
    if (strFullTitle.empty()) {
        strFullTitle = fileGetTitle(media->url.c_str());
    }

    return strFullTitle;
}

void CPlayer::filterLowRatingMedia(Playlist *playlist) {
    for (int i = playlist->getCount() - 1; i >= 0; i--) {
        auto media = playlist->getItem(i);
        if (media) {
            if (media->rating < m_ratingFilterMin) {
                playlist->removeItem(i);
            }
        }
    }
}

string CPlayer::getMediaKey() {
    return g_LyricSearch.getAssociateFileKeyword(m_szSrcMedia, m_szFullTitle);
}

int CPlayer::getMediaID() {
    if (m_currentMedia) {
        return m_currentMedia->ID;
    } else {
        return MEDIA_ID_INVALID;
    }
}

void CPlayer::getCurMediaAttribute(MediaAttribute mediaAttr, string &strValue) {
    strValue.clear();

    if (mediaAttr == MA_ARTIST) {
        strValue = m_szArtist;
    } else if (mediaAttr == MA_ALBUM) {
        strValue = m_szAlbum;
    } else if (mediaAttr == MA_TITLE) {
        strValue = m_szTitle;
    } else if (mediaAttr == MA_DURATION) {
        if (m_mediaLength > 0) {
            strValue = formatDuration(m_mediaLength / 1000);
        }
    } else {
        m_currentMedia->getAttribute(mediaAttr, strValue);
    }
}

void CPlayer::saveNowPlaying() {
    if (!m_isNowPlayingModified) {
        return;
    }

    m_mediaLib->saveNowPlaying();
    setNowPlayingModified(false);
}

void CPlayer::onUISeekbarSeek(int nPos) {
    g_currentLyrics.SetPlayElapsedTime(nPos);

    MPlayerApp::getEventsDispatcher()->dispatchSyncEvent(ET_LYRICS_DRAW_UPDATE);
}

string fixFileUri(cstr_t szUri) {
    static cstr_t SZ_FILE_PREFIX = "file://";
    static cstr_t SZ_LOCALHOST_MAC = "localhost";
    size_t nLenPrefix = strlen(SZ_FILE_PREFIX);
    if (szUri && strncasecmp(szUri, SZ_FILE_PREFIX, nLenPrefix) == 0) {
        szUri += nLenPrefix;

        if (startsWith(szUri, SZ_LOCALHOST_MAC)) {
            // For mac: file://localhost/Volumes/
            szUri += strlen(SZ_LOCALHOST_MAC);
        } else if (szUri[0] == '/' && szUri[2] == ':') {
            // Songbird use file:///c:/xx...xxx.mp3 , remove the extra '/'
        }
        szUri++;

        string uri = uriUnquote(szUri);
        strrep(uri, '/', PATH_SEP_CHAR);
        return uri;
    } else {
        return szUri;
    }
}

void CPlayer::onMediaChanged() {
    emptyStr(m_szSrcMedia);
    emptyStr(m_szFullTitle);
    emptyStr(m_szArtist);
    emptyStr(m_szTitle);
    emptyStr(m_szAlbum);
    m_mediaLength = 0;

    auto media = m_currentMedia;
    if (media) {
        strcpy_safe(m_szSrcMedia, CountOf(m_szSrcMedia), media->url.c_str());
        strcpy_safe(m_szArtist, CountOf(m_szArtist), media->artist.c_str());
        strcpy_safe(m_szTitle, CountOf(m_szTitle), media->title.c_str());
        strcpy_safe(m_szAlbum, CountOf(m_szAlbum), media->album.c_str());

        m_mediaLength = media->duration;

        string strFullTitle;
        formatFullTitle(m_szArtist, m_szTitle, m_szSrcMedia, strFullTitle);
        strcpy_safe(m_szFullTitle, CountOf(m_szFullTitle), strFullTitle.c_str());
    }
}

string CPlayer::getTitleFilterFile() {
    return dirStringJoin(getAppDataDir().c_str(), "TitlFilters.txt");
}

MediaPtr CPlayer::newMedia(cstr_t szUrl) {
    auto media = m_mediaLib->addFast(szUrl);
    if (!media) {
        media = make_shared<Media>();
        media->url = szUrl;
    }

    return media;
}

PlaylistPtr CPlayer::newPlaylist() {
    return make_shared<Playlist>();
}

ResultCode CPlayer::setCurrentMediaInNowPlaying(int index) {
    RMutexAutolock autolock(m_mutexDataAccess);

    if (index >= 0 && index < (int)m_currentPlaylist->getCount()) {
        m_idxCurrentMedia = index;
        autolock.unlock();
        currentMediaChanged();
        return ERR_OK;
    } else {
        return ERR_NOT_FOUND;
    }
}

void CPlayer::setNowPlaying(const PlaylistPtr &playlist) {
    m_isNowPlayingModified = true;
    m_currentPlaylist->clear();
    m_currentPlaylist->clone(playlist.get());
    m_idxCurrentMedia = 0;

    generateShuffleMediaQueue();

    notifyCurrentPlaylistChanged(IMPEvent::PCA_FULL_UPDATE, 0, 0);

    currentMediaChanged();
}

void CPlayer::updateMediaInfo(Media *media) {
    assert(media);
    Media newInfo = *media;

    loadMediaTagInfo(&newInfo);
    // 元数据推断：合并文件名/路径/标签，只改内存与库，不改音频文件。
    // 已确认(VERIFIED)的歌曲不重跑。
    if (newInfo.metaStatus != MetadataStatus::VERIFIED && !newInfo.url.empty()) {
        auto infer = g_metadataInference.inferForMedia(&newInfo);
        g_metadataInference.applyToMedia(infer, &newInfo);
    }
    if (!newInfo.isEqual(media)) {
        // update media info to media library, if changed.
        *media = newInfo;
        m_mediaLib->updateMediaInfo(media);

        // Send notifications
        if (m_currentMedia && m_currentMedia->ID == media->ID) {
            // Current media info changed.
            auto event = new IEvent();
            event->eventType = ET_PLAYER_CUR_MEDIA_INFO_CHANGED;
            MPlayerApp::getInstance()->getEventsDispatcher()->dispatchUnsyncEvent(event);
        } else {
            // Changed other media file.
            auto event = new IEvent();
            event->eventType = ET_PLAYER_MEDIA_INFO_CHANGED;
            event->strValue = std::to_string(media->ID);
            MPlayerApp::getInstance()->getEventsDispatcher()->dispatchUnsyncEvent(event);
        }
    }
}

ResultCode CPlayer::loadMediaTagInfo(Media *media) {
    bool ret = m_playerCore->getMediaInfo(media->url.c_str(), media);
    if (!ret || (media->artist.empty() && media->title.empty())) {
        BasicMediaTags tags;
        ExtendedMediaInfo infoExt;
        int ret = MediaTags::getTags(media->url.c_str(), tags, infoExt);
        if (ret == ERR_OK) {
            media->artist = tags.artist.c_str();
            media->title = tags.title.c_str();
            media->album = tags.album.c_str();
            media->year = atoi(tags.year.c_str());
            media->genre = tags.genre.c_str();
            media->trackNumb = atoi(tags.trackNo.c_str());

            media->duration = infoExt.mediaLength;
            media->bitsPerSample = infoExt.bitsPerSample;
            media->bitRate = infoExt.bitRate;
            media->channels = infoExt.channels;
            media->sampleRate = infoExt.sampleRate;
        }
    }

    FileStatInfo info;
    if (getFileStatInfo(media->url.c_str(), info)) {
        media->timeAdded = info.createdTime;
        media->fileSize = info.fileSize;
    }
    media->format = getMediaFormat(media->url.c_str());

    // get artist, title from music file name.
    if (media->artist.empty() && media->title.empty()) {
        getArtistTitleFromFileName(media->artist, media->title, media->url.c_str());
    }

    return ERR_OK;
}

void CPlayer::registerVisualizer(IEventHandler *eventHandler) {
    // 旧 VisParam 路径已废弃；请用 subscribeAudioAnalysis。
    (void)eventHandler;
}

void CPlayer::unregisterVisualizer(IEventHandler *eventHandler) {
    (void)eventHandler;
}

bool CPlayer::supportsAudioAnalysis() const {
    return m_playerCore && m_playerCore->supportsAudioAnalysis();
}

void CPlayer::setAudioAnalysisOptions(const AudioAnalysisOptions &options) {
    m_analysisOptions = options;
    if (m_playerCore) {
        m_playerCore->setAudioAnalysisOptions(options);
    }
}

void CPlayer::syncAudioAnalysisSinkToCore() {
    if (!m_playerCore) {
        return;
    }
    if (m_analysisSinks.empty()) {
        m_playerCore->setAudioAnalysisSink(nullptr);
    } else {
        m_playerCore->setAudioAnalysisOptions(m_analysisOptions);
        m_playerCore->setAudioAnalysisSink(this);
    }
}

void CPlayer::subscribeAudioAnalysis(IAudioAnalysisSink *sink) {
    if (!sink) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_analysisSinksMutex);
    if (std::find(m_analysisSinks.begin(), m_analysisSinks.end(), sink) == m_analysisSinks.end()) {
        m_analysisSinks.push_back(sink);
    }
    syncAudioAnalysisSinkToCore();
}

void CPlayer::unsubscribeAudioAnalysis(IAudioAnalysisSink *sink) {
    if (!sink) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_analysisSinksMutex);
    m_analysisSinks.erase(
        std::remove(m_analysisSinks.begin(), m_analysisSinks.end(), sink),
        m_analysisSinks.end());
    syncAudioAnalysisSinkToCore();
}

void CPlayer::onAudioAnalysisFrame(const AudioAnalysisFrame &frame) {
    std::vector<IAudioAnalysisSink *> sinks;
    {
        std::lock_guard<std::mutex> lock(m_analysisSinksMutex);
        sinks = m_analysisSinks;
    }
    for (auto *s : sinks) {
        if (s) {
            s->onAudioAnalysisFrame(frame);
        }
    }
}

void CPlayer::notifyPlaylistChanged(Playlist *playlist, IMPEvent::PlaylistChangeAction action, int nIndex, int nIndexOld) {
    if (m_currentPlaylist.get() == playlist) {
        // first notify the message
        notifyCurrentPlaylistChanged(action, nIndex, nIndexOld);

        //
        // is current media changed?
        //
        RMutexAutolock autolock(m_mutexDataAccess);

        if (m_idxCurrentMedia >= (int)m_currentPlaylist->getCount()) {
            m_idxCurrentMedia = 0;
        }

        generateShuffleMediaQueue();

        // is current media?
        auto media = m_currentPlaylist->getItem(m_idxCurrentMedia);
        if (media) {
            if (m_currentMedia == media) {
                return;
            }
        }

        // 插入/删除只是挪了下标时，当前曲没变，不要 stop/reload（否则会冲掉紧接着的 playMedia）。
        int nNewCurrentIndex;
        if (m_currentPlaylist->getItemIndex(m_currentMedia, nNewCurrentIndex) == ERR_OK) {
            m_idxCurrentMedia = nNewCurrentIndex;
            return;
        }

        autolock.unlock();
        currentMediaChanged();
    }
}

void CPlayer::notifyPlayStateChanged() {
    CEventPlayerStatusChanged *pEvent = new CEventPlayerStatusChanged();
    pEvent->eventType = ET_PLAYER_STATUS_CHANGED;
    pEvent->status = m_state;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);

    g_playerEventDispatcher.stopLyrDrawUpdate();

    if (m_state == PS_PLAYING) {
        g_playerEventDispatcher.startLyrDrawUpdate();
    }
}

void CPlayer::notifyCurrentPlaylistChanged(IMPEvent::PlaylistChangeAction action, int nIndex, int nIndexOld) {
    CEventPlaylistChanged *pEvent = new CEventPlaylistChanged();
    pEvent->eventType = ET_PLAYER_CUR_PLAYLIST_CHANGED;
    pEvent->action = action;
    pEvent->nIndex = nIndex;
    pEvent->nIndexOld = nIndexOld;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);
}

void CPlayer::notifySettingsChanged(IMPEvent::SettingType settingType, int value) {
    CEventPlayerSettingChanged *pEvent = new CEventPlayerSettingChanged();
    pEvent->eventType = ET_PLAYER_SETTING_CHANGED;
    pEvent->settingType = settingType;
    pEvent->value = value;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);
}

void CPlayer::notifyEQSettingsChanged(const EQualizer *eq) {
    CEventPlayerEQChanged *pEvent = new CEventPlayerEQChanged();
    pEvent->eventType = ET_PLAYER_EQ_SETTING_CHANGED;
    pEvent->eqlalizer = *eq;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);
}

void CPlayer::notifySeek() {
    IEvent *pEvent = new IEvent();
    pEvent->eventType = ET_PLAYER_SEEK;

    // CoreAVPlayer 在 Seek 之后，立即调用 getPos 的值偶尔正确，延迟100ms 通知 Seek 更新.
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEventDelayed(pEvent, 100);
}

// For internal using, do NOT lock.
void CPlayer::generateShuffleMediaQueue() {
    VecInts vMediaSequence;
    auto nCount = m_currentPlaylist->getCount();
    m_idxCurrentShuffleMedia = 1;

    for (int i = 0; i < nCount; i++) {
        vMediaSequence.push_back(i);
    }

    m_vShuffleMedia.clear();
    for (int i = 0; i < nCount; i++) {
        int nNext = rand() % vMediaSequence.size();
        m_vShuffleMedia.push_back(vMediaSequence[nNext]);
        vMediaSequence.erase(vMediaSequence.begin() + nNext);
    }
}

void CPlayer::setCurrentMedia(MediaPtr &media) {
    {
        RMutexAutolock autolock(m_mutexDataAccess);

        int oldId = m_currentMedia ? m_currentMedia->ID : MEDIA_ID_INVALID;
        int newId = media ? media->ID : MEDIA_ID_INVALID;
        if (oldId != newId) {
            m_playHistoryRecorded = false;
        }

        m_currentMedia = media;
        if (m_currentMedia) {
            Media newInfo = *m_currentMedia.get();

            loadMediaTagInfo(&newInfo);
            if (!newInfo.isEqual(m_currentMedia.get())) {
                // update media info to media library, if changed.
                *(m_currentMedia.get()) = newInfo;
                m_mediaLib->updateMediaInfo(m_currentMedia.get());
            }
        }
    }

    g_player.onMediaChanged();

    IEvent *pEvent = new IEvent();
    pEvent->eventType = ET_PLAYER_CUR_MEDIA_CHANGED;
    MPlayerApp::getEventsDispatcher()->dispatchUnsyncEvent(pEvent);
}

void CPlayer::currentMediaChanged() {
    bool bPlay = false;

    if (m_state != PS_STOPPED) {
        if (m_state == PS_PLAYING) {
            bPlay = true;
        }
        m_isAutoPlayNext = false;
        stop();
    }

    MediaPtr media;

    {
        RMutexAutolock autolock(m_mutexDataAccess);

        if (m_currentMedia) {
            if (m_isAutoPlayNext && m_isCurMediaPlayed) {
                recordPlayHistory(false);
            }

            m_currentMedia = nullptr;
            m_isCurMediaPlayed = false;
        }

        if (m_currentPlaylist->getCount() > 0) {
            media = m_currentPlaylist->getItem(m_idxCurrentMedia);
        }
    }

    setCurrentMedia(media);

    if (bPlay) {
        play();
    }
}

ResultCode CPlayer::doNext(bool bLoop) {
    RMutexAutolock autolock(m_mutexDataAccess);

    if (m_currentPlaylist->getCount() <= 1) {
        return ERR_EMPTY_PLAYLIST;
    }

    int nOld = m_idxCurrentMedia;

    if (m_isShuffle) {
        if (m_idxCurrentShuffleMedia >= (int)m_vShuffleMedia.size() - 1) {
            if (m_loopMode != MP_LOOP_ALL && !bLoop) {
                return ERR_END_OF_PLAYLIST;
            }

            // all media has been played, regenerate radom playlist.
            generateShuffleMediaQueue();
        }

        assert(m_vShuffleMedia.size() == m_currentPlaylist->getCount());
        m_idxCurrentMedia = m_vShuffleMedia[m_idxCurrentShuffleMedia];
        m_idxCurrentShuffleMedia++;
        // m_idxCurrentMedia = rand() % m_currentPlaylist->getCount();
    } else {
        if (m_idxCurrentMedia < (int)m_currentPlaylist->getCount() - 1) {
            m_idxCurrentMedia++;
        } else if (m_loopMode != MP_LOOP_ALL && !bLoop) {
            return ERR_END_OF_PLAYLIST;
        } else {
            m_idxCurrentMedia = 0;
        }
    }

    autolock.unlock();

    if (nOld != m_idxCurrentMedia) {
        // Notify changed
        currentMediaChanged();
    }
    return ERR_OK;
}

void CPlayer::playNextOnFailed() {
    if (!m_currentPlaylist || m_countPlayFailed >= m_currentPlaylist->getCount()) {
        // Stop playing next if all files were tried.
        return;
    }

    next();
    if (m_currentMedia) {
        play();
    }
}


#if UNIT_TEST

#include "../TinyJS/utils/unittest.h"

TEST(HelperFun, testGetArtistTitleFromFileName) {
    string artist, title;
    cstr_t szFile;

    szFile = "c:\\abc\ar - ti-a.mp3";
    getArtistTitleFromFileName(artist, title, szFile);
    ASSERT_TRUE(artist == "ar");
    ASSERT_TRUE(title == "ti-a");

    szFile = "c:\\abc\ar-ti-a.mp3";
    getArtistTitleFromFileName(artist, title, szFile);
    ASSERT_TRUE(artist == "ar");
    ASSERT_TRUE(title == "ti-a");

    szFile = "c:\\abc\ti.mp3";
    getArtistTitleFromFileName(artist, title, szFile);
    ASSERT_TRUE(artist == "");
    ASSERT_TRUE(title == "ti");
}

#endif
