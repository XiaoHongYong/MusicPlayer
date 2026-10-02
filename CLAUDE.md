# CLAUDE.md

自用跨平台桌面音乐播放器（作者个人项目，边做边改）。主开发平台为 Mac，兼容 Windows/Linux。

## 项目定位与技术栈

- **语言/构建**：C++17、CMake（生成 Xcode 工程后由 `xcodebuild` 编译）。PROJECT 名为 `MusicPlayer`，CMake 工程文件在根目录 `CMakeLists.txt`。
- **平台 UI**:macOS 用 Cocoa/AppKit（`MusicPlayer/`、`MPlayerUI/mac`），另外有 Win32（`*/win32`）与 GTK2（`*/gtk2`）两个旧分支。
- **跨平台封装**：`Window/`（窗口/菜单/对话框）和 `MPlayerEngine/`（播放内核，`mac|win32|linux` 子目录各自实现）。要新增平台能力时，改对应子目录实现而非平台无关代码。
- **皮肤系统**：`Skin/` 是基于 **TinyJS**（内嵌 JS 引擎）+ **Agg**（2D 图形渲染）的自绘皮肤引擎，UI 对象如 `JsXXX...` 直接暴露给 JS。
- **浏览器管理后台**：`LocalServer/` = 本地 HTTP Server（静态页）+ WebSocket Server，RSA 交换 AES 做通信加密（见 `design.md`）。歌库管理主要在这个后台里做。
- **媒体内核依赖**：faad2（MP4/AAC）、flac、minimp3（MP3）、mac 还可用 CoreMedia/AVPlayer；输出用 CoreAudio。第三方库全在 `third-parties/`。
- **标签/歌词**：`MediaTags/`（ID3/LRC/flac/ogg/m4a/aac 标签与歌词解析），`LyricsLib/`（歌词显示/当前歌词），`LyricServer/`、`MLProtocol/`（歌词云端相关）。
- **核心调度**：`MPlayerUI/MPlayerApp.cpp` 是应用入口（`MPlayerApp::getInstance()`），`MPlayer/` 负责媒体/播放列表/媒体库扫描。

## 构建与测试

- 一键构建脚本：`./build.sh [Release|Debug] [-g] [-b] [-p]`
  - `-g`：`cmake -G Xcode` 生成 Xcode 工程到 `build/`
  - `-b`：`xcodebuild -project build/MusicPlayer.xcodeproj -scheme MusicPlayer -configuration <type>` 编译
  - `-p`：打 dmg 包到 `../Release/<version>`
- 单元测试：CMake 配 `-DUT=ON`（定义 `UNIT_TEST`，链入 googletest）。测试在应用启动时由 `runAllUnittest()`（`TinyJS/utils/unittest.cpp`）执行 `RUN_ALL_TESTS()`。
  - 用 gtest 的 `TEST(...)` + `ASSERT_*` 宏（例：`MediaTags/LrcParser.cpp` 底部、`TinyJS/unittest/`）。
  - 纯 `-fsyntax-only` 语法检查（不链接）可加速验证，例如：
    `clang++ -std=c++17 -fsyntax-only -D_MAC_OS -I. -I./TinyJS MediaTags/LrcParser.cpp`
- 编码/格式：源码带 UTF-8 BOM，可用 `tools/add_utf8_bom.py`；`.clang-format` 风格由 `tools/cpp_format*.py` 处理。

## 重要约定与"坑"

- **Release 构建必须定义 `NDEBUG`**：根 `CMakeLists.txt` 已通过 `set_target_properties(... COMPILE_DEFINITIONS_RELEASE "NDEBUG")` 实现，改动构建配置时不要删掉。
- **歌词解析入口**：`MediaTags/LrcParser.cpp::parseLyricsString/parseLyricsBinary`；`_LrcParser::parse` 有"按纯文本重解析"递归分支会把歌词当 timestamps 处理，改动需谨慎。
- 代码主要注释为中文；命名风格：类/成员 `camelCase`，平台无关层保持三平台一致。
- 提交信息、PR 描述按 Claude Code 的归属规则落款（见下方 Git 部分）。

## Git 约定

- 主分支为 `master`。
- 提交信息用 `* 简述` 风格（历史多为 `* Fixed ...` / `* Add ...`）。
- 仅在用户要求时才 commit/push。