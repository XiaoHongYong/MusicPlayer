#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 macOS 红绿灯标题栏按钮精灵图 caption_btn_mac.png。

5 列 × 3 行、16px/格，列语义与 caption_btn.png 一致：
  0 留空, 1=最小化, 2=最大化, 3=还原, 4=关闭。
行 = normal / hover / pressed。mac 布局里 XML 按钮顺序为 关闭、最小化、最大化。

用法: python3 Skins-Design/raw/caption_btn_mac.py
输出: Skins-Design/skins/assets/caption_btn_mac.png (+ @2x)
"""
import os
from PIL import Image, ImageDraw

SS = 4
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       '..', 'skins', 'assets')

# 标准 macOS 红绿灯
CLOSE = (255, 95, 87)
CLOSE_D = (200, 70, 64)
MINI = (254, 188, 46)
MINI_D = (210, 150, 30)
MAXI = (40, 200, 64)
MAXI_D = (28, 150, 48)
GLYPH = (60, 60, 60)
WHITE = (255, 255, 255)


def new_canvas(w, h):
    return Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))


def save_asset(name, img):
    w, h = img.size
    img2x = img.resize((w // (SS // 2), h // (SS // 2)), Image.LANCZOS)
    img1x = img.resize((w // SS, h // SS), Image.LANCZOS)
    img1x.save(os.path.join(OUT_DIR, name + '.png'))
    img2x.save(os.path.join(OUT_DIR, name + '@2x.png'))
    print('  ', name, img1x.size, '+', img2x.size)


def light_color(col, row):
    if col == 1:
        return MINI_D if row == 2 else MINI
    if col in (2, 3):
        return MAXI_D if row == 2 else MAXI
    if col == 4:
        return CLOSE_D if row == 2 else CLOSE
    return (180, 180, 180)


def draw_glyph(draw, col, s, o):
    c = GLYPH + (220,)
    if col == 1:  # 减号
        draw.line([4 * s, o // 2, 12 * s, o // 2], fill=c, width=2 * s)
    elif col == 2:  # 加号 / 放大
        cx, cy = o // 2, o // 2
        draw.line([cx, 4 * s, cx, 12 * s], fill=c, width=2 * s)
        draw.line([4 * s, cy, 12 * s, cy], fill=c, width=2 * s)
    elif col == 3:  # 还原：小加号
        cx, cy = o // 2, o // 2
        draw.line([cx, 5 * s, cx, 11 * s], fill=c, width=2 * s)
        draw.line([5 * s, cy, 11 * s, cy], fill=c, width=2 * s)
    elif col == 4:  # x
        draw.line([5 * s, 5 * s, 11 * s, 11 * s], fill=c, width=2 * s)
        draw.line([11 * s, 5 * s, 5 * s, 11 * s], fill=c, width=2 * s)


def gen_caption_btn_mac(cell=16, name='caption_btn_mac'):
    sheet = new_canvas(cell * 5, cell * 3)
    s = SS
    o = cell * s
    pad = 2 * s
    for col in range(5):
        for row in range(3):
            cell_img = new_canvas(cell, cell)
            d = ImageDraw.Draw(cell_img)
            if col == 0:
                sheet.paste(cell_img, (col * o, row * o))
                continue
            color = light_color(col, row)
            d.ellipse([pad, pad, o - pad - 1, o - pad - 1], fill=color + (255,))
            if row >= 1:
                draw_glyph(d, col, s, o)
            sheet.paste(cell_img, (col * o, row * o), cell_img)
    save_asset(name, sheet)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print('Output:', os.path.abspath(OUT_DIR))
    gen_caption_btn_mac(16, 'caption_btn_mac')
    print('Done.')


if __name__ == '__main__':
    main()
