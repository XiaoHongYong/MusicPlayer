# MusicPlayer

一款小巧、快速、自绘界面的桌面音乐播放器。现在的音乐播放器功能越来越多、体积越来越大、很慢也不好用——这个项目反其道而行：它播放你的音乐、显示歌词，然后退到一边，不打扰你。这是一个个人项目，为我自己喜欢而做，只能抽空一点点打磨。

## 功能介绍

* **歌曲播放**，支持多种格式（MP3、AAC/M4A、OGG、FLAC、WAV 等）
* **滚动歌词显示**，与当前播放曲目同步
* **小巧、资源占用少**——对内存、CPU 和磁盘都很轻量
* **网页版媒体中心**——在浏览器里管理你的歌库：专辑、歌手、搜索、统计等
* **很酷的皮肤**——可切换、可自定义，内置 Neon / Glass 皮肤
* **开源、免费、无广告**
* **跨平台**：Windows、macOS

## 开发

### 技术栈

* C++，C++17
* macOS UI：Cocoa / AppKit；Windows：Win32（另有旧版 GTK2 分支）
* CMake（在 macOS 上生成 Xcode 工程，由 `xcodebuild` 编译）
* 渲染：Agg（2D 图形）、TinyJS（内嵌 JS 引擎）用于皮肤系统
* 媒体内核：faad2（MP4/AAC）、flac、minimp3（MP3）；macOS 上还可使用 CoreMedia / AVPlayer；输出用 CoreAudio
* 网页版媒体中心：Vue + Quasar（前端），后端由 C++ `LocalServer` 承载（REST + SSE）

### 开发环境

| 工具 | 作用 |
|---|---|
| C++ / VSCode | 主要语言与编辑器 |
| Windows: Visual Studio Community 2022 | Windows 上构建 |
| Mac: Xcode | macOS 上构建 |
| CMake | 构建系统 |
| Node.js 18（Vue + Quasar） | 网页版媒体中心前端 |
| Python 3 | 编译脚本需要 |

### 构建

```bash
./build.sh Release -g -b   # -g 生成 Xcode 工程，-b 编译
```

完整构建选项（`-p` 打 dmg 包、`-DUT=ON` 跑单元测试、i18n 流程及各种注意事项）见 [CLAUDE.md](CLAUDE.md)。

## 文档

- 皮肤制作：[`docs/skin-authoring.md`](docs/skin-authoring.md)
- 皮肤编译器：[`docs/skin-compiler.md`](docs/skin-compiler.md)
- 网页版媒体中心设计与路线图：[`docs/web-console/README.md`](docs/web-console/README.md)
- 专辑封面下载：[`docs/album-art-download.md`](docs/album-art-download.md)

## 协议

开源，基于 [MIT License](LICENSE-MIT)。免费、无广告。