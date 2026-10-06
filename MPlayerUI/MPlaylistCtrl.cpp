#include "MPlayerApp.h"
#include "MPlaylistCtrl.h"
#include "PlayListFile.h"
#include "DlgPlaylist.hpp"


void splitKeywords(cstr_t keyword, VecStrings &vKeywords) {
    vKeywords.clear();

    auto start = keyword, p = keyword;
    while (*p) {
        while (isAlpha(*p) || isDigit(*p) || (unsigned int)(*p) >= 128) {
            p++;
        }

        if (p == start) {
            p++;
        }

        if (*start != ' ' && *start != '\t') {
            vKeywords.push_back(string(start, p));
        }

        start = p;
    }
}

static uint32_t searchWithKeywordEx(const VecStrings &vKeywords, cstr_t text, CSkinListCtrl::VecTextColor &vItemClrs) {
    VecStrings keywordRemains = vKeywords;
    auto lastUnmatch = text, start = text, p = text;

    while (*p) {
        while (isAlpha(*p) || isDigit(*p)) {
            p++;
        }

        if (p == start) {
            p++;
        }

        if (*start != ' ' && *start != '\t') {
            for (VecStrings::size_type i = 0; i < keywordRemains.size(); i++) {
                if (int(p - start) == keywordRemains[i].size()
                    && strncasecmp(start, keywordRemains[i].c_str(), keywordRemains[i].size()) == 0) {
                    keywordRemains.erase(keywordRemains.begin() + i);

                    // set unmatched color
                    if (lastUnmatch != start) {
                        vItemClrs.add(int(start - lastUnmatch), CSkinListCtrl::CN_TEXT);
                    }

                    // set matched color
                    vItemClrs.add(int(p - start), CSkinListCtrl::CN_CUSTOMIZED_START);

                    lastUnmatch = p;
                    break;
                }
            }
            if (keywordRemains.empty()) {
                return 1;
            }
        }

        start = p;
    }

    if (keywordRemains.empty()) {
        return 1;
    }

    return 0;
}


static uint32_t searchWithKeyword(cstr_t keyword, cstr_t text, CSkinListCtrl::VecTextColor &vItemClrs) {
    vItemClrs.clear();

    auto dst = text;
    while (*dst) {
        auto dstStart = dst;
        auto key = keyword;

        while (*dst && toLower(*key) == toLower(*dst)) {
            key++;
            dst++;
        }

        if (*key == 0) {
            if (int(dstStart - text) > 0) {
                vItemClrs.add(int(dstStart - text), CSkinListCtrl::CN_TEXT);
            }
            vItemClrs.add(int(dst - dstStart), CSkinListCtrl::CN_CUSTOMIZED_START);
            return uint32_t(-1);
        }

        if (*dst) {
            dst++;
        }
    }

    return 0;
}

bool deleteSelectedItemsInPlaylist(Playlist *playlist, vector<int> &vSelIndex) {
    ResultCode nRet = ERR_OK;
    long n;
    long count = 0;

    assert(vSelIndex.size() > 0);
    if (vSelIndex.size() == 0) {
        return false;
    }

    sort(vSelIndex.begin(), vSelIndex.end());

    count = playlist->getCount();
    if (count == 0) {
        return false;
    }

    for (n = 0; n < (long)vSelIndex.size(); n++) {
        assert(vSelIndex[n] < count && vSelIndex[n] >= 0);
        playlist->removeItem(vSelIndex[n]);
    }

    return nRet == ERR_OK;
}

