#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从母版生成各平台应用图标。

母版: Skins-Design/raw/app-icon-master-1024.png
产物（已在 .gitignore）:
  - MusicPlayer/MusicPlayer.icns
  - MusicPlayer/Assets.xcassets/AppIcon.appiconset/icon_*.png
  - MusicPlayer/win32/player.ico
  - MusicPlayer/tray-icons/menu-logo{,@2x}.png
  - Skins-Design/skins/assets/logo.png
  - LocalServer/www/public/favicon.ico
  - LocalServer/www/public/favicon-16.png
  - LocalServer/www/public/favicon-32.png
  - LocalServer/www/public/apple-touch-icon.png
  - LocalServer/www/public/app-icon.png

用法:
  python3 tools/generate_app_icons.py
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image, ImageFilter
except ImportError:
    print("需要 Pillow: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "Skins-Design" / "raw" / "app-icon-master-1024.png"

ICNS_OUT = ROOT / "MusicPlayer" / "MusicPlayer.icns"
APPICONSET = ROOT / "MusicPlayer" / "Assets.xcassets" / "AppIcon.appiconset"
ICO_OUT = ROOT / "MusicPlayer" / "win32" / "player.ico"
LOGO_OUT = ROOT / "Skins-Design" / "skins" / "assets" / "logo.png"
MENU_LOGO = ROOT / "MusicPlayer" / "tray-icons" / "menu-logo.png"
MENU_LOGO_2X = ROOT / "MusicPlayer" / "tray-icons" / "menu-logo@2x.png"
WWW_PUBLIC = ROOT / "LocalServer" / "www" / "public"
WWW_FAVICON_ICO = WWW_PUBLIC / "favicon.ico"
WWW_FAVICON_16 = WWW_PUBLIC / "favicon-16.png"
WWW_FAVICON_32 = WWW_PUBLIC / "favicon-32.png"
WWW_APPLE_TOUCH = WWW_PUBLIC / "apple-touch-icon.png"
WWW_APP_ICON = WWW_PUBLIC / "app-icon.png"

FAVICON_ICO_SIZES = [16, 32, 48]

# macOS iconutil 命名: (文件名, 像素边长)
ICONSET_SIZES = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
]

# Contents.json 里的 slot: (文件名, size 字段, scale)
APPICON_SLOTS = [
    ("icon_16x16.png", "16x16", "1x"),
    ("icon_16x16@2x.png", "16x16", "2x"),
    ("icon_32x32.png", "32x32", "1x"),
    ("icon_32x32@2x.png", "32x32", "2x"),
    ("icon_128x128.png", "128x128", "1x"),
    ("icon_128x128@2x.png", "128x128", "2x"),
    ("icon_256x256.png", "256x256", "1x"),
    ("icon_256x256@2x.png", "256x256", "2x"),
    ("icon_512x512.png", "512x512", "1x"),
    ("icon_512x512@2x.png", "512x512", "2x"),
]

ICO_SIZES = [16, 32, 48, 64, 128, 256]


def resize(master: Image.Image, size: int) -> Image.Image:
    im = master.resize((size, size), Image.Resampling.LANCZOS)
    # 小尺寸略锐化，避免糊成一团
    if size <= 32:
        im = im.filter(ImageFilter.UnsharpMask(radius=0.6, percent=140, threshold=1))
    return im


def write_png(path: Path, im: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, "PNG")
    print(f"  {path.relative_to(ROOT)} ({im.size[0]}x{im.size[1]})")


def build_icns(master: Image.Image) -> None:
    if sys.platform != "darwin":
        print("  跳过 .icns（非 macOS，无 iconutil）")
        return

    with tempfile.TemporaryDirectory(prefix="MusicPlayer.iconset.") as tmp:
        # iconutil 要求目录名以 .iconset 结尾
        iconset = Path(tmp) / "MusicPlayer.iconset"
        iconset.mkdir()
        for name, size in ICONSET_SIZES:
            resize(master, size).save(iconset / name, "PNG")

        ICNS_OUT.parent.mkdir(parents=True, exist_ok=True)
        subprocess.check_call(
            ["iconutil", "--convert", "icns", "--output", str(ICNS_OUT), str(iconset)]
        )
        print(f"  {ICNS_OUT.relative_to(ROOT)} ({ICNS_OUT.stat().st_size} bytes)")


def build_appiconset(master: Image.Image) -> None:
    APPICONSET.mkdir(parents=True, exist_ok=True)
    # 清掉旧 png，避免残留尺寸
    for old in APPICONSET.glob("icon_*.png"):
        old.unlink()

    for name, size in ICONSET_SIZES:
        write_png(APPICONSET / name, resize(master, size))

    contents = {
        "images": [
            {"filename": name, "idiom": "mac", "scale": scale, "size": size}
            for name, size, scale in APPICON_SLOTS
        ],
        "info": {"author": "xcode", "version": 1},
    }
    contents_path = APPICONSET / "Contents.json"
    contents_path.write_text(json.dumps(contents, indent=2) + "\n", encoding="utf-8")
    print(f"  {contents_path.relative_to(ROOT)}")


def write_ico(path: Path, master: Image.Image, sizes: list[int]) -> None:
    imgs = [resize(master, s) for s in sizes]
    path.parent.mkdir(parents=True, exist_ok=True)
    imgs[-1].save(
        path,
        format="ICO",
        sizes=[(s, s) for s in sizes],
        append_images=imgs[:-1],
    )
    print(f"  {path.relative_to(ROOT)} ({path.stat().st_size} bytes)")


def build_ico(master: Image.Image) -> None:
    write_ico(ICO_OUT, master, ICO_SIZES)


def build_www_icons(master: Image.Image) -> None:
    write_ico(WWW_FAVICON_ICO, master, FAVICON_ICO_SIZES)
    write_png(WWW_FAVICON_16, resize(master, 16))
    write_png(WWW_FAVICON_32, resize(master, 32))
    write_png(WWW_APPLE_TOUCH, resize(master, 180))
    write_png(WWW_APP_ICON, resize(master, 64))


def main() -> int:
    if not MASTER.is_file():
        print(f"找不到母版: {MASTER}", file=sys.stderr)
        return 1

    master = Image.open(MASTER).convert("RGBA")
    if master.size != (1024, 1024):
        print(f"警告: 母版尺寸为 {master.size}，期望 1024x1024，将按比例缩放")
        master = master.resize((1024, 1024), Image.Resampling.LANCZOS)

    print(f"母版: {MASTER.relative_to(ROOT)} ({master.mode})")
    print("生成:")

    build_icns(master)
    build_appiconset(master)
    build_ico(master)
    write_png(LOGO_OUT, resize(master, 92))
    write_png(MENU_LOGO, resize(master, 24))
    write_png(MENU_LOGO_2X, resize(master, 48))
    build_www_icons(master)

    print("完成.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
