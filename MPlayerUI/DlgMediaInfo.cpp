#include "MPlayerApp.h"
#include "DlgMediaInfo.h"
#include "../MediaTags/ID3/ID3v1.h"
#include "../MediaTags/LrcParser.h"


class CDlgMediaInfoPage : public CSkinContainer {
public:
    CDlgMediaInfoPage(CDlgMediaInfo *parent, cstr_t szPageId, cstr_t szAssociateTabButtonId) : CSkinContainer() {
        _parent = parent;
        _strAssociateTabButtonId = szAssociateTabButtonId;
        _strPageId = szPageId;
        _nAssociateTabButtonId = -1;
        _pageId = -1;
        _isModified = false;
    }

    virtual void onUpdateView(bool bRedraw = true) { }
    virtual void onSave() { }

    void onCreate() override {
        CSkinContainer::onCreate();

        _nAssociateTabButtonId = getIDByName(_strAssociateTabButtonId.c_str());
        _pageId = getIDByName(_strPageId.c_str());
    }

    void getFieldNewValue(int nID, string &strValue) {
        string str;

        str = getUIObjectText(nID);
        if (strcmp(str.c_str(), strValue.c_str()) != 0) {
            _isModified = true;
            strValue = str;
        }
    }

    int getAssociateTabButtonId() const { return _nAssociateTabButtonId; }

    void setEditorReadonly(int id, bool isReadonly) {
        auto obj = getUIObjectById(id, CSkinEditCtrl::className());
        if (obj) {
            auto editor = static_cast<CSkinEditCtrl *>(obj);
            auto style = editor->getStyle();
            if (isReadonly) {
                style |= CSkinEditCtrl::S_READ_ONLY;
            } else {
                style &= ~CSkinEditCtrl::S_READ_ONLY;
            }
            editor->setStyle(style);
        }
    }

protected:
    friend class CDlgMediaInfo;

    CDlgMediaInfo               *_parent;
    bool                        _isModified;
    string                      _strAssociateTabButtonId, _strPageId;
    int                         _nAssociateTabButtonId;
    int                         _pageId;

};

class CDlgMediaInfoPageBasic : public CDlgMediaInfoPage {
public:
    CDlgMediaInfoPageBasic(CDlgMediaInfo *parent) : CDlgMediaInfoPage(parent, "CID_C_MI_BASIC", "ID_MI_BASIC"), _tags(parent->_tags) {
    }

    void onCreate() {
        CDlgMediaInfoPage::onCreate();

        GET_ID_BY_NAME3(CID_E_ARTIST, CID_E_ALBUM, CID_E_TITLE);
        GET_ID_BY_NAME4(CID_E_TRACK, CID_E_YEAR, CID_CB_GENRE, CID_E_COMMENT);

        CSkinComboBox *pcbGenre;
        pcbGenre = (CSkinComboBox *)getUIObjectById(CID_CB_GENRE, CSkinComboBox::className());
        if (pcbGenre) {
            cstr_t *szGenre = CID3v1::GetAllGenreDescription();
            int n = CID3v1::GetGenreCount();
            for (int i = 0; i < n; i++) {
                pcbGenre->addString(szGenre[i]);
            }
        }
    }

    void onUpdateView(bool bRedraw = true) {
        _isModified = false;
        _idToValues = {
            { CID_E_ARTIST, &_tags.artist },
            { CID_E_ALBUM, &_tags.album },
            { CID_E_TITLE, &_tags.title },
            { CID_E_TRACK, &_tags.trackNo },
            { CID_E_YEAR, &_tags.year },
            { CID_CB_GENRE, &_tags.genre },
        };

        for (auto &item : _idToValues) {
            setUIObjectText(item.first, item.second->c_str(), bRedraw);
            setEditorReadonly(item.first, !_parent->_canModifyTags);
        }
    }

    virtual void onSave() {
        for (auto &item : _idToValues) {
            getFieldNewValue(item.first, *item.second);
        }
        _parent->_isTagsModified = _isModified;
    }