bool offsetAllSelectedRowInPlaylist(Playlist *playlist, vector<int> &vSelIndex, bool bDown) {
    assert(vSelIndex.size() > 0);
    if (vSelIndex.size() == 0) {
        return false;
    }

    sort(vSelIndex.begin(), vSelIndex.end());

    auto count = playlist->getCount();
    if (count == 0) {
        return false;
    }

    if (bDown) {
        for (int n = (int)vSelIndex.size() - 1; n >= 0; n--) {
            assert(vSelIndex[n] < count && vSelIndex[n] >= 0);
            playlist->moveItem(vSelIndex[n], vSelIndex[n] + 1);
        }
    } else {
        for (int n = 0; n < (int)vSelIndex.size(); n++) {
            assert(vSelIndex[n] < count && vSelIndex[n] >= 0);
            playlist->moveItem(vSelIndex[n], vSelIndex[n] - 1);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////

UIOBJECT_CLASS_NAME_IMP(CMPlaylistCtrl, "Playlist")

CMPlaylistCtrl::CMPlaylistCtrl() {
    m_bHorzScrollBar = false;
    m_editorSearch = nullptr;
}

CMPlaylistCtrl::~CMPlaylistCtrl() {
}

bool CMPlaylistCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (isPropertyName(szProperty, "KeywordEdit")) {
        m_idEditorSearch = szValue;
    } else if (strcasecmp(szProperty, "SearchResultMenu") == 0) {
        m_menuSearchResultName = szValue;
    } else if (strcasecmp(szProperty, "ContextMenu") == 0) {
        m_menuCtxName = szValue;
    } else {
        return CSkinListCtrl::setProperty(szProperty, szValue);
    }

    return true;
}

void CMPlaylistCtrl::onCreate() {
    CSkinListCtrl::onCreate();

    loadMenu();

    m_editorSearch = (CSkinEditCtrl *)m_pSkin->getUIObjectById(m_idEditorSearch, CSkinEditCtrl::className());
    if (m_editorSearch) {
        m_editorSearch->setEditNotification(this);
    }

    assert(getColumnCount() == 0);
    addColumn(_TLT("Index"), 40, CColHeader::TYPE_TEXT, false, DT_CENTER);
    addColumn(_TLT("Media Files"), 130, CColHeader::TYPE_TEXT_EX, false, DT_CENTER);
    addColumn(_TLT("Duration"), 70, CColHeader::TYPE_TEXT, false, DT_CENTER);

    registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_PLAYLIST_CHANGED, ET_PLAYER_CUR_MEDIA_CHANGED, ET_PLAYER_MEDIA_INFO_CHANGED);

    updatePlaylist(false);
}

void CMPlaylistCtrl::onTimer(int nId) {
    if (!m_editorSearch) {
        return;
    }

    if (m_timerIdBeginSearch) {
        m_pSkin->unregisterTimerObject(this, m_timerIdBeginSearch);
        m_timerIdBeginSearch = 0;
    }

    string keyword = m_editorSearch->getText();

    trimStr(keyword);
    keyword = toLower(keyword.c_str());
    doSearch(keyword.c_str());
}

void CMPlaylistCtrl::onEvent(const IEvent *pEvent) {
    if (m_searchResults) {
        return;
    }

    if (pEvent->eventType == ET_PLAYER_CUR_PLAYLIST_CHANGED) {
        CEventPlaylistChanged *pEventPlaylistChanged = (CEventPlaylistChanged*)pEvent;
        onCurrentPlaylistEvent(pEventPlaylistChanged);
    } else if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED) {
        if (g_player.isCurrentMediaTempPlaying()) {
            // 临时播放的歌曲不会改变播放列表.
            return;
        }

        setNowPlaying();

        if (m_nowPlayingRow != -1) {
            makeSureRowVisible(m_nowPlayingRow);
        }

        int row = g_player.getCurrentMediaIndex();
        auto media = g_player.getCurrentMedia();
        if (media && row < getRowCount()) {
            string strTitle = g_player.formatMediaTitle(media.get());
            setItemText(row, 1, strTitle.c_str());

            setItemMediaDuration(row, media->duration); // ms
        }

        invalidate();
    } else if (pEvent->eventType == ET_PLAYER_MEDIA_INFO_CHANGED) {
        auto id = atoi(pEvent->strValue.c_str());
        auto playlist = g_player.getNowPlaying();
        int row = 0;
        auto media = playlist->getItemByID(id, &row);
        if (media && row < getRowCount()) {
            string strTitle = g_player.formatMediaTitle(media.get());
            setItemText(row, 1, strTitle.c_str());
            setItemMediaDuration(row, media->duration); // ms

            invalidate();
        }
    }
}

