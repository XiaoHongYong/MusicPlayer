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
    /** 多封面切换时的上一张（与 artCrossfade 配合做交叉淡入） */
    const CSFImage *albumArtPrev = nullptr;
    /** 未做圆形裁剪的原图（plain-cover 等按比例适配控件用） */
    const CSFImage *albumArtOrg = nullptr;
    const CSFImage *albumArtOrgPrev = nullptr;
    /**
     * 封面交叉淡入进度：0=全显示 albumArtPrev，1=全显示 albumArt。
     * >= 1 或 albumArtPrev 为空时视为无转场。
     */
    float artCrossfade = 1.f;
    bool hasAlbumArt = false;

    const CColor *palette = nullptr;
    int paletteSize = 0;
    const CColor *rayColors = nullptr;
    int rayColorCount = 0;
    /** AudioReactor 等粒子用：色相尽量拉开的一组强调色 */
    const CColor *particleColors = nullptr;
    int particleColorCount = 0;

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
