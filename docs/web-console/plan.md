# Personal Music Center — 实施计划

> 本文档是 web-console 系列设计文档的**实施落地方案**，面向现有 C++ 代码库。
> 相关文档：[product.md](product.md) · [architecture.md](architecture.md) · [frontend.md](frontend.md) · [api.md](api.md) · [player.md](player.md) · [statistics.md](statistics.md) · [data-model.md](data-model.md)

## 执行状态

- **阶段 0（后端 REST API 层）**：✅ 已实现（`LocalServer/Http/ApiHandler.{hpp,cpp}`，注册于 `/api/v1`），编译 + 链接通过（`xcodebuild` Debug 构建成功）。
  - 端点：`bootstrap`、`library/snapshot`、`player/state`、`player/queue`、`player/{play|pause|next|previous|seek|volume|shuffle|repeat}`、`songs/{id}/stream`（含 HTTP Range 206）。
  - 遗留：运行态联调需在本机正常启动 App 后 `curl http://127.0.0.1:12120/api/v1/bootstrap`。（无头启动 GUI App 时 LocalServer 尚未 reachable，故未在本环境完成端到端验证。）
- 阶段 1~8：待实施（见下）。

## 1. 现状盘点

### 1.1 设计文档与现有代码的差距

| 维度 | 设计文档假设 | 现有代码事实 | 结论 |
|---|---|---|---|
| 后端语言/框架 | Rust + Axum | **C++17** 自定义 `HttpServer` + websocketpp `WebSocket`，`LocalServer/` | 不重写，在现有 C++ LocalServer 上扩展 |
| 数据库 | SQLite 归一化表（artists/albums/genres/songs） | 已有 SQLite `medialib.db`（`medialib`/`playlists` 单表，artist/genre 为 TEXT 列），`MPlayer/MediaLibrary.cpp` | 保留现有表，新增历史/队列表做增量 |
| 前端 | React + TypeScript + Vite + shadcn/ui + ECharts | `LocalServer/www` 仅有 Quasar(Vue) 脚手架配置，**无任何实际页面** | 前端从零重建（对齐文档技术栈） |
| HTTP API | REST `/api/v1/...` + OpenAPI | 只有静态文件服务（`StaticFilesHandler` 挂在 `/`），无任何 API 路由 | 新增 REST 路由层 |
| 实时事件 | WS `/ws/events` + 事件枚举 + `state_version` | 已有二进制 WS（RSA→AES），`TYPE_PLAYER_NOTIFICATION` 推状态；命名沿用旧事件 | 保留加密通道，对齐事件命名并加 `state_version` |
| 播放/队列/统计 | 服务器持 Player State、队列、统计快照 | `MPlayer` 内核已有 `g_player`、`CMediaLibrary`、`getNowPlaying`、countPlayed | 在现有内核上封装 View Model |

### 1.2 关键结论

- **不引入 Rust**：重写后端会丢弃整个既有播放器与媒体库基础设施，违背"个人项目边做边改"的定位。
- **文档是契约**：`api.md` 的端点路径、字段名、事件名作为**对外契约**；`data-model.md` 的 schema 作为**增量迁移目标**；现有内核能力对接到契约上。
- **先纵切后端，再纵切前端**：按 [ai-coding.md](../../docs/ai-coding.md) 的"一个垂直切片"原则，每个阶段都是可运行的闭环。

## 2. 总体目标架构

```text
Browser / Web App (React + Vite, 新建 src/)
├── App Shell (Sidebar / Top Bar / Mini Player)
├── TanStack Query  →  REST 快照 (bootstrap / library / statistics)
├── Zustand         →  Player state / Queue / UI
├── Crossfilter2    →  Library Snapshot 本地筛选
└── ECharts         →  Statistics 可视化
└── HTMLAudioElement (Stream via HTTP Range)
        │  REST(JSON)                     │ WS (已有加密通道)
        ▼                                 ▼
C++ LocalServer
├── Http/Server  ──静态文件注册已存在，新增注册 ApiHandler(/api/v1) 路由层──► /api/v1/*
├── WebSocket/Server（保留 RSA→AES 握手；事件命名对齐文档，增加 state_version）
└── 数据来源：MPlayer 内核 (g_player, CMediaLibrary, medialib.db)
```