bool CMPlaylistCtrl::onCommand(uint32_t nId) {
    if (nId >= ID_ADD_RESULTS_TO_PLAYLIST_START && nId <= ID_ADD_RESULTS_TO_PLAYLIST_END) {
        int index = nId - ID_ADD_RESULTS_TO_PLAYLIST_START;
        if (index >= 0 && index < m_playlistNames.size()) {
            auto id = m_playlistNames[index].id;
            auto mediaLib = g_player.getMediaLibrary();
            MediaPtr firstSelected;
            mediaLib->addToPlaylist(id, getAllSearchResult(firstSelected));
        }
    } else if (nId >= ID_ADD_SELECTED_TO_PLAYLIST_START && nId <= ID_ADD_SELECTED_TO_PLAYLIST_END) {
        int index = nId - ID_ADD_SELECTED_TO_PLAYLIST_START;
        if (index >= 0 && index < m_playlistNames.size()) {
            auto id = m_playlistNames[index].id;
            auto mediaLib = g_player.getMediaLibrary();
            mediaLib->addToPlaylist(id, getSelected());
        }
    }

    switch (nId) {
        case ID_SHOW_IN_FINDER: {
            auto playlist = getSelected();
            auto medias = playlist->getAll();
            VecStrings files;

            for (auto &media : medias) {
                files.push_back(media->url);
            }

            showInFinder(files);
            return true;
        }
        case ID_PLAY_SELECTED_FILE: {
            auto playlist = getSelected();
            auto media = playlist->getItem(0);
            if (media) {
                g_player.playMedia(media);
            }
            return true;
        }
        case ID_REPLACE_NOW_PLAYING_WITH_RESULTS: {
            MediaPtr firstSelected;
            auto playlist = getAllSearchResult(firstSelected);
            g_player.setNowPlaying(playlist);
            g_player.setNowPlayingModified(true);
            g_player.playMedia(firstSelected);
            g_player.saveNowPlaying();
            return true;
        }
        case ID_REPLACE_NOW_PLAYING_WITH_SELECTED: {
            MediaPtr firstSelected;
            auto playlist = getSelectedSearchResult(firstSelected);
            g_player.setNowPlaying(playlist);
            g_player.setNowPlayingModified(true);
            g_player.playMedia(firstSelected);
            g_player.saveNowPlaying();
            return true;
        }
        case ID_ADD_RESULTS_TO_NOW_PLAYING: {
            MediaPtr firstSelected;
            auto playlist = getAllSearchResult(firstSelected);
            auto curPl = g_player.getNowPlaying();
            curPl->insert(-1, playlist.get());
            g_player.setNowPlayingModified(true);
            g_player.saveNowPlaying();
            return true;
        }
        case ID_ADD_SELECTED_TO_NOW_PLAYING: {
            MediaPtr firstSelected;
            auto playlist = getSelectedSearchResult(firstSelected);
            auto curPl = g_player.getNowPlaying();
            curPl->insert(-1, playlist.get());
            g_player.setNowPlayingModified(true);
            g_player.saveNowPlaying();
            return true;
        }
        case ID_ADD_RESULTS_TO_PLAYLIST_NEW: {
            MediaPtr firstSelected;
            auto playlist = getAllSearchResult(firstSelected);
            showNewPlaylistDialog(m_pSkin, playlist);
            return true;
        }
        case ID_ADD_SELECTED_TO_PLAYLIST_NEW: {
            auto playlist = getSelected();
            showNewPlaylistDialog(m_pSkin, playlist);
            return true;
        }
        case ID_PL_DEL: {
            auto playlist = getSelected();
            auto nowplaying = g_player.getNowPlaying();
            nowplaying->removeItems(playlist->getAll());
            return true;
        }
        case ID_REMOVE_FROM_LIBRARY: {
            auto playlist = getSelected();
            auto medias = playlist->getAll();
            auto mediaLib = g_player.getMediaLibrary();
            g_player.getNowPlaying()->removeItems(medias);
            mediaLib->remove(medias, false);
            return true;
        }
        default: return false;
    }
}

