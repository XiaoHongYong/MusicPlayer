# 无封面时自动下载专辑封面

播放本地歌曲且没有封面时，后台线程自动搜索并下载封面。

顺序：**MusicBrainz + Cover Art Archive**（国际）→ 失败后 **国内源**（网易云 `cloudsearch`，再不行用 iTunes 港台/美区）。

## 触发

换歌时 `MPlayerApp::onMediaChanged` 会自动下载。封面右键菜单 **Download Album Art...** 会打开对话框，显示搜索/下载日志与错误（手动下载不受自动开关和一个月内去重限制）。

仅当同时满足：开关开启、当前是本地文件、`CCurMediaAlbumArt::loadNext()` 为空（无内嵌图、无歌曲目录约定文件名、无应用缓存）。

开关：偏好设置顶栏 **Album Art**（与 Lyrics 平级），或 `MusicPlayer.ini` 节 `[Download]`，键 `EnableAutoDownloadAlbumArt`，缺省 `true`。

来源：`AlbumArtSource`（0=自动 MusicBrainz 后国内源，1=仅 MusicBrainz，2=仅国内源）。

保存：`AlbumArtSaveMode`（0=歌曲同目录且与歌曲同名，1=专门下载目录 `AlbumArtDownPath`，缺省 `{getAppDataDir()}AlbumArt/`）。

## 源文件

| 文件 | 作用 |
|---|---|
| `MPlayerUI/AlbumArtQuery.*` | `extractMediaIdentityCandidates()`：从标签、`Artist - Title` 文件名、`Artist/Album/文件` 目录提取候选 `(artist, album, title)`，按准确度排序 |
| `MPlayerUI/IAlbumArtSource.h` | 搜索/下载抽象接口；换来源只改实现 |
| `MPlayerUI/MusicBrainzAlbumArtSource.*` | 首选：MusicBrainz 搜 release/recording，Cover Art Archive 下 `front-500` |
| `MPlayerUI/ChinaAlbumArtSource.*` | 国内兜底：网易云 `/api/cloudsearch/pc`（专辑 type=10 / 单曲 type=1，兼容 `al`/`ar`），旧 `/api/search/get/web` 作回退；再 iTunes `hk/tw/us`（中国区目录已空） |
| `MPlayerUI/AlbumArtDownloadMgr.*` | 下载线程、5 秒/张限速、多来源失败回退、落盘、完成后 `ET_PLAYER_CUR_MEDIA_INFO_CHANGED`；`downloadNow()` 给手动下载对话框用 |
| `MPlayerUI/DlgAlbumArtDownload.*`、`Skins-Design/skins/assets/DlgAlbumArtDownload.xml` | 封面菜单「Download Album Art...」：实时日志（候选、搜索结果、HTTP 错误、保存路径） |
| `MPlayerUI/PreferPageAlbumArt.*`、`Skins-Design/skins/assets/Pf_AlbumArt.xml` | 偏好设置顶栏 Album Art（与 Lyrics 平级） |
| `MPlayerUI/AlbumArtSearchHistory.*` | 一个月内搜过的查询不再搜（失败也记） |
| `MPlayerUI/CurMediaAlbumArt.cpp` | 加载顺序：内嵌 → 歌曲目录（专辑名 / `Artist - Album` / `Folder`）→ 应用数据缓存 |
| `Utils/HttpsGet.*`、`Utils/mac/HttpsGet.mm` | HTTPS GET（macOS 用 NSURL；其它平台 mbedtls） |

全局对象：`extern CAlbumArtDownloadMgr g_albumArtDownloader`。

## 搜索与下载

1. 按来源顺序：先 MusicBrainz，全部候选都失败后再走国内源。
2. 每个来源内按候选列表依次 `search`，结果按 score 降序，`downloadImage` 成功即停。
3. MusicBrainz User-Agent：`MusicPlayer/<version> ( https://www.crintsoft.com/music-player )`（对方强制要求）。
4. 国内源用浏览器 UA；网易云请求带 `Referer: https://music.163.com/`。iTunes Search 的 `country=cn` 已无目录，华语用 `hk`/`tw`。
5. 接口请求间隔约 1.1s；完成一张封面后再等 5s 处理下一任务。

换来源：实现 `IAlbumArtSource`。`createDefaultAlbumArtSource()` / `createChinaAlbumArtSource()` 在 `CAlbumArtDownloadMgr::init()` 里按顺序注册。

## 保存位置

- **歌曲同目录且同名**：`{歌曲目录}{歌曲文件名}{ext}`，例如 `Hey You.mp3` → `Hey You.jpg`。
- **专门下载目录**：`AlbumArtDownPath`（缺省应用数据下的 `AlbumArt/`），文件名用 `artist - album`。
- 首选位置写失败时会试另一处。

显示加载：内嵌 → 歌曲目录（专辑名 / `Artist - Album` / 歌曲同名 / `Folder`）→ 专门下载目录 → 应用数据 `AlbumArt/`。

历史文件：`getAppDataFile("AlbumArtSearchHistory.txt")`，键为 `lowercase(artist)|lowercase(album 或 title)`。

## 注意

新增 `MPlayerUI/*.cpp` 后须 `./build.sh -g` 再编译。CMake 用 `aux_source_directory`，不重新生成工程会链接不到新符号。
