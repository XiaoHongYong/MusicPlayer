#pragma once

#include "../PluginWMP/wmp.h"
#include "MDAgent.h"

class CMDWmpCore: public CMDAgent
{
    OBJ_REFERENCE_DECL
public:
    CMDWmpCore(void);
    ~CMDWmpCore(void);

    //
    // individual methods
    //

    virtual cstr_t getDescription();

    virtual cstr_t getFileExtentions();
    virtual MLRESULT getMediaInfo(IMPlayer *pPlayer, IMediaInput *pInput, IMedia *pMedia);

    //
    // decode media file related methods
    //

    virtual bool isSeekable();
    virtual bool isUseOutputPlug();

    // ERR_PLAYER_INVALID_FILE: decoder can't decode the file
    // ERR_DECODER_INNER_ERROR: inner error occurs at decoder
    // ERR_DECODER_UNSUPPORTED_FEATURE:
    // ERR_DECODER_INIT_FAILED:
    virtual MLRESULT play(IMPlayer *pPlayer, IMediaInput *pInput);
    virtual MLRESULT pause();
    virtual MLRESULT unpause();
    virtual MLRESULT stop();

    // media length, pos related functions, unit: ms
    virtual uint32_t getLength();
    virtual MLRESULT seek(uint32_t nPos);
    virtual uint32_t getPos();

    // volume
    virtual MLRESULT setVolume(int nVolume, int nBanlance);

protected:
    bool init();

    virtual MLRESULT init(CMPlayer *pPlayer) { return CMDAgent::init(pPlayer); }
    virtual MLRESULT doDecode(IMedia *pMedia);

    virtual void notifyEod(IMediaDecode *pDecoder, MLRESULT nError) { }

    bool isOK();

protected:
    friend class CWmpEventsSink;

    CMPAutoPtr<IMPlayer> m_mplayer;

    CMPAutoPtr<IWMPPlayer> m_player;
    CMPAutoPtr<IWMPControls> m_controls;
    CMPAutoPtr<IWMPPlaylist> m_playlist;
    CMPAutoPtr<IWMPMediaCollection> m_collection;

    CMPAutoPtr<IConnectionPoint> m_pConnectionPoint;
    uint32_t m_dwAdviseCookie;

};