    BasicMediaTags              &_tags;
    int                         CID_E_ARTIST, CID_E_ALBUM, CID_E_TITLE;
    int                         CID_E_TRACK, CID_E_YEAR, CID_CB_GENRE, CID_E_COMMENT;
    std::vector<std::pair<int, string *>> _idToValues;

};

string formatFileSize(long size) {
    static cstr_t UNITS[] = { "KB", "MB", "G" };

    if (size < 1024) {
        return stringPrintf("%lld bytes", size);
    }

    int index = 0;
    double v = size / 1024.0;
    for (; v >= 1024 && index - 1 < (int)CountOf(UNITS); index++) {
        v /= 1024;
    }

    return stringPrintf("%.2f %s (%lld bytes)", v, UNITS[index], size);
}

class CDlgMediaInfoPageDetail : public CDlgMediaInfoPage {
public:
    CDlgMediaInfoPageDetail(CDlgMediaInfo *parent) : CDlgMediaInfoPage(parent, "CID_C_MI_DETAIL", "ID_MI_DETAIL"), _info(parent->_info), _parent(parent) {
    }

    void onCreate() {
        CDlgMediaInfoPage::onCreate();

        _detailsListCtrl = static_cast<CSkinListCtrl *>(getUIObjectById("LIST_DETAILS", CSkinListCtrl::className()));
        if (_detailsListCtrl) {
            _detailsListCtrl->addColumn("Name", 160);
            _detailsListCtrl->addColumn("Value", 450);
        }
    }

    void onUpdateView(bool bRedraw = true) {
        if (_detailsListCtrl) {
            _detailsListCtrl->deleteAllItems();

            int index = _detailsListCtrl->insertItem(-1, _TL("Duration"));
            _detailsListCtrl->setItemText(index, 1, formatDuration((_info.mediaLength + 500) / 1000));

            index = _detailsListCtrl->insertItem(-1, _TL("Sample rate"));
            _detailsListCtrl->setItemText(index, 1, stringPrintf("%d Hz", _info.sampleRate));

            index = _detailsListCtrl->insertItem(-1, _TL("Channels"));
            _detailsListCtrl->setItemText(index, 1, stringPrintf("%d", _info.channels));

            index = _detailsListCtrl->insertItem(-1, _TL("Bitrate"));
            _detailsListCtrl->setItemText(index, 1, stringPrintf("%d kbps", _info.bitRate));

            _detailsListCtrl->insertItem(-1, "");

            index = _detailsListCtrl->insertItem(-1, _TL("File Name"));
            _detailsListCtrl->setItemText(index, 1, fileGetName(_parent->_strMediaFile.c_str()));

            index = _detailsListCtrl->insertItem(-1, _TL("Path"));
            _detailsListCtrl->setItemText(index, 1, fileGetPath(_parent->_strMediaFile.c_str()));

            index = _detailsListCtrl->insertItem(-1, _TL("Kind"));
            _detailsListCtrl->setItemText(index, 1, MediaTags::getFileKind(_parent->_strMediaFile.c_str()));

            FileStatInfo fstat;
            getFileStatInfo(_parent->_strMediaFile.c_str(), fstat);

            index = _detailsListCtrl->insertItem(-1, _TL("Size"));
            _detailsListCtrl->setItemText(index, 1, formatFileSize(fstat.fileSize));

            index = _detailsListCtrl->insertItem(-1, _TL("Modified"));
            _detailsListCtrl->setItemText(index, 1, DateTime((int64_t)fstat.moifiedTime * 1000).toDateTimeString());

            index = _detailsListCtrl->insertItem(-1, _TL("Created"));
            _detailsListCtrl->setItemText(index, 1, DateTime((int64_t)fstat.createdTime * 1000).toDateTimeString());
        }
    }

protected:
    CDlgMediaInfo               *_parent;
    ExtendedMediaInfo           &_info;
    CSkinListCtrl               *_detailsListCtrl;

};

