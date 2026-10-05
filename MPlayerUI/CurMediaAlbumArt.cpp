#include "MPlayerApp.h"
#include "CurMediaAlbumArt.h"
#include "AlbumArtQuery.h"
#include "AlbumArtDownloadMgr.h"


static cstr_t SZ_SUPPORTED_IMG_EXT[] = { ".jpg", ".gif", ".bmp", ".png" };

bool isSupportedImageFile(cstr_t szFile) {
    cstr_t szExt;
    int i;

    szExt = strrchr(szFile, '.');
    if (!szExt) {
        return false;
    }

    for (i = 0; i < CountOf(SZ_SUPPORTED_IMG_EXT); i++) {
        if (strcasecmp(szExt, SZ_SUPPORTED_IMG_EXT[i]) == 0) {
            return true;
        }
    }

    return false;
}

bool getCurrentMediaAlbumArtInSongDir(VecStrings &vPicFiles) {
    string strFile;
    cstr_t szAlbumName = g_player.getAlbum(), szSongFile = g_player.getSrcMedia();
    VecStrings vOtherPicFiles;
    int nFileCount = 0;

    if (!isFileExist(szSongFile)) {
        return false;
    }

    FileFind find;

    string strDir = fileGetPath(szSongFile);
    if (!find.openDir(strDir.c_str())) {
        return false;
    }

    string file;
    while (find.findNext()) {
        // enum every image file

        if (find.isCurDir()) {
            continue;
        }

        nFileCount++;
        if (!isSupportedImageFile(find.getCurName())) {
            continue;
        }

        strFile = strDir;
        strFile += find.getCurName();
        string strFileTitle = fileGetTitle(find.getCurName());

        //
        // album.jpg
        //
        if (strcasecmp(strFileTitle.c_str(), szAlbumName) == 0) {
            vPicFiles.push_back(strFile);
            continue;
        }

        //
        // artist - album.jpg
        //
        string strArAl;
        strArAl = g_player.getArtist();
        if (!isEmptyString(szAlbumName)) {
            strArAl = formatMediaTitle(g_player.getArtist(), szAlbumName);
        }
        if (strArAl.empty()) {
            strArAl = fileGetTitle(g_player.getSrcMedia());
        }
        if (strcasecmp(strFileTitle.c_str(), strArAl.c_str()) == 0) {
            vPicFiles.push_back(strFile);
            continue;
        }

        //
        // 与歌曲同名：Hey You.jpg
        //
        if (strcasecmp(strFileTitle.c_str(), fileGetTitle(szSongFile).c_str()) == 0) {
            vPicFiles.push_back(strFile);
            continue;
        }

        //
        // Folder.jpg
        //
        if (strcasecmp(strFileTitle.c_str(), "Folder") == 0) {
            vPicFiles.push_back(strFile);
            continue;
        }

        // other files
        vOtherPicFiles.push_back(strFile);
    }

    if (nFileCount < 60 && vPicFiles.size() < 1) {
        vPicFiles.insert(vPicFiles.begin(), vOtherPicFiles.begin(), vOtherPicFiles.end());
    }

    return true;
}

static void addAlbumArtFilesByTitles(const string &dir, const VecStrings &titles, VecStrings &vPicFiles, SetStrings &added) {
    if (dir.empty() || !isDirExist(dir.c_str())) {
        return;
    }
    static cstr_t exts[] = { ".jpg", ".png", ".gif", ".bmp" };
    for (auto &title : titles) {
        if (title.empty()) {
            continue;
        }
        for (auto *ext : exts) {
            string fn = dir + title + ext;
            if (isFileExist(fn.c_str()) && added.find(fn) == added.end()) {
                vPicFiles.push_back(fn);
                added.insert(fn);
            }
        }
    }
}

static void getCurrentMediaAlbumArtInDownloadDir(VecStrings &vPicFiles) {
    auto ids = extractMediaIdentityCandidates(g_player.getArtist(), g_player.getAlbum(),
        g_player.getTitle(), g_player.getSrcMedia());
    VecStrings titles;
    titles.push_back(fileGetTitle(g_player.getSrcMedia()));
    for (auto &id : ids) {
        titles.push_back(albumArtFileTitle(id));
    }

    SetStrings added;
    addAlbumArtFilesByTitles(g_albumArtDownloader.getCustomSaveDir(), titles, vPicFiles, added);
    addAlbumArtFilesByTitles(albumArtAppDataDir(), titles, vPicFiles, added);
}

CCurMediaAlbumArt::CCurMediaAlbumArt() {
}

CCurMediaAlbumArt::~CCurMediaAlbumArt() {
}

void CCurMediaAlbumArt::reset() {
    m_vAlbumPicFiles.clear();
    restartLoop();
}

void CCurMediaAlbumArt::restartLoop() {
    m_idxEmbeddedPicture = 0;
    m_idxFilePicture = 0;
}

RawImageDataPtr CCurMediaAlbumArt::loadNext() {
    string songFile = g_player.getSrcMedia();
    if (m_idxEmbeddedPicture != -1) {
        string picData;
        int ret = MediaTags::getEmbeddedPicture(songFile.c_str(), m_idxEmbeddedPicture, picData);
        if (ret == ERR_OK) {
            // 加载嵌入的图片
            m_idxEmbeddedPicture++;
            return loadRawImageDataFromMem(picData.c_str(), (int)picData.size());
        }

        // 没有嵌入的图片
        m_idxEmbeddedPicture = -1;
    }

    if (m_idxFilePicture != -1) {
        if (m_idxFilePicture == 0) {
            getCurrentMediaAlbumArtInSongDir(m_vAlbumPicFiles);
            if (m_vAlbumPicFiles.empty()) {
                getCurrentMediaAlbumArtInDownloadDir(m_vAlbumPicFiles);
            }
        }

        if (m_idxFilePicture < m_vAlbumPicFiles.size()) {
            int index = m_idxFilePicture;
            m_idxFilePicture++;
            if (m_idxFilePicture >= m_vAlbumPicFiles.size()) {
                m_idxFilePicture = -1;
            }
            return loadRawImageDataFromFile(m_vAlbumPicFiles[index].c_str());
        }
    }
    return nullptr;
}
