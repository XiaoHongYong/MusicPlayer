# Personal Music Center — API 设计文档

## 1. API 原则

- Base path：`/api/v1`
- JSON API 使用 HTTP status 表达成功/失败。
- 不使用无意义的 `{code:0,data:{}}` 包装。
- OpenAPI 作为唯一 API Contract。
- TypeScript Client 从 OpenAPI 自动生成。
- 查询接口优先返回前端需要的完整 View Model，而不是强迫前端多次查询。
- Library / Statistics 使用 Snapshot API；普通 CRUD 仍使用 REST Resource API。

## 2. Bootstrap

### GET `/api/v1/bootstrap`

用途：应用启动所需的轻量状态。

```json
{
  "server": {
    "version": "1.0.0"
  },
  "settings": {},
  "player": {},
  "queue": [],
  "library": {
    "snapshot_version": 184,
    "song_count": 12580,
    "album_count": 820,
    "artist_count": 1240
  }
}
```

## 3. Library Snapshot

### GET `/api/v1/library/snapshot`

一次返回整个媒体库前端需要的数据。

建议响应：

```json
{
  "version": 184,
  "generated_at": "2026-10-05T08:30:00Z",
  "artists": [],
  "albums": [],
  "genres": [],
  "songs": []
}
```

### GET `/api/v1/library/scan/status`

返回当前扫描状态。

### POST `/api/v1/library/scan`

开始扫描。

## 4. Songs

```http
GET    /api/v1/songs/{id}
PATCH  /api/v1/songs/{id}
DELETE /api/v1/songs/{id}
GET    /api/v1/songs/{id}/stream
GET    /api/v1/songs/{id}/cover
GET    /api/v1/songs/{id}/lyrics
GET    /api/v1/songs/{id}/waveform
```

列表页面不建议依赖 `/songs?xxx` 做高频复杂筛选；列表初始数据直接来自 Library Snapshot。

Library Snapshot 中每首 `song` 带 `has_lyrics`（廉价判断：已关联歌词文件 / 同目录同名 `.lrc` / 已记录的嵌入歌词 URL）。列表用该字段展示「有歌词」标记；**是否真能打开歌词以本接口为准**。

### GET `/api/v1/songs/{id}/lyrics`

返回一首歌的歌词内容与解析行，供全屏播放页滚动高亮、媒体库查看歌词。

查找顺序：

1. 媒体库已关联的 `lyricsFile`（外部文件或 `song://…` 嵌入歌词 URL）
2. 歌曲所在目录按歌手/歌名匹配的最佳歌词文件
3. 音频文件内嵌歌词（ID3 / Vorbis / MP4 等）

```json
{
  "song_id": 123,
  "has_lyrics": true,
  "synced": true,
  "source": "/Music/Artist/Song.lrc",
  "source_type": "lrc",
  "content": "[00:12.00]hello\n[00:15.50]world\n",
  "lines": [
    { "time": 12.0, "text": "hello" },
    { "time": 15.5, "text": "world" }
  ]
}
```

字段：

| 字段 | 说明 |
|---|---|
| `has_lyrics` | 是否找到可解析歌词 |
| `synced` | 是否带时间轴（LRC / karaoke）；纯文本为 `false` |
| `source` | 歌词来源路径或嵌入 URL |
| `source_type` | `lrc` / `txt` / `embedded` |
| `content` | 原始/导出文本 |
| `lines[].time` | 行开始时间（秒）；无时间轴时为 `null` |
| `lines[].text` | 该行歌词 |

无歌词时 HTTP 仍为 200：

```json
{ "song_id": 123, "has_lyrics": false }
```

歌曲不存在时 404 `SONG_NOT_FOUND`。

## 5. Artists

```http
GET /api/v1/artists
GET /api/v1/artists/{id}
GET /api/v1/artists/{id}/albums
GET /api/v1/artists/{id}/songs
```

这些资源接口主要用于 detail page、深链接或未来的低带宽客户端。

## 6. Albums