class CDlgMediaInfoPageLyrics : public CDlgMediaInfoPage {
public:
    CDlgMediaInfoPageLyrics(CDlgMediaInfo *parent) : CDlgMediaInfoPage(parent, "CID_C_MI_LYRICS", "ID_MI_LYRICS") {
    }

    void onCreate() {
        CDlgMediaInfoPage::onCreate();

        GET_ID_BY_NAME(CID_E_LYRICS);
    }

    void onUpdateView(bool bRedraw = true) {
        _isModified = false;

        setUIObjectText(CID_E_LYRICS, _parent->_lyrics.c_str());
        setEditorReadonly(CID_E_LYRICS, !_parent->_canModifyLyrics);
    }

    virtual void onSave() {
        getFieldNewValue(CID_E_LYRICS, _parent->_lyrics);
        _parent->_isLyricsModified = _isModified;
    }

protected:
    int                         CID_E_LYRICS;

};

//////////////////////////////////////////////////////////////////////////

class CDlgMediaInfoPagePictures : public CDlgMediaInfoPage {
public:
    CDlgMediaInfoPagePictures(CDlgMediaInfo *parent) : CDlgMediaInfoPage(parent, "CID_C_MI_PICTURES", "ID_MI_PICTURES") {
    }

    void onCreate() {
        CDlgMediaInfoPage::onCreate();

        _picListCtrl = (CSkinListCtrl *)getUIObjectById("LIST_PICTURES", CSkinListCtrl::className());
        if (_picListCtrl) {
            _picListCtrl->addColumn("Name", 250, CColHeader::TYPE_IMAGE, false, DT_CENTER);
        }
    }

    bool onCommand(uint32_t nId) {
        if (nId == ID_PIC_ACTIONS) {
            assert(_picListCtrl);
            if (_picListCtrl == nullptr) {
                return false;
            }

            CMenu menu;
            VecStrings options;

            menu.createPopupMenu();

            menu.appendItem(ID_ADD_PIC, _TL("&Add Picture..."));
            menu.appendItem(ID_DEL_PIC, _TL("&Delete Picture"));
            menu.appendItem(ID_SAVE_PIC_AS, _TL("&Save Picture as..."));

            if (_picListCtrl->getSelectedCount() == 0) {
                menu.enableItem(ID_DEL_PIC, false);
                menu.enableItem(ID_SAVE_PIC_AS, false);
            }

            CRect rc;
            getUIObjectRect(nId, rc);
            m_pSkin->clientToScreen(rc);
            menu.trackPopupMenu(rc.left, rc.bottom, m_pSkin);
        } else if (nId == ID_ADD_PIC) {
            // add picture
            CFileOpenDlg    dlg(_TLT("Browse picture file"),
                "", "All picture files (*.bmp; *.jpg; *.gif; *.png)\0*.bmp;*.jpg;*.jpeg;*.gif;*.png\0\0", 1);

            if (dlg.doModal(m_pSkin) != IDOK) {
                return true;
            }

            string picData;
            if (readFile(dlg.getOpenFile(), picData)) {
                _parent->_isPicturesModified = true;
                _parent->_pictures.push_back(picData);

                int index = _picListCtrl->insertItem(-1, "");
                auto image = loadRawImageDataFromMem(picData.c_str(), (uint32_t)picData.size());
                _picListCtrl->setItemImage(index, 0, image);
            } else {
                ERR_LOG1("Failed to read picture file: %s", dlg.getOpenFile());
            }
        } else if (nId == ID_DEL_PIC) {
            // delete picture
            while (true) {
                int index = _picListCtrl->getNextSelectedItem(-1);
                if (index != -1) {
                    _parent->_isPicturesModified = true;
                    _picListCtrl->deleteItem(index);
                    if (index < _parent->_pictures.size()) {
                        _parent->_pictures.erase(_parent->_pictures.begin() + index);
                    }
                } else {
                    break;
                }
            }
        } else if (nId == ID_SAVE_PIC_AS) {
            int nPos = -1;

            while (1) {
                nPos = _picListCtrl->getNextSelectedItem(nPos);
                if (nPos == -1) {
                    break;
                }

                assert(nPos < _parent->_pictures.size());
                assert(_picListCtrl->getItemCount() == _parent->_pictures.size());

                CFileSaveDlg    dlg(_TLT("Save picture as..."),
                    "", "All picture files (*.jpg; *.gif; *.png; *.bmp)\0*.jpg;*.gif;*.png;*.bmp\0\0", 1);

                if (dlg.doModal(m_pSkin) != IDOK) {
                    break;
                }
                string strExt = guessPictureDataExt(_parent->_pictures[nPos]);

                string file = dlg.getSaveFile();
                fileSetExt(file, strExt.c_str());

                writeFile(file.c_str(), _parent->_pictures[nPos]);
            }
        } else {
            return CDlgMediaInfoPage::onCommand(nId);
        }

        return true;
    }

    void onUpdateView(bool bRedraw = true) {
        _parent->_isPicturesModified = false;
        if (_picListCtrl) {
            _picListCtrl->deleteAllItems();

            for (auto &pic : _parent->_pictures) {
                int index = _picListCtrl->insertItem(-1, "");
                auto image = loadRawImageDataFromMem(pic.c_str(), (uint32_t)pic.size());
                _picListCtrl->setItemImage(index, 0, image);
            }
        }
    }

protected:
    CSkinListCtrl              *_picListCtrl;

};


//////////////////////////////////////////////////////////////////////////

CDlgMediaInfo::CDlgMediaInfo(const MediaPtr &media) : CMPSkinWnd(), _media(media) {
}

CDlgMediaInfo::~CDlgMediaInfo(void) {
}


CUIObject *CDlgMediaInfo::createUIObject(cstr_t className, CSkinContainer *container) {
    static std::pair<cstr_t, std::function<CDlgMediaInfoPage *(CDlgMediaInfo *)>> Pages[] = {
        { "Container.basic", [](CDlgMediaInfo *dlg) { return new CDlgMediaInfoPageBasic(dlg); } },
        { "Container.detail", [](CDlgMediaInfo *dlg) { return new CDlgMediaInfoPageDetail(dlg); } },
        { "Container.lyrics", [](CDlgMediaInfo *dlg) { return new CDlgMediaInfoPageLyrics(dlg); } },
        { "Container.pictures", [](CDlgMediaInfo *dlg) { return new CDlgMediaInfoPagePictures(dlg); } },
    };

    for (auto &page : Pages) {
        if (strcmp(className, page.first) == 0) {
            auto obj = page.second(this);
            obj->setParent(this, container);
            _vInfoPages.push_back(obj);
            return obj;
        }
    }

    return CMPSkinWnd::createUIObject(className, container);
}

void CDlgMediaInfo::onSkinLoaded() {
    CMPSkinWnd::onSkinLoaded();

    reloadMediaInfo();

    assert(!_vInfoPages.empty());
    auto defPageId = g_profile.getString("media-info", "active-page", _vInfoPages[0]->_strPageId.c_str());
    auto defPage = _vInfoPages[0];

    for (auto page : _vInfoPages) {
        if (page->_strPageId == defPageId) {
            defPage = page;
        }
    }

    CSkinToolbar*pToolbar = (CSkinToolbar *)getUIObjectById(m_pSkinFactory->getIDByName("CID_MEDIA_INFO_TAB"),
        CSkinToolbar::className());
    if (pToolbar) {
        pToolbar->setCheck(m_pSkinFactory->getIDByName(defPage->_strAssociateTabButtonId),
            true, false);
    }

    auto obj = getUIObjectById(defPage->_strPageId);
    if (obj && obj->isContainer()) {
        obj->getParent()->switchToPage(static_cast<CSkinContainer *>(obj), false, 0, false);
    }
}

