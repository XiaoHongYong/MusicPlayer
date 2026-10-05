# Personal Music Center — 数据模型设计

## 1. 设计原则

- 单机本地系统，优先简单可靠。
- SQLite 作为唯一持久化数据库。
- 音频二进制和数据库分离。
- ID 第一版使用 SQLite INTEGER 主键，API 直接传 number；未来有跨实例同步需求再升级为 UUID/ULID。
- 播放历史保持极简，只保存“哪首歌 + 什么时候播放”。
- Player 状态和 Queue 可持久化，但运行态保存在内存。

## 2. Entity Overview

```text
Artist ─────┐
            ├── Album ─── Song
Genre ──────┘            │
                         ├── Rating
                         └── PlayHistory

Playlist ─── PlaylistSong ─── Song

Player ─── PlayerQueue ─── Song
```

## 3. artists

```sql
CREATE TABLE artists (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  normalized_name TEXT NOT NULL UNIQUE,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

## 4. albums

```sql
CREATE TABLE albums (
  id INTEGER PRIMARY KEY,
  title TEXT NOT NULL,
  normalized_title TEXT NOT NULL,
  artist_id INTEGER,
  year INTEGER,
  cover_path TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (artist_id) REFERENCES artists(id)
);
```

## 5. genres

```sql
CREATE TABLE genres (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  normalized_name TEXT NOT NULL UNIQUE
);
```

如果一首歌曲可能属于多个 genre，可增加 `song_genres`；MVP 可以先使用 `songs.genre_id` 单值设计。

## 6. songs

```sql
CREATE TABLE songs (
  id INTEGER PRIMARY KEY,
  title TEXT NOT NULL,
  normalized_title TEXT NOT NULL,
  artist_id INTEGER,
  album_id INTEGER,
  genre_id INTEGER,
  album_artist TEXT,
  year INTEGER,
  disc_number INTEGER,
  track_number INTEGER,
  duration_ms INTEGER,
  codec TEXT,
  bitrate INTEGER,
  sample_rate INTEGER,
  channels INTEGER,
  file_path TEXT NOT NULL UNIQUE,
  file_size INTEGER NOT NULL,
  modified_at TEXT NOT NULL,
  cover_path TEXT,
  rating REAL NOT NULL DEFAULT 0,
  play_count INTEGER NOT NULL DEFAULT 0,
  last_played_at TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (artist_id) REFERENCES artists(id),
  FOREIGN KEY (album_id) REFERENCES albums(id),
  FOREIGN KEY (genre_id) REFERENCES genres(id)
);
```

### 6.1 文件变更判断

第一版使用：

```text
file_path + file_size + modified_at
```

判断文件是否变化。

无需为了每次扫描计算内容 hash。未来需要处理“文件移动后保持实体不变”时，再加入可选 content fingerprint。

## 7. ratings

如果只需要当前 rating，`songs.rating` 已经够用；为了保持模型简单，第一版不单独建 `ratings` 表。

只有后期需要评分历史时，再拆成：

```text
ratings
  id
  song_id
  rating
  rated_at
```

## 8. playlists

```sql
CREATE TABLE playlists (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  description TEXT,
  cover_song_id INTEGER,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (cover_song_id) REFERENCES songs(id)
);
```

## 9. playlist_songs

```sql
CREATE TABLE playlist_songs (
  playlist_id INTEGER NOT NULL,
  song_id INTEGER NOT NULL,
  position INTEGER NOT NULL,
  added_at TEXT NOT NULL,
  PRIMARY KEY (playlist_id, song_id),
  FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
  FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE
);
```

## 10. play_history

极简设计：

```sql
CREATE TABLE play_history (
  id INTEGER PRIMARY KEY,
  song_id INTEGER NOT NULL,
  played_at TEXT NOT NULL,
  FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE
);

CREATE INDEX idx_play_history_played_at
ON play_history(played_at DESC);

CREATE INDEX idx_play_history_song_time
ON play_history(song_id, played_at DESC);
```

不要第一版记录 duration、completion_ratio、device_id、source 等字段，除非明确发现这些数据能产生产品价值。

### 10.1 有效播放规则

建议 Audio 开始后满足以下任一条件才写历史：

- 连续播放超过约 10 秒；或
- 播放进度达到歌曲的 20% 左右。

这样可以减少用户误点、快速试听带来的噪声。

### 10.2 30 天清理

如果产品决定历史只保留 30 天，后台每天执行：

```sql
DELETE FROM play_history
WHERE played_at < datetime('now', '-30 day');
```

这样数据库天然不会无限增长。

## 11. Player State

Player 运行态主要在内存：

```text
PlayerState
├── player_id
├── state
├── song_id
├── position
├── volume
├── shuffle
├── repeat
└── state_version
```

服务器不保存毫秒级 position 更新；只在状态变更、切歌、暂停等重要事件时广播。

## 12. player_queue

如果需要刷新网页后恢复队列：

```sql
CREATE TABLE player_queue (
  player_id TEXT NOT NULL,
  position INTEGER NOT NULL,
  song_id INTEGER NOT NULL,
  added_at TEXT NOT NULL,
  PRIMARY KEY (player_id, position),
  FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE
);
```

如果第一版不需要持久化队列，可以完全放内存。

## 13. app_settings

```sql
CREATE TABLE app_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

适合保存：

```text
appearance.mode
appearance.theme
library.auto_scan
player.default_volume
player.repeat
player.shuffle
```

## 14. scan_state

```sql
CREATE TABLE scan_state (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  snapshot_version INTEGER NOT NULL DEFAULT 0,
  last_scan_started_at TEXT,
  last_scan_finished_at TEXT,
  status TEXT NOT NULL DEFAULT 'idle'
);
```

## 15. SQLite Index

MVP 至少建立：

```text
songs(artist_id)
songs(album_id)
songs(genre_id)
songs(rating)
songs(play_count DESC)
songs(last_played_at DESC)
play_history(played_at DESC)
play_history(song_id, played_at DESC)
```

FTS5：

```text
song_fts
artist_fts
album_fts
```

## 16. 前端 View Model

数据库实体不要原样暴露给所有页面。前端建议使用：

```text
SongListItem
AlbumCard
ArtistCard
PlaylistItem
StatisticsSongFact
HistoryItem
```

这样 UI 模型发生变化时不会把数据库 schema 直接绑定到页面。
