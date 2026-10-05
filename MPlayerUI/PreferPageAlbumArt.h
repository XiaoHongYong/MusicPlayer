#ifndef _PREFER_PAGE_ALBUM_ART_H_
#define _PREFER_PAGE_ALBUM_ART_H_

#pragma once

#include "PreferencePageBase.h"


class CPagePfAlbumArtRoot : public CPagePfBase {
    UIOBJECT_CLASS_NAME_DECLARE(CPagePfBase)
public:
    CPagePfAlbumArtRoot();

    void onInitialUpdate() override;

};

void registerPfAlbumArtPages(CSkinFactory *pSkinFactory);

#endif // _PREFER_PAGE_ALBUM_ART_H_