bool CMPlaylistCtrl::onKeyDown(uint32_t code, uint32_t flags) {
    if (CSkinListCtrl::onKeyDown(code, flags)) {
        return true;
    }

    showHideEditorSearch(0);
    m_editorSearch->setFocus();

    return false;
}

bool CMPlaylistCtrl::onHandleKeyDown(uint32_t code, uint32_t flags) {
    if (code == VK_RETURN) {
        auto pl = getSelected();
        if (pl->getCount() <= 1) {
            // 只选择了一首歌曲，则直接播放
            auto media = pl->getItem(0);
            if (media) {
                g_player.playMedia(media);
            }
            return true;
        }

        // 弹出菜单，用户选择希望的操作
        CPoint pt(m_rcContent.right, m_rcContent.top);

        m_pSkin->clientToScreen(pt);
        popupSearchResultMenu(pt);
    } else if (code == VK_ESCAPE && m_searchResults) {
        if (m_editorSearch) {
            m_editorSearch->setText("");
        }

        m_searchResults = nullptr;
        m_categoriesResults.clear();
        updatePlaylist(false);
        if (m_nowPlayingRow != -1) {
            makeSureRowVisible(m_nowPlayingRow);
        }
        invalidate();
    } else if (code == VK_DELETE) {
        deleteSelectedItems();
    } else if ((code == VK_UP || code == VK_DOWN) &&
               isModifierKeyPressed(MK_CONTROL, flags)) {
        offsetAllSelectedItems(code == VK_DOWN);
    } else {
        return CSkinListCtrl::onHandleKeyDown(code, flags);
    }

    return true;
}

bool CMPlaylistCtrl::onRButtonUp(uint32_t nFlags, CPoint point) {
    if (m_searchResults) {
        if (m_menuSearchResult) {
            popupSearchResultMenu(getCursorPos());
            return true;
        }
    } else if (m_menuCtx) {
        popupContexMenu(getCursorPos());
        return true;
    }

    return CSkinListCtrl::onRButtonUp(nFlags, point);
}

void CMPlaylistCtrl::onLanguageChanged() {
    loadMenu();

    CSkinListCtrl::onLanguageChanged();
}

void CMPlaylistCtrl::onVScroll(uint32_t nSBCode, int nPos, IScrollBar *pScrollBar) {
    showHideEditorSearch(nPos);

    CSkinListCtrl::onVScroll(nSBCode, nPos, pScrollBar);
}

// 最终输入的文字
void CMPlaylistCtrl::onInputText(cstr_t text) {
    showHideEditorSearch(0);
    m_editorSearch->onInputText(text);
    m_editorSearch->setFocus();
}

// MarketText 是临时的文字，当输入其他字符时会被替代
void CMPlaylistCtrl::onInputMarkedText(cstr_t text) {
    showHideEditorSearch(0);
    m_editorSearch->onInputMarkedText(text);
    m_editorSearch->setFocus();
}

void CMPlaylistCtrl::onChar(uint32_t nChar) {
    showHideEditorSearch(0);
    m_editorSearch->onChar(nChar);
    m_editorSearch->setFocus();
}

void CMPlaylistCtrl::sendNotifyEvent(CSkinListCtrlEventNotify::Command cmd, int nClickedRow, int nClickedCol) {
    if (cmd == CSkinListCtrlEventNotify::C_DBL_CLICK && nClickedRow != -1) {
        auto pl = getSelected();
        if (pl->getCount() <= 1) {
            auto media = pl->getItem(0);
            if (media) {
                g_player.playMedia(media);
            }
            return;
        }

        onCommand(ID_REPLACE_NOW_PLAYING_WITH_SELECTED);
    }

    CSkinListCtrl::sendNotifyEvent(cmd, nClickedRow, nClickedCol);
}

