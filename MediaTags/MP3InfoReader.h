#pragma once

#include "../Utils/Utils.h"


class MP3Info {
public:
    enum MPAVersion {
        MPEG25                      = 0,
        MPEGReserved,
        MPEG2,
        MPEG1
    };

    enum MPALayer {
        Layer1,
        Layer2,
        Layer3,
        LayerReserved
    };

    enum Emphasis {
        EmphNone                    = 0,
        Emph5015,
        EmphReserved,
        EmphCCITJ17
    };

    enum ChannelMode {
        Stereo,
        JointStereo,
        DualChannel,
        SingleChannel
    };

    MPAVersion                  version = MPEG2;
    MPALayer                    layer = Layer3;
    Emphasis                    emphasis = EmphNone;
    ChannelMode                 channelMode = Stereo;
    uint32_t                    sampleRate = 0;
    uint8_t                     bitsPerSample = 16;
    uint32_t                    samplesPerFrame = 0;
    uint32_t                    frameSize = 0;
    uint32_t                    frameCount = 0;
    uint32_t                    firstFrameOffset = 0;
    size_t                      fileSize = 0;
    size_t                      bytesFrames = 0;
    uint32_t                    bitRate = 0;
    uint32_t                    duration = 0;

    bool                        hasCRC = false;
    bool                        isPrivate = false;
    uint8_t                     modeExt = 0;
    bool                        isCopyRight = false;
    bool                        isOriginal = false;
    bool                        hasToc = false;

    // only valid for MPEG 1 layer II (0=table a, 1=table b,...)
    uint16_t                    allocationTableIndex = 0;

    bool                        isXingToc = false;
    uint32_t                    tocVbriFramesPerEntry = 0;
    vector<int>                 toc;

    int getChannelCount() { return channelMode == SingleChannel ? 1 : 2; }

};

bool readMP3Info(FILE *fp, MP3Info &infoOut);
