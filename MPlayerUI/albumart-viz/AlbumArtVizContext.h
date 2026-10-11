#pragma once

#include "../../GfxRaw/RawGraph.h"
#include "../../MPlayerEngine/analysis/AudioAnalysisFrame.h"
#include "../../Skin/SFImage.h"

#include <cstdint>

/**
 * 封面可视化共享输入：由宿主每帧组装，特效 / 转场只读使用。
 *
 *   lowEnergy / bassPulse / bassStrength / lowOnset → Bass 专属
 *   beatPulse / beatStrength / onsetStrength       → 通用打击 / 起音
 */
struct AlbumArtVizContext {
    CRawGraph *canvas = nullptr;
    CRect bounds;
    float scaleFactor = 1.f;

    const CSFImage *albumArt = nullptr;
    bool hasAlbumArt = false;

    const CColor *palette = nullptr;
    int paletteSize = 0;
    const CColor *rayColors = nullptr;
    int rayColorCount = 0;

    const AudioAnalysisFrame *frame = nullptr;
    bool hasFrame = false;

    const float *displayLevels = nullptr;
    int bandCount = 0;

    float artRatio = 0.64f;

    float lowEnergy = 0.f;
    float lowOnset = 0.f;
    float lowOnsetStrength = 0.f;
    float bassPulse = 0.f;
    float bassStrength = 0.f;
    float beatPulse = 0.f;
    float beatStrength = 0.f;
    float onsetStrength = 0.f;
    float bassRhythmRate = 0.f;
    float rhythmConfidence = 0.f;

    float beatFlash = 0.f; // == beatPulse（兼容）
    float ringSmooth = 0.f;
    double nowSec = 0.0;
};
