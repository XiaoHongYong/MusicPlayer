#include "RawBmpFont.h"
#include "RawGlyphSet.hpp"

static void cropGlyphBitmapTop(Glyph *glyph, int rows) {
    if (rows <= 0 || rows >= (int)glyph->heightBitmap) {
        return;
    }

    const int dstW = (int)glyph->widthBitmap;
    const int dstH = (int)glyph->heightBitmap - rows;
    auto *dst = new uint8_t[dstW * dstH * G_PIX_SIZE];
    memcpy(dst, glyph->bitmap + rows * dstW * G_PIX_SIZE, dstW * dstH * G_PIX_SIZE);

    delete[] glyph->bitmap;
    glyph->bitmap = dst;
    glyph->heightBitmap = (uint8_t)dstH;
}

static void cropGlyphBitmapBottom(Glyph *glyph, int rows) {
    if (rows <= 0 || rows >= (int)glyph->heightBitmap) {
        return;
    }

    const int dstW = (int)glyph->widthBitmap;
    const int dstH = (int)glyph->heightBitmap - rows;
    auto *dst = new uint8_t[dstW * dstH * G_PIX_SIZE];
    memcpy(dst, glyph->bitmap, dstW * dstH * G_PIX_SIZE);

    delete[] glyph->bitmap;
    glyph->bitmap = dst;
    glyph->heightBitmap = (uint8_t)dstH;
}

void fitGlyphBitmapToBaseline(Glyph *glyph, int fontHeight, int bakeBaselineY, int refAscent, int bottomPad, int topPad) {
    if (!glyph || !glyph->bitmap || glyph->heightBitmap == 0 || fontHeight < 1) {
        return;
    }

    // 绘制时目的 y = topOffset - MARGIN_FONT，字号盒子是 [0, fontHeight)。
    if (topPad < 0) {
        topPad = 0;
    }
    const int maxBottom = fontHeight + MARGIN_FONT;
    const int minTop = MARGIN_FONT + topPad;
    const int inkHeight = fontHeight - topPad;
    int baseInCell = minTop + refAscent;
    // ascent 大于字号时，基线不能贴在单元格底边，否则下沿 1～2px 会被当成 descent 裁掉。
    int baseLimit = fontHeight - bottomPad;
    if (baseLimit > fontHeight) {
        baseLimit = fontHeight;
    }
    if (baseLimit < minTop) {
        baseLimit = minTop;
    }
    if (baseInCell > baseLimit) {
        baseInCell = baseLimit;
    }
    if (baseInCell < minTop) {
        baseInCell = minTop;
    }

    // 烘焙缓冲中基线到墨迹顶的距离（行号，与 topOffset 同坐标系）
    int inkToBase = bakeBaselineY - (int)glyph->topOffset;
    if (inkToBase < 1) {
        inkToBase = 1;
    }

    // 过高时优先裁基线以上，再裁以下，避免「从 bbox 顶硬裁」使各字母 inkToBase 不一致
    if (inkHeight > 0 && (int)glyph->heightBitmap > inkHeight) {
        int excess = (int)glyph->heightBitmap - inkHeight;
        const int aboveBase = inkToBase;
        const int belowBase = (int)glyph->heightBitmap - inkToBase;

        int cropTop = min(excess, max(0, aboveBase - 1));
        if (cropTop > 0) {
            cropGlyphBitmapTop(glyph, cropTop);
            inkToBase -= cropTop;
            excess -= cropTop;
        }
        if (excess > 0) {
            int cropBottom = min(excess, max(0, belowBase - 1));
            if (cropBottom > 0) {
                cropGlyphBitmapBottom(glyph, cropBottom);
                excess -= cropBottom;
            }
        }
    }

    int newTop = baseInCell - inkToBase;

    // 需要上移时裁掉基线以上的行，而不是只改 newTop（否则会破坏词内基线）
    if (newTop < minTop) {
        const int shift = minTop - newTop;
        const int cropTop = min(shift, max(0, inkToBase - 1));
        if (cropTop > 0) {
            cropGlyphBitmapTop(glyph, cropTop);
            inkToBase -= cropTop;
            newTop += cropTop;
        }
    }

    if (newTop + (int)glyph->heightBitmap > maxBottom) {
        int overflow = newTop + (int)glyph->heightBitmap - maxBottom;
        int belowBase = (int)glyph->heightBitmap - inkToBase;
        int cropBottom = min(overflow, max(0, belowBase));
        if (cropBottom > 0) {
            cropGlyphBitmapBottom(glyph, cropBottom);
            overflow -= cropBottom;
        }
        if (overflow > 0) {
            int cropTop = min(overflow, max(0, inkToBase - 1));
            if (cropTop > 0) {
                cropGlyphBitmapTop(glyph, cropTop);
                inkToBase -= cropTop;
                newTop += cropTop;
            }
        }
    }

    if (newTop < minTop) {
        newTop = minTop;
    }
    if (newTop + (int)glyph->heightBitmap > maxBottom && glyph->heightBitmap > 0) {
        newTop = maxBottom - (int)glyph->heightBitmap;
        if (newTop < minTop) {
            newTop = minTop;
        }
    }

    glyph->topOffset = (uint8_t)newTop;
}
