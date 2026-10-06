# Personal Music Center — 产品需求文档

## 1. 产品定位

这是一个面向本地音乐库的 Web 音乐中心（Personal Music Center），数据源为本地 HTTP 服务可访问的音乐文件和媒体元数据。

产品不追求做成复杂的流媒体平台，而是强调四件事：

1. **本地媒体库管理**：扫描、浏览、搜索、评分、播放列表。
2. **高质量播放体验**：Web 播放、队列、全局播放器、远程控制、多设备状态同步。
3. **轻量但有价值的数据统计**：媒体库统计、评分统计、30 天播放历史。
4. **高质量 UI + AI 可维护性**：默认漂亮、主题可切换、组件尽量标准化、页面和功能按 Feature 组织。

## 2. 目标用户场景

### 2.1 本地音乐播放

用户打开 Web 应用后，可以快速看到最近播放、最近添加、收藏和高评分音乐，点击歌曲立即播放。

### 2.2 媒体库管理

用户可以按 Songs / Albums / Artists / Genres 浏览整个媒体库；媒体库规模上限约 100,000 首歌曲，不要求为每次筛选都访问后端。

### 2.3 远程控制

同一个服务可以同时被多个浏览器访问。一个浏览器负责实际播放，另一个浏览器作为 Remote Controller 控制播放、暂停、切歌、Seek、音量和队列。

### 2.4 统计

统计页面一次加载完整的统计快照，后续筛选、排序、联动全部在浏览器中完成，不把前端交互转化为复杂查询 API。

## 3. 页面信息架构

```text
App Shell
├── Home
├── Library
│   ├── Songs
│   ├── Albums
│   ├── Artists
│   └── Genres
├── Playlists
│   ├── Playlist List
│   └── Playlist Detail
├── History
├── Statistics
└── Settings

Persistent UI
├── Global Search / Command Palette
├── Mini Player
├── Queue Drawer
└── Fullscreen Now Playing
```

## 4. 页面 UI 设计

### 4.1 App Shell

桌面端采用三层结构：

```text
┌─────────────────────────────────────────────────────────────────┐
│ Top Bar: Search / Breadcrumb / Actions / Settings              │
├───────────────┬─────────────────────────────────────────────────┤
│ Sidebar       │ Main Content                                   │
│               │                                                 │
│ Home          │                                                 │
│ Library       │                                                 │
│  Songs        │                                                 │
│  Albums       │                                                 │
│  Artists      │                                                 │
│  Genres       │                                                 │
│ Playlists     │                                                 │
│ History       │                                                 │
│ Statistics    │                                                 │
│               │                                                 │
│               │                                                 │
├───────────────┴─────────────────────────────────────────────────┤
│ Persistent Mini Player + Progress                              │
└─────────────────────────────────────────────────────────────────┘
```

Sidebar 可折叠；移动端切换为底部导航 + Drawer。

### 4.2 Home

首页不是数据管理页面，而是“立即开始听”的 Dashboard。

推荐布局：

```text
Good evening
[ Search ]

Recently Played                    [View all]
┌────┐ ┌────┐ ┌────┐ ┌────┐
│Cover│ │Cover│ │Cover│ │Cover│
│Song │ │Song │ │Song │ │Song │
└────┘ └────┘ └────┘ └────┘

Recently Added
┌───────────────┐
│ Song list     │
└───────────────┘

Favorites / Top Rated / Most Played
┌──────────┐ ┌──────────┐ ┌──────────┐
│ Album    │ │ Artist   │ │ Playlist │
└──────────┘ └──────────┘ └──────────┘
```

首页只请求已经整理好的首页数据，不自己进行复杂统计。

### 4.3 Library — Songs

核心目标是“快速浏览 + 快速筛选 + 快速播放”。

```text
Songs                                      [Scan] [More]

[Search songs...] [Genre ▼] [Artist ▼] [Rating ▼] [Sort ▼]

┌───┬───────────────┬────────────┬──────────┬────────┬─────┬─────┐
│   │ Title         │ Artist     │ Album    │ Rating │ 歌词 │ ... │
├───┼───────────────┼────────────┼──────────┼────────┼─────┼─────┤
│▶  │ ...           │ ...        │ ...      │ ★★★★☆  │  有  │ ... │
└───┴───────────────┴────────────┴──────────┴────────┴─────┴─────┘

[1–50]                           [50 / page]
```

关键交互：

- 行 Hover 显示 Play / Add to Queue / Favorite / Rating。
- 「歌词」列根据 snapshot 的 `has_lyrics` 显示有/无；点击打开歌词查看（请求 `GET /songs/{id}/lyrics`）。
- 点击整行进入歌曲/专辑上下文，不打断播放。
- 筛选和排序使用前端内存数据。
- 表格建议使用虚拟滚动，确保 100,000 行数据仍保持流畅。

### 4.4 Library — Albums

卡片为主、表格为辅：

```text
Albums                              [Grid] [List]
[Search...] [Artist ▼] [Genre ▼] [Year ▼] [Sort ▼]

┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│  Cover   │ │  Cover   │ │  Cover   │ │  Cover   │
│          │ │          │ │          │ │          │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
Album A      Album B      Album C      Album D
Artist       Artist       Artist       Artist
```