void CDlgMediaInfo::onCommand(uint32_t nId) {
    for (auto page : _vInfoPages) {
        if (page->_nAssociateTabButtonId == nId) {
            auto obj = getUIObjectById(page->_strPageId);
            if (obj && obj->isContainer()) {
                obj->getParent()->switchToPage(static_cast<CSkinContainer *>(obj), false, 0, true);
                g_profile.writeString("media-info", "active-page", page->_strPageId.c_str());
                return;
            }
        }
    }

    CMPSkinWnd::onCommand(nId);
}

void CDlgMediaInfo::onDestroy() {
    _media = nullptr;

    CMPSkinWnd::onDestroy();
}

void converID3v1Tag(char szTag[], int nLen, cstr_t szValue, int nEncoding) {
    strcpy_safe(szTag, nLen, szValue);
}

void CDlgMediaInfo::onOK() {
    for (auto page : _vInfoPages) {
        page->onSave();
    }

    int ret = ERR_OK;
    if (_isTagsModified && _canModifyTags) {
        ret = MediaTags::setBasicTags(_strMediaFile.c_str(), _tags);
    }

    if (_isLyricsModified && ret == ERR_OK && _canModifyLyrics) {
        ret = MediaTags::saveEmbeddedLyrics(_strMediaFile.c_str(), _lyrics);
    }

    // id3v2 pictures
    if (_isPicturesModified && ret == ERR_OK && _canModifyPictures) {
        ret = MediaTags::setEmbeddedPictures(_strMediaFile.c_str(), _pictures);
    }

    if (ret != ERR_OK) {
        messageOut(ERROR2STR_LOCAL(ret));
        return;
    }

    g_player.updateMediaInfo(_media.get());

    CMPSkinWnd::onOK();
}

void CDlgMediaInfo::reloadMediaInfo() {
    //
    // get Media file name and info
    //
    if (!_media) {
        return;
    }

    _strMediaFile = _media->url;

    _isTagsModified = false;
    _isLyricsModified = false;
    _isPicturesModified = false;

    _canModifyTags = MediaTags::canSaveBasicTags(_strMediaFile.c_str());
    _canModifyLyrics = MediaTags::canSaveEmbeddedLyrics(_strMediaFile.c_str());
    _canModifyPictures = MediaTags::canSavePictures(_strMediaFile.c_str());

    MediaTags::getTags(_strMediaFile.c_str(), _tags, _info);

    auto vExistingLyrUrls = MediaTags::getEmbeddedLyrics(_strMediaFile.c_str());
    if (!vExistingLyrUrls.empty()) {
        RawLyrics rawLyrics;
        // The firsted returned lyrics should be the best lyrics.
        int ret = MediaTags::openEmbeddedLyrics(_strMediaFile.c_str(), vExistingLyrUrls[0].c_str(), false, ED_SYSDEF, rawLyrics);
        if (ret == ERR_OK) {
            _lyrics = toLyricsString(rawLyrics);
        }
        _lyricsOrg = _lyrics;
    }

    MediaTags::getEmbeddedPictures(_strMediaFile.c_str(), _pictures);

    for (auto page : _vInfoPages) {
        page->onUpdateView();
    }
}

void showMediaInfoDialog(CSkinWnd *pParent, const MediaPtr &media) {
    SkinWndStartupInfo skinWndStartupInfo(_SZ_SKINWND_CLASS_NAME, _SZ_SKINWND_CLASS_NAME,
        "MediaInfo.xml", pParent);

    CDlgMediaInfo *pWndMediaInfo = new CDlgMediaInfo(media);
    skinWndStartupInfo.pSkinWnd = pWndMediaInfo;

    CSkinApp::getInstance()->getSkinFactory()->activeOrCreateSkinWnd(skinWndStartupInfo);
}