void CMPlaylistCtrl::makeSureRowVisible(int nRow) {
    CSkinListCtrl::makeSureRowVisible(nRow);

    showHideEditorSearch(m_nFirstVisibleRow);
}

void CMPlaylistCtrl::showHideEditorSearch(int row) {
    if (m_header && !m_header->isVisible() && row == 0) {
        m_header->setVisible(true, false);
    } else if (m_header && m_header->isVisible() && row > 0 && !m_editorSearch->isOnFocus()) {
        m_header->setVisible(false, false);
    }
}

void CMPlaylistCtrl::deleteSelectedItems() {
    if (m_searchResults) {
        return;
    }

    int nItem = -1;
    vector<int> vSelItems;

    while ((nItem = getNextSelectedItem(nItem)) != -1) {
        vSelItems.push_back(nItem);
    }

    if (vSelItems.empty()) {
        return;
    }

    auto playlist = g_player.getNowPlaying();
    if (deleteSelectedItemsInPlaylist(playlist.get(), vSelItems)) {
        nItem = vSelItems[0];
        if (nItem >= getItemCount()) {
            nItem = getItemCount() - 1;
        }
        if (nItem >= 0) {
            setItemSelectionState(nItem, true);
        }
        g_player.setNowPlayingModified(true);
        g_player.saveNowPlaying();
    }
}

void CMPlaylistCtrl::offsetAllSelectedItems(bool bMoveDown) {
    if (m_searchResults) {
        return;
    }

    vector<int> vSelItems;
    int nItem = -1;

    while ((nItem = getNextSelectedItem(nItem)) != -1) {
        vSelItems.push_back(nItem);
    }

    auto playlist = g_player.getNowPlaying();
    if (offsetAllSelectedRowInPlaylist(playlist.get(), vSelItems, bMoveDown)) {
        // UpdateButtonState();
        g_player.setNowPlayingModified(true);
        g_player.saveNowPlaying();
    }
}

void CMPlaylistCtrl::onEditorTextChanged() {
    if (!m_editorSearch) {
        return;
    }

    if (m_timerIdBeginSearch != 0) {
        m_pSkin->unregisterTimerObject(this, m_timerIdBeginSearch);
    }

    // 输入文字之后启动定时器，更新搜索结果
    m_timerIdBeginSearch = m_pSkin->registerTimerObject(this, 1000);
}

bool CMPlaylistCtrl::onEditorKeyDown(uint32_t code, uint32_t flags) {
    if (!m_editorSearch) {
        return false;
    }

    return onHandleKeyDown(code, flags);
}

void CMPlaylistCtrl::onEditorMouseWheel(int wheelDistance, int mkeys, CPoint pt) {
    onMouseWheel(wheelDistance, mkeys, pt);
}

void CMPlaylistCtrl::onEditorKillFocus() {
    if (m_header) {
        if (m_pVertScrollBar && m_pVertScrollBar->getScrollPos() > 0) {
            m_header->setVisible(false, false);
            invalidate();
        }
    }
}

void CMPlaylistCtrl::insertMedia(const PlaylistPtr &playlist, int index, bool redraw) {
    auto media = playlist->getItem(index);
    if (media) {
        insertItem(index, itos(index + 1).c_str(), 0, 0, false);

        string strTitle = g_player.formatMediaTitle(media.get());
        setItemText(index, 1, strTitle.c_str(), false);

        setItemMediaDuration(index, media->duration); // ms
    }

    if (redraw) {
        invalidate();
    }
}

void CMPlaylistCtrl::appendMedia(const MediaPtr &media, const CSkinListCtrl::VecTextColor &vItemClrs) {
    auto index = getRowCount();

    insertItem(index, itos(index + 1).c_str(), 0, 0, false);

    string strTitle = g_player.formatMediaTitle(media.get());
    setItemTextEx(index, 1, strTitle.c_str(), vItemClrs);

    setItemMediaDuration(index, media->duration); // ms
}