### 4.5 Library — Artists

以视觉卡片 + 快速统计为主：

```text
Artists

┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ Avatar/Cover │ │ Avatar/Cover │ │ Avatar/Cover │
│ Artist       │ │ Artist       │ │ Artist       │
│ 32 albums    │ │ 8 albums     │ │ 4 albums     │
│ 421 tracks   │ │ 72 tracks    │ │ 38 tracks    │
└──────────────┘ └──────────────┘ └──────────────┘
```

### 4.6 Library — Genres

采用可视化入口：Genre Card + 数量 + 总播放量 + Top Artist。

### 4.7 Album Detail

```text
┌──────────────┬──────────────────────────────────────────────┐
│              │ Album Title                                   │
│    Cover     │ Artist · Year · N tracks                     │
│              │ [Play] [Shuffle] [Add to Playlist] [More]   │
└──────────────┴──────────────────────────────────────────────┘

Tracks
01  Song A                                03:21
02  Song B                                04:01
03  Song C                                05:42
...
```

### 4.8 Artist Detail

上方使用 Artist Hero；下面按 Album 列表、Top Songs、最近播放分区展示。

### 4.9 Playlists

Playlist 列表用卡片 + 最近更新时间；Playlist Detail 使用封面/标题/描述/歌曲表格。

```text
Playlist Name                           [Play] [Shuffle] [Edit]
12 tracks · 46 min

01 Song A
02 Song B
03 Song C
```

### 4.10 History

刻意做轻，不建设复杂 BI。

只展示最近 30 天：

```text
History                              [Last 30 days]

Today
  22:14  Song A — Artist
  21:42  Song B — Artist

Yesterday
  23:02  Song C — Artist
  22:31  Song A — Artist

Sep 30
  ...
```

后端可以返回按天聚合的数据；如果某一天同一首歌重复播放，可以直接展示 `× N`。

### 4.11 Statistics

统计页面采用“数据快照 + 前端联动”的设计。

```text
Statistics
[All time ▼] [Genre ▼] [Artist ▼] [Rating ▼]

┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
│Tracks  │ │Albums  │ │Artists │ │Plays   │
└────────┘ └────────┘ └────────┘ └────────┘

Listening Activity
┌──────────────────────────────────────────────────────────┐
│ ECharts line/bar chart                                   │
└──────────────────────────────────────────────────────────┘

Top Artists                  Rating Distribution
┌────────────────────────┐   ┌──────────────────────────┐
│ ECharts bar chart      │   │ ECharts bar/donut chart │
└────────────────────────┘   └──────────────────────────┘

Top Albums / Top Songs / Genre Distribution
```

统计页面的交互不调用“带复杂 filter 的 API”；所有可筛选维度来自已加载快照。

### 4.12 Settings

按分组做设置页面：

```text
Settings
├── Appearance
│   ├── Light / Dark / System
│   ├── Theme Color
│   ├── Density
│   └── Sidebar behavior
├── Library
│   ├── Music folders
│   ├── Auto scan
│   └── Scan now
├── Player
│   ├── Default volume
│   ├── Gapless preference
│   ├── Default repeat mode
│   └── Crossfade（后期）
└── About
```

### 4.13 Fullscreen Now Playing

从 Mini Player 点击封面进入全屏。桌面端左右分栏：左侧封面与控制，右侧歌词。

```text
┌─────────────────────────────────────────────────────────────────┐
│ ← Back                                              Queue       │
│                                                                 │
│        ┌──────────┐         上一行歌词（muted）                   │
│        │  COVER   │         ▶ 当前行（高亮、略放大）              │
│        └──────────┘         下一行歌词                            │
│        Song / Artist        …滚动跟随播放进度…                    │
│        ━━━━━○━━━━━                                              │
│           ◀ ▶ ▶                                                 │
└─────────────────────────────────────────────────────────────────┘
```

- 有时间轴的歌词按 `position` 高亮当前行并滚动居中。
- 纯文本歌词可滚动阅读，不高亮时间轴。
- 无歌词时显示空状态（「暂无歌词」），不挡住封面。
- 窄屏改为封面上方、歌词下方。

## 5. 非功能需求

- 首屏优先显示 Shell，不因加载大媒体库数据而阻塞 UI。
- 媒体库支持约 100,000 songs。
- 列表必须虚拟化。
- 主题切换无需刷新页面。
- 浏览器刷新后 Player 状态和队列可恢复。
- Remote Control 状态延迟目标 < 200 ms（同一局域网的正常环境）。
- 常规筛选、排序、rating 操作不能产生额外的复杂查询请求。

## 6. MVP 优先级

### P0

App Shell、Home、Songs、Albums、Artists、Genres、Album/Artist Detail、Streaming、Mini Player、Queue、播放控制、Now Playing 歌词、媒体库歌词查看、Light/Dark、主题色、Library Scan。

### P1

Playlists、Rating、Favorites、History、Statistics、WebSocket Remote Control、Search。

### P2

Waveform、Smart Playlist、Tag Editing、Crossfade、Recommendation、Duplicate Detection。