### 2.1 开发路线（对齐 ai-coding.md §20）

```text
P0  阶段0  后端 REST 层脚手架 + bootstrap + player state（本计划最先执行）
P0  阶段1  Library Snapshot API + Scanner 触发
P0  阶段2  前端项目脚手架 + App Shell + 主题
P0  阶段3  Songs/Albums/Artists/Genres 页（基于 Snapshot，前端内存筛选）
P0  阶段4  Streaming + AudioEngine + Mini Player + Queue
P1  阶段5  Playlists / Rating / History
P1  阶段6  Statistics Snapshot + Crossfilter + ECharts
P1  阶段7  WebSocket Remote Control 对齐 + 事件完善
P1  阶段8  全屏 Now Playing / Search / 设置
```

每个阶段独立可运行、可验证。

## 3. 阶段 0：后端 REST API 层（本计划第一个纵切）

### 3.1 路由机制

现有 `HttpServer::Server::getRequestHandler` 用 `iStartsWith(url, getUriPath())` 前缀匹配，**先注册者优先**（[Server.cpp](../../LocalServer/Http/Server.cpp:44)）。当前仅注册了 `StaticFilesHandler("/")`。

**关键点**：`/api/v1` 处理器必须在 `/` 之前注册，否则 `/api/v1/...` 会被静态文件处理器截走（`LocalServer.cpp:110` 中构造后即注册静态处理器）。

**文件**：
- 新增 `LocalServer/Http/ApiHandler.hpp/.cpp`：实现 `IRequestHandler`，`getUriPath()` 返回 `/api/v1`；在 `onRequestHeader` 内按 `method + path + query` 分发到具体 handler 函数。
- `LocalServer/LocalServer.cpp`：在注册静态处理器**之前**插入 `m_httpServer.registerRequestHandler(make_shared<HttpServer::ApiHandler>(...))`。

### 3.2 阶段 0 端点清单（映射到现有内核）

| 文档端点 (api.md) | 实现 | 数据来源 |
|---|---|---|
| `GET /api/v1/bootstrap` | 返回 server/settings/player/queue/library 摘要 | `g_player.*` + `CMediaLibrary` 计数 |
| `GET /api/v1/library/snapshot` | 返回 {version, generated_at, artists, albums, genres, songs} | `CMediaLibrary::getAllArtist/Album/Genre` + 遍历 media；denormalized |
| `GET /api/v1/player/state` | 返回 PlayerState 结构 | `writePlayerStatus/writePlayerPosition/writePlayerSettings`（复用 [PlayerEventSender.cpp](../../LocalServer/PlayerEventSender.cpp:45) 的 JSON 字段） |
| `POST /api/v1/player/play` `pause` `next` `previous` `seek` `volume` `shuffle` `repeat` | 命令端点 | 映射到 `PlayerRemoteCtrlHandler.cpp` 已有的 `g_player.*` 调用点 |
| `GET /api/v1/player/queue` | 返回队列 item_id + song 摘要 | `g_player.getNowPlaying()` |
| `GET /api/v1/songs/{id}/stream` | **HTTP Range 206 支持** | 由 `medialib.url` 定位本地文件 |
| `GET /api/v1/songs/{id}/cover` | 封面图片 | `media.lyricsFile/cover` 或内嵌封面 |

### 3.3 PlayerState 契约（对齐 player.md §2 与 api.md §9）

```json
{
  "state": "idle|loading|playing|paused|error",
  "player_id": "1",
  "song_id": 123,
  "position": 128.4,
  "duration": 245.8,
  "volume": 0.8,
  "shuffle": true,
  "repeat": "off|one|all",
  "state_version": 901
}
```

### 3.4 需在阶段 0 新增的增量能力

- **`state_version`**：在 `LocalServer` / `PlayerEventSender` 维护一个单调递增计数器，随每个事件携带；用于前端丢弃过期状态。
- **HTTP Range 流式**：`songs/{id}/stream` 支持 `Range` / `206` / `Accept-Ranges` / `Content-Length` / MIME；浏览器 Audio 元素依赖（api.md §16）。
- **`PlaylistSong` / `play_history` 表**（data-model.md）：阶段 0 先建 schema 与迁移骨架；历史上报在阶段 5 接入。