```http
GET /api/v1/albums
GET /api/v1/albums/{id}
GET /api/v1/albums/{id}/songs
```

## 7. Genres

```http
GET /api/v1/genres
GET /api/v1/genres/{id}/songs
```

## 8. Search

### GET `/api/v1/search?q=...`

全局搜索可以由后端 FTS5 提供，因为用户输入本身需要快速定位目标资源；搜索结果只返回少量命中项。

```json
{
  "songs": [],
  "albums": [],
  "artists": [],
  "playlists": []
}
```

进入 Library 后的筛选不是 Search API 的职责，而是前端内存操作。

## 9. Player

### GET `/api/v1/player/state`

```json
{
  "state": "playing",
  "player_id": "player_1",
  "song_id": 123,
  "position": 128.4,
  "duration": 245.8,
  "volume": 0.8,
  "shuffle": true,
  "repeat": "all",
  "state_version": 901
}
```

### Commands

```http
POST /api/v1/player/play
POST /api/v1/player/pause
POST /api/v1/player/next
POST /api/v1/player/previous
POST /api/v1/player/seek
POST /api/v1/player/volume
POST /api/v1/player/shuffle
POST /api/v1/player/repeat
```

### Seek

```json
{
  "position": 125.3
}
```

### Queue

```http
GET    /api/v1/player/queue
POST   /api/v1/player/queue
DELETE /api/v1/player/queue/{item_id}
PUT    /api/v1/player/queue/order
DELETE /api/v1/player/queue
```

新增 / 替换队列：

```json
{
  "action": "insert",
  "song_ids": [123, 456, 789],
  "index": -1,
  "play": false,
  "position": "next"
}
```

- `action`：`replace` 用 `song_ids` 整表替换当前播放列表；`insert`（缺省）插入歌曲。
- `index`：`replace` 时为开播下标；`insert` 时为插入位置（`-1` 或省略为队尾）。
- `position`：`insert` 可用 `"next"`（当前曲之后）或 `"end"`（队尾），与 `index` 二选一。
- `play`：是否立即播放插入/替换后的目标曲，缺省 `false`。

## 10. Players / Remote Control

### GET `/api/v1/players`

返回可控制的 Player instances。

```json
[
  {
    "id": "macbook",
    "name": "MacBook",
    "role": "player",
    "connected": true
  },
  {
    "id": "iphone",
    "name": "iPhone",
    "role": "controller",
    "connected": true
  }
]
```

控制时带目标：

```http
POST /api/v1/players/{player_id}/commands
```

```json
{
  "type": "play"
}
```

## 11. Rating

### PUT `/api/v1/songs/{id}/rating`

```json
{
  "rating": 4.5
}
```

第一版范围限定为 `0.0 ~ 5.0`，步长 `0.5`。

## 12. Playlist

```http
GET    /api/v1/playlists
POST   /api/v1/playlists
GET    /api/v1/playlists/{id}
PATCH  /api/v1/playlists/{id}
DELETE /api/v1/playlists/{id}
POST   /api/v1/playlists/{id}/songs
DELETE /api/v1/playlists/{id}/songs/{song_id}
PUT    /api/v1/playlists/{id}/songs/order
```

## 13. History

### POST `/api/v1/history`

客户端在满足“有效播放”条件后上报一次播放记录。

```json
{
  "song_id": 123,
  "played_at": "2026-10-05T08:45:10Z"
}
```

### GET `/api/v1/history/recent?days=30`

返回后端已聚合的数据，而不是直接暴露全部原始日志。

示例：

```json
{
  "days": [
    {
      "date": "2026-10-05",
      "items": [
        {
          "song_id": 123,
          "count": 3,
          "last_played_at": "2026-10-05T08:45:10Z"
        }
      ]
    }
  ]
}
```

第一版不要为 History 增加复杂的多维筛选 API。

## 14. Statistics Snapshot

### GET `/api/v1/statistics/snapshot`

响应目标是“一次拿齐前端统计需要的数据”。

```json
{
  "generated_at": "2026-10-05T08:30:00Z",
  "overview": {},
  "song_facts": [],
  "artist_aggregates": [],
  "album_aggregates": [],
  "genre_aggregates": [],
  "rating_distribution": [],
  "daily_play_counts": []
}
```

不要添加几十个 `?artist=...&genre=...&rating=...` 的统计接口。前端交互过滤全部在 snapshot 上完成。

## 15. SSE 事件流

播放器 → 网页的单向推送。控制命令仍用 REST，不在此通道上行。

### GET `/api/v1/events`

`Accept: text/event-stream`。长连接，`Content-Type: text/event-stream`，无 `Content-Length`。注释心跳（`:` 行）保持代理与浏览器不断开。

每条 SSE 消息：

```text
id: 902
event: player.state_changed
data: {"event":"player.state_changed","state_version":902,"data":{...}}

```

- `event` 字段与 JSON 里的 `event` 相同，便于 `EventSource.addEventListener`。
- `id` 使用当前 `state_version`（库事件可用 snapshot version）。
- 连接建立后立刻推一条当前 `player.state_changed`（完整 PlayerState）作为快照。
- 浏览器 `EventSource` 断开后会自动重连；服务端不依赖 `Last-Event-ID`。

事件：

```text
player.state_changed
player.song_changed
player.queue_changed
library.scan_started
library.scan_progress
library.scan_finished
library.updated
rating.changed
playlist.updated
history.updated
```

`data` 约定：

| event | data |
|---|---|
| `player.state_changed` | 完整 PlayerState（与 `GET /player/state` 相同） |
| `player.song_changed` | 完整 PlayerState |
| `player.queue_changed` | `{ "items": [ QueueItem... ] }`（与 `GET /player/queue` 相同） |
| `library.scan_started` / `scan_progress` / `scan_finished` | `{ "state", "total", "scanned", "version" }` |
| `library.updated` | `{ "version" }`（前端比较后决定是否重拉 snapshot） |
| `rating.changed` | `{ "song_id", "rating" }` |
| `playlist.updated` | `{ "playlist_id" }`（`null` 表示列表集合变化，如新建/删除） |
| `history.updated` | `{ "song_id" }`（有效播放已写入；前端重拉 History / snapshot） |

连接建立后立刻推当前 `player.state_changed` 与 `player.queue_changed`。

**提交与推送分离：** 网页若要上报数据，只用 REST（如浏览器播放 `POST /api/v1/history`）。SSE 只做服务器 → 网页的状态通知，不上行。桌面播放器在进程内记账后推 `history.updated`，网页不要轮询 `/player/state` 再代为 POST。

事件示例：

```json
{
  "event": "player.state_changed",
  "state_version": 902,
  "data": {
    "state": "paused",
    "player_id": "player_1",
    "song_id": 123,
    "position": 141.2,
    "duration": 245.8,
    "volume": 0.8,
    "shuffle": false,
    "repeat": "all",
    "state_version": 902
  }
}
```

## 16. Media Streaming

### GET `/api/v1/songs/{id}/stream`

必须支持：

- `Range` request
- `206 Partial Content`
- 正确的 `Content-Length`
- `Accept-Ranges: bytes`
- 正确 MIME type
- Last-Modified / ETag（推荐）

浏览器原生 Audio 元素依赖这些能力实现 Seek 和渐进式播放。

## 17. Error Model

```json
{
  "error": {
    "code": "SONG_NOT_FOUND",
    "message": "Song not found"
  }
}
```

推荐错误 code：

```text
INVALID_ARGUMENT
NOT_FOUND
CONFLICT
PLAYER_NOT_CONNECTED
SONG_NOT_PLAYABLE
SCAN_RUNNING
INTERNAL_ERROR
```

## 18. API 与前端数据层约定

- Snapshot → TanStack Query cache + Zustand/derived store。
- Player state → Zustand。
- SSE `EventSource` → Event Hub → 更新对应 store/query cache。
- 所有 API TypeScript types 从 OpenAPI 生成。