void CMPlaylistCtrl::updateMediaIndex() {
    for (int i = 0; i < (int)m_vRows.size(); i++) {
        setItemText(i, 0, itos(i).c_str(), false);
    }

    invalidate();
}

void CMPlaylistCtrl::onCurrentPlaylistEvent(CEventPlaylistChanged *pEventPlaylistChanged) {
    if (pEventPlaylistChanged == nullptr || pEventPlaylistChanged->action == IMPEvent::PCA_FULL_UPDATE) {
        // reload playlist
        updatePlaylist(true);
    } else if (pEventPlaylistChanged->action == IMPEvent::PCA_CLEAR) {
        deleteAllItems();
    } else if (pEventPlaylistChanged->action == IMPEvent::PCA_INSERT) {
        auto playlist = g_player.getNowPlaying();
        insertMedia(playlist, pEventPlaylistChanged->nIndex, false);
        updateMediaIndex();
    } else if (pEventPlaylistChanged->action == IMPEvent::PCA_MOVE) {
        int nIndex = pEventPlaylistChanged->nIndex;
        int nIndexOld = pEventPlaylistChanged->nIndexOld;
        if (nIndex >= 0 && nIndex < getItemCount()
            && nIndexOld >= 0 && nIndexOld < getItemCount()) {
            Row *temp = m_vRows[nIndex];
            m_vRows[nIndex] = m_vRows[nIndexOld];
            m_vRows[nIndexOld] = temp;
        }
        updateMediaIndex();
    } else if (pEventPlaylistChanged->action == IMPEvent::PCA_REMOVE) {
        int nSel = getNextSelectedItem();
        int nIndex = pEventPlaylistChanged->nIndex;
        if (nIndex >= 0 && nIndex < getItemCount()) {
            deleteItem(pEventPlaylistChanged->nIndex, false);
        }

        if (nSel != -1) {
            if (nSel >= getItemCount()) {
                nSel = getItemCount() - 1;
            }
            if (nSel != -1) {
                m_pSkin->enterInDrawUpdate();
                setItemSelectionState(nSel, true);
                m_pSkin->leaveInDrawUpdate();
            }
        }

        updateMediaIndex();
    } else {
        assert(0);
    }
}

void CMPlaylistCtrl::setItemMediaDuration(int index, int ms) {
    if (ms > 0) {
        char buf[64];

        auto seconds = (ms + 500) / 1000;
        int minutes = seconds / 60; seconds %= 60;
        if (minutes < 1000) {
            sprintf(buf, "%d:%02d", minutes, seconds % 60);
        } else {
            int hours = minutes / 60; minutes %= 60;
            int days = hours / 24; hours %= 24;
            if (days > 0) {
                sprintf(buf, "%dD%dH", days, hours);
            } else {
                sprintf(buf, "%dH%dM", hours, minutes);
            }
        }
        setItemText(index, 2, buf, false);
    }
}

void CMPlaylistCtrl::updatePlaylist(bool isRedraw) {
    PlaylistPtr playlist;

    if (m_searchResults) {
        playlist = m_searchResults;
    } else {
        playlist = g_player.getNowPlaying();
    }

    deleteAllItems(false);

    int count = playlist->getCount();
    for (int i = 0; i < count; i++) {
        insertMedia(playlist, i);
    }

    setNowPlaying();

    if (isRedraw) {
        invalidate();
    }
}

void CMPlaylistCtrl::setNowPlaying() {
    m_nowPlayingRow = -1;

    if (m_searchResults) {
        auto media = g_player.getCurrentMedia();
        if (media) {
            auto id = media->ID;
            uint32_t count = m_searchResults->getCount();
            for (uint32_t i = 0; i < count; i++) {
                auto m = m_searchResults->getItem(i);
                if (m) {
                    if (m->ID == id) {
                        m_nowPlayingRow = i;
                        break;
                    }
                }
            }
        }
    } else {
        m_nowPlayingRow = g_player.getCurrentMediaIndex();
    }
}