## 4. 阶段 1：Library Snapshot 与 Scanner

- `POST /api/v1/library/scan` → 触发内核扫描（`MPlayer/MediaScanner`），异步执行，不阻塞 HTTP 线程。
- `GET /api/v1/library/scan/status` → 返回扫描状态（idle/running/finished + 版本号）。
- Snapshot 版本号：用 `scan_state.snapshot_version` 或 `medialib` 变更计数；WS 收到 `library.updated` 后前端比对版本决定是否重拉（architecture.md §7）。

## 5. 阶段 2~3：前端脚手架 + 页面（React）

按 `frontend.md` 落地：
- 目录结构 `src/app|components/ui|features|api|stores|hooks|analytics|lib`。
- `features/*` 各带 `api.ts/types.ts/hooks.ts/selectors.ts/components/utils.ts`（ai-coding.md §13）。
- App Shell 三层布局 + Mini Player（player.md §4）。
- Library Snapshot 驱动，TanStack Query 缓存，前端内存筛选/虚拟滚动。

## 6. 阶段 5：Playlists / Rating / History

- 复用现有 `playlists` 表，补齐 REST CRUD（api.md §12）。
- `PUT /songs/{id}/rating`：写 `medialib.rating`（现有字段）。
- `POST /history`（有效播放 10s/20% 后上报）+ `GET /history/recent?days=30`：新增 `play_history` 表，30 天聚合 + 清理。

## 7. 阶段 6：Statistics

- 新增 `GET /api/v1/statistics/snapshot`：后端聚合 overview/aggregates/rating_distribution/daily_play_counts/song_facts（统计职责仅限统计读取，交互过滤全在浏览器，statistics.md §8）。
- 前端 Crossfilter2 + ECharts 渲染 5 张 MVP 图（Listening Activity / Top Artists / Genre Distribution / Rating Distribution / Top Songs）。

## 8. 阶段 7：实时事件对齐

- WS 事件命名从现有 `TYPE_PLAYER_NOTIFICATION` 结构化对齐到文档枚举（`player.state_changed` 等），并在每个事件带 `state_version`。
- 前端全局单一 `WebSocketManager → EventHub → Store/QueryCache`（ai-coding.md §11）。
- 若引入 Controller/Player 区分，`/players` + 远程命令端点（api.md §10）。既有 Player Remote Ctrl 已覆盖命令侧，重点是 `player_id` 与多实例语义。

## 9. 数据模型增量（data-model.md 对现有 schema 的补充）

现有 `medialib` 已含 artist/album/genre/rating/count_played/time_played 等字段，**无需动现有表结构**。增量仅：

1. `play_history(id, song_id, played_at)` + 索引 + 30 天清理任务。
2. `player_queue(player_id, position, song_id, added_at)`（可选，阶段 4 若需跨刷新恢复）。
3. 升级检查在 `MediaLibrary::upgradeCheck()` 中追加建表（不走 destructive 迁移）。

## 10. 验证方式

- 后端：`./build.sh Release -b` 编译通过；用 `curl` 验证 `/api/v1/bootstrap`、`/snapshot`、`/player/state`、`/songs/{id}/stream` 的 Range 206。
- 前端：`pnpm typecheck && pnpm lint && pnpm test`；Playwright E2E 覆盖核心路径（ai-coding.md §14）。

## 11. 风险与注意

- **路由顺序**：`/api/v1` 处理器必须优先于 `/` 静态处理器注册（§3.1）。
- **流式安全**：`/stream` 需按 `id` 解析到 `medialib.url` 并限制在媒体根目录，防止任意文件读取。
- **同步**：`g_player` 状态在 HTTP 与 WS 线程并发访问，需顾现有锁（内核已有多数锁，新增 minimize）。
- **Release NDEBUG**：新增代码不得触碰根 CMake 的 `NDEBUG`（CLAUDE.md 约定）。