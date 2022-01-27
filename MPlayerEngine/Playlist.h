#pragma once

#include "IMPlayer.h"
#include "MPTools.h"
#include "MPTime.h"
#include "Media.h"

class CMPlayer;

class CPlaylist : public IPlaylist  
{
    OBJ_REFERENCE_DECL
public:
    CPlaylist(CMPlayer *pPlayer);
    virtual ~CPlaylist();

    virtual uint32_t getCount();

    virtual MLRESULT getItem(long nIndex, IMedia **ppMedia);

    virtual MLRESULT getName(IXStr *str);

    virtual MLRESULT insertItem(long nIndex, IMedia *pMedia);

    virtual MLRESULT moveItem(long nIndexOld, long nIndexNew);

    virtual MLRESULT removeItem(long nIndex);

    virtual MLRESULT clear();

public:
    MLRESULT getItemIndex(IMedia *pMedia, long &nIndex);

protected:
    typedef vector<CMedia *>        V_MEDIA;

    std::mutex                  m_mutexDataAccess;
    V_MEDIA                     m_vMedia;

    CMPlayer                    *m_pPlayer;

};