void CMPlaylistCtrl::doSearch(cstr_t keyword) {
    // clear old results
    if (m_searchResults) {
        m_searchResults = nullptr;
    }
    m_categoriesResults.clear();
    deleteAllItems(false);

    if (isEmptyString(keyword)) {
        updatePlaylist(true);
        return;
    }

    m_searchResults = g_player.newPlaylist();

    CSkinListCtrl::VecTextColor vItemClrs;
    VecStrings vKeywords;
    splitKeywords(keyword, vKeywords);

    VecMediaCategories allCategories;

    auto mediaLib = g_player.getMediaLibrary();
    mediaLib->getMediaCategories(allCategories);

    //
    // 搜索分类列表
    //
    for (auto &category : allCategories) {
        string title = g_LangTool.toLocalString(mediaCategoryTypeToString(category.type));
        title += ": " + (category.name.empty() ? _TL("Unknown") : category.name);

        auto matchValue = searchWithKeyword(keyword, title.c_str(), vItemClrs);
        if (matchValue <= 0) {
            matchValue = searchWithKeywordEx(vKeywords, title.c_str(), vItemClrs);
        }
        if (matchValue > 0) {
            auto index = getRowCount();

            m_categoriesResults.push_back(category);

            insertItem(index, itos(index + 1).c_str(), 0, 0, false);

            int count = vItemClrs.sumCount();
            if (count < title.size()) {
                vItemClrs.add((int)title.size() - count, CSkinListView::CN_TEXT);
            }
            string temp = " " + stringPrintf(_TL("(%d Tracks)"), category.mediaCount);
            vItemClrs.add(temp.size(), CSkinListView::CN_CUSTOMIZED_START + 1);
            title += temp;

            setItemTextEx(index, 1, title.c_str(), vItemClrs);

            setItemMediaDuration(index, category.mediaDuration * 1000);
        }
    }


    //
    // search playlist with @keyword
    //
    auto playlist = mediaLib->getAll();
    int count = playlist->getCount();
    for (uint32_t i = 0; i < count; i++) {
        auto media = playlist->getItem(i);
        if (media) {
            string title = CPlayer::formatMediaTitle(media.get());

            auto matchValue = searchWithKeyword(keyword, title.c_str(), vItemClrs);
            if (matchValue <= 0) {
                matchValue = searchWithKeywordEx(vKeywords, title.c_str(), vItemClrs);
            }
            if (matchValue > 0) {
                appendMedia(media, vItemClrs);
                m_searchResults->insertItem(-1, media);
            }
        }
    }

    invalidate();
}

PlaylistPtr CMPlaylistCtrl::getSelected() {
    if (m_searchResults) {
        MediaPtr firstSelected;
        return getSelectedSearchResult(firstSelected);
    }

    auto nowplaying = g_player.getNowPlaying();
    auto playlist = g_player.newPlaylist();
    int i = -1;
    while (true) {
        i = getNextSelectedItem(i);
        if (i >= 0 && i < (int)nowplaying->getCount()) {
            auto media = nowplaying->getItem(i);
            if (media) {
                playlist->insertItem(-1, media);
            }
        } else {
            break;
        }
    }

    return playlist;
}

PlaylistPtr CMPlaylistCtrl::getSelectedSearchResult(MediaPtr &firstSelectedOut) {
    PlaylistPtr playlist = g_player.newPlaylist();
    auto mediaLib = g_player.getMediaLibrary();

    int i = -1;
    while (true) {
        i = getNextSelectedItem(i);
        if (i >= 0) {
            if (i < m_categoriesResults.size()) {
                auto pl = mediaLib->getMediaCategory(m_categoriesResults[i]);
                if (pl) {
                    playlist->insert(-1, pl.get());
                }
            } else {
                int index = i - (int)m_categoriesResults.size();
                if (index < (int)m_searchResults->getCount()) {
                    auto media = m_searchResults->getItem(index);
                    if (media) {
                        playlist->insertItem(-1, media);
                    }
                }
            }
        } else {
            break;
        }
    }

    return playlist;
}

