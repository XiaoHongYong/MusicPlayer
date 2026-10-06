# Personal Music Center — 技术架构文档

## 1. 架构目标

本项目优先保证：

- 单机部署简单。
- 本地文件访问高效。
- Web 播放与远程控制稳定。
- 前端数据集加载后可长时间本地交互。
- API 简单、可生成 TypeScript Client。
- 领域边界清晰，适合 AI Agent 按 Feature 开发。

## 2. 总体架构

```text
                         Browser / Web App
┌─────────────────────────────────────────────────────────────┐
│ React + TypeScript                                         │
│                                                             │
│ App Shell                                                   │
│ ├─ TanStack Query      Server snapshot/cache                │
│ ├─ Zustand              Player/UI state                     │
│ ├─ Crossfilter2         Local filtering                    │
│ └─ ECharts              Statistics visualization             │
│                                                             │
│ HTMLAudioElement + Media Session API                        │
└───────────────┬───────────────────────────┬─────────────────┘
                │ REST                      │ SSE (events)
                ▼                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Rust + Axum                                                  │
│                                                             │
│ API Layer                                                   │
│ ├─ Bootstrap                                                │
│ ├─ Library                                                  │
│ ├─ Player                                                   │
│ ├─ Lyrics                                                   │
│ ├─ Playlist                                                 │
│ ├─ Rating                                                   │
│ ├─ History                                                  │
│ └─ Statistics                                               │
│                                                             │
│ Domain / Application Services                               │
│ ├─ Library Scanner                                          │
│ ├─ Metadata Parser                                          │
│ ├─ Playback State                                           │
│ ├─ Statistics Aggregator                                    │
│ └─ Realtime Event Hub                                       │
│                                                             │
│ Repository                                                  │
│ └─ SQLx                                                     │
└───────────────┬─────────────────────┬────────────────────────┘
                │                     │
                ▼                     ▼
          SQLite Database        Local Music Files
```

## 3. 技术选型

| Layer | Recommendation |
|---|---|
| Frontend | React + TypeScript |
| Build | Vite |
| UI | shadcn/ui |
| CSS | Tailwind CSS |
| Icons | Lucide |
| Routing | React Router |
| Client state | Zustand |
| Server/cache state | TanStack Query |
| Local filtering | crossfilter2 |
| Charts | ECharts |
| Backend | Rust + Axum |
| DB | SQLite |
| ORM/DB access | SQLx |
| Serialization | Serde |
| API contract | OpenAPI |
| Realtime | SSE (`GET /api/v1/events`) |
| Audio | HTMLAudioElement |
| OS media integration | Media Session API |
| E2E | Playwright |

## 4. 数据加载策略

核心原则：**把“数据获取”与“数据交互”分开。**

### 4.1 Bootstrap

启动时只取得轻量数据：

- App settings
- Player state
- 当前队列
- Library summary
- Server capabilities

不要让 bootstrap 强制携带整个 100,000 条媒体库数据。

### 4.2 Library Snapshot

进入 Library 或应用启动后后台请求：

```http
GET /api/v1/library/snapshot
```

一次返回整个媒体库所需的前端数据集。

建议数据集：

```text
artists[]
albums[]
genres[]
songs[]
```

`songs[]` 可以适当做 denormalized，直接携带：

- artist name/id
- album name/id
- genre name/id
- rating
- play_count
- year
- duration
- has_lyrics

这样列表筛选无需为了显示名称进行前端多表 join。歌词正文不进 snapshot，按需请求 `/songs/{id}/lyrics`。

### 4.3 Statistics Snapshot

统计页面一次获取完整统计快照：

```http
GET /api/v1/statistics/snapshot
```

返回的是“可视化事实集”和聚合结果，不返回原始几十万条播放日志。

建议包含：

```text
overview
song_facts[]
artist_aggregates[]
album_aggregates[]
genre_aggregates[]
rating_distribution[]
daily_play_counts[]
```

其中 `song_facts[]` 可以直接复用 Library Snapshot 的轻量字段，也可以按需返回更紧凑的数据结构。

## 5. Crossfilter + ECharts

推荐建立独立的前端 `AnalyticsDataset`：

```text
LibrarySnapshot
      │
      ▼
AnalyticsDatasetBuilder
      │
      ▼
Crossfilter Dimensions
      │
      ├── artist
      ├── album
      ├── genre
      ├── year
      ├── rating
      ├── play_count
      └── duration
      │
      ▼
Chart Adapters
      │
      ▼
ECharts
```

ECharts 只负责绘制，不负责业务过滤。Crossfilter 负责维度和聚合；页面状态负责当前筛选条件。

### 5.1 为什么这么做

用户拖动图表、切换 Genre、点 Artist、修改 Rating 时：

```text
UI event
  ↓
Crossfilter filter
  ↓
Recompute groups
  ↓
React state update
  ↓
ECharts update
```

整个过程不需要 HTTP 请求。

### 5.2 规模判断

100,000 条 Song Fact 对现代桌面浏览器是合理规模。第一版使用普通 JS object/array；只有在 profiling 证明确实存在内存或 GC 问题时，再考虑 TypedArray 或列式数据结构。

## 6. 文件扫描架构

```text
Library Scan Job
    ↓
Enumerate files
    ↓
Detect Added / Changed / Deleted
    ↓
Parse Metadata
    ↓
Resolve Artist / Album / Genre
    ↓
Update SQLite
    ↓
Refresh cached snapshot version
    ↓
Emit library.updated
```

扫描过程异步执行，不能阻塞 HTTP 请求线程。

## 7. Snapshot Version

媒体库快照建议带一个版本号：

```json
{
  "version": 184,
  "generated_at": "...",
  "songs": []
}
```

前端记录 `version`，SSE 收到 `library.updated` 后比较版本：

```text
same version → ignore
new version  → refetch snapshot
```

不要尝试让每一个文件变化都实时 patch 前端 100,000 条记录；第一版采用“事件 + 新快照重载”更简单、更可靠。

## 8. Remote Control 架构

Server 是 Player State 的权威源：

```text
Controller A ─┐
Controller B ─┼─ REST 命令 ─→ Server Player State
Controller C ─┘         │
                        │ SSE 推送
                        ▼
                 各网页 EventSource
```

播放器客户端本地使用 `HTMLAudioElement` 播放文件，Server 保存逻辑播放状态和队列。

需要区分：

- **Controller**：只发控制命令。
- **Player**：持有真实 AudioElement 并执行播放。

## 9. 一致性原则

- REST 用于 snapshot、CRUD、初始化、播放控制和非实时操作。
- SSE 用于 player state 和 library / playlist / rating 变更通知。
- Player state 的写入顺序由 server 串行化。
- 客户端收到状态后根据 `state_version` 或时间戳丢弃过期状态。

## 10. 部署

推荐初期单进程：

```text
music-server
├── HTTP API（含 SSE /api/v1/events）
├── Library Scanner
├── SQLite
└── Static Web Assets
```

部署为一个二进制 + 数据目录：

```text
app/
├── music-server
├── web/
├── music.db
└── media-cache/
```

这样最符合“个人本地媒体中心”的定位。