PlaylistPtr CMPlaylistCtrl::getAllSearchResult(MediaPtr &firstSelectedOut) {
    PlaylistPtr playlist = g_player.newPlaylist();
    auto mediaLib = g_player.getMediaLibrary();

    for (auto &item : m_categoriesResults) {
        auto pl = mediaLib->getMediaCategory(item);
        playlist->insert(-1, pl.get());
    }

    playlist->insert(-1, m_searchResults.get());

    return playlist;
}

void CMPlaylistCtrl::loadMenu() {
    if (!m_menuSearchResultName.empty()) {
        m_menuSearchResult = m_pSkin->getSkinFactory()->loadMenu(m_pSkin, m_menuSearchResultName.c_str());
        if (m_menuSearchResult) {
            m_submenuAddResultToPlaylist = m_menuSearchResult->getSubmenuByPlaceHolderID(ID_ADD_RESULTS_TO_PLAYLIST_NEW);
            m_submenuAddSelectedToPlaylist = m_menuSearchResult->getSubmenuByPlaceHolderID(ID_ADD_SELECTED_TO_PLAYLIST_NEW);
        }
    }

    if (!m_menuCtxName.empty()) {
        m_menuCtx = m_pSkin->getSkinFactory()->loadMenu(m_pSkin, m_menuCtxName.c_str());
        if (m_menuCtx) {
            m_ctxSubmenuAddToPlaylist = m_menuCtx->getSubmenuByPlaceHolderID(ID_ADD_SELECTED_TO_PLAYLIST_NEW);
        }
    }
}

void CMPlaylistCtrl::popupSearchResultMenu(CPoint pt) {
    int count = getSelectedCount();

    m_menuSearchResult->enableItem(ID_SHOW_IN_FINDER, count > 0);
    m_menuSearchResult->enableItem(ID_PLAY_SELECTED_FILE, count > 0);

    const int MAX_COUNT = 10;
    VecStrings names;

    m_playlistNames = g_player.getMediaLibrary()->getRecentPlaylistBriefs();
    if (m_playlistNames.size() > MAX_COUNT) {
        m_playlistNames.resize(MAX_COUNT);
    }

    for (auto &name : m_playlistNames) {
        names.push_back(name.name);
    }

    if (m_submenuAddResultToPlaylist.isValid()) {
        m_submenuAddResultToPlaylist.replaceAllItems(ID_ADD_RESULTS_TO_PLAYLIST_START, names);
    }
    if (m_submenuAddSelectedToPlaylist.isValid()) {
        m_submenuAddSelectedToPlaylist.replaceAllItems(ID_ADD_SELECTED_TO_PLAYLIST_START, names);
    }

    m_menuSearchResult->trackPopupMenu(pt.x, pt.y, m_pSkin);
}

void CMPlaylistCtrl::popupContexMenu(CPoint pt) {
    int count = getSelectedCount();

    m_menuCtx->enableItem(ID_SHOW_IN_FINDER, count > 0);
    m_menuCtx->enableItem(ID_PLAY_SELECTED_FILE, count > 0);

    const int MAX_COUNT = 10;
    VecStrings names;

    m_playlistNames = g_player.getMediaLibrary()->getRecentPlaylistBriefs();
    if (m_playlistNames.size() > MAX_COUNT) {
        m_playlistNames.resize(MAX_COUNT);
    }

    for (auto &name : m_playlistNames) {
        names.push_back(name.name);
    }

    if (m_ctxSubmenuAddToPlaylist.isValid()) {
        m_ctxSubmenuAddToPlaylist.replaceAllItems(ID_ADD_SELECTED_TO_PLAYLIST_START, names);
    }

    m_menuCtx->trackPopupMenu(pt.x, pt.y, m_pSkin);
}
