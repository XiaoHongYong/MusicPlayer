# Personal Music Center — Player 设计文档

## 1. Player 定位

Player 是独立领域，不依赖页面生命周期。无论用户当前处于 Home、Library、Statistics 还是 Settings，播放器都持续存在。

```text
                    Player Domain
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       Player State    Queue        Audio Engine
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                     Mini Player
                         │
                         ▼
                    Full Player
```

## 2. Player State

```ts
interface PlayerState {
  state: 'idle' | 'loading' | 'playing' | 'paused' | 'error';
  songId?: number;
  position: number;
  duration: number;
  volume: number;
  shuffle: boolean;
  repeat: 'off' | 'one' | 'all';
  stateVersion: number;
}
```

## 3. Queue

Queue 和 Playlist 必须分离。

```text
Playlist = 长期保存的歌曲集合
Queue    = 当前播放上下文
```

Queue 支持：

- Play Now
- Play Next
- Add to Queue
- Remove
- Reorder
- Clear
- Shuffle

## 4. Mini Player UI

底部固定：

```text
┌──────┬──────────────────┬──────────────────────┬───────────┐
│Cover │ Song / Artist    │ ◀ ▶ ▶│ Progress │ Volume   │ Queue  │
└──────┴──────────────────┴──────────────────────┴───────────┘
```

左侧：封面、标题、Artist。

中央：Previous / Play / Next、Seek bar。

右侧：Volume、Repeat、Shuffle、Queue。

移动端压缩为：

```text
[Cover] [Title / Artist] [Play]
```

## 5. Fullscreen Player UI

```text
┌──────────────────────────────────────────────────┐
│ ← Back                                  Queue    │
│                                                  │
│ ← Back                                  Queue    │
│                                                  │
│   ┌──────────┐     上一行                         │
│   │  COVER   │     当前歌词（高亮）                 │
│   └──────────┘     下一行                         │
│   Song / Artist                                  │
│   ━━━━━○━━━━━      无歌词时显示空状态               │
│      ◀ ▶ ▶                                       │
└──────────────────────────────────────────────────┘
```

歌词数据来自 `GET /api/v1/songs/{id}/lyrics`，随 `song_id` 变化重新请求。同步滚动只使用本地 `HTMLAudioElement` 的 `currentTime`，不要为歌词向服务器轮询位置。

## 6. Audio Playback

真实播放由浏览器的 `HTMLAudioElement` 完成。

```text
Server
  ↓ HTTP Range
Audio URL
  ↓
HTMLAudioElement
```

React 不直接充当音频缓冲层。

## 7. Position 同步

不要每 100 ms 向服务器发送位置。

客户端本地更新 UI；服务器只在以下事件同步：

- play
- pause
- seek
- next
- previous
- song changed
- volume changed（可选）

WebSocket 广播状态时包含 `state_version`。

## 8. Remote Control

两个角色：

```text
Controller
  └── 发命令

Player
  └── 拥有 AudioElement
```

例如手机：

```text
Controller → Server → player command → MacBook Player
```

## 9. Player Selection

如果同时有多个 Player：

```text
Output / Player
○ MacBook
○ Desktop
○ Browser Tab
```

当前控制目标必须在 UI 中明确可见。

## 10. 播放历史触发

不要以 `play()` 事件直接记历史。

推荐客户端维护一个简单的 `playedEnough` 状态：

```text
play
 ↓
10 sec or 20% threshold
 ↓
POST /history
```

同一首歌连续暂停/继续不重复记录。

## 11. Play Next / Queue Source

从 Album 点击 Play All：

```text
queue.source = album
```

从 Playlist 播放：

```text
queue.source = playlist
```

从 Search 播放单曲：

```text
queue.source = manual
```

`source` 只是内存中的上下文，不需要第一版持久化到数据库。

## 12. Repeat / Shuffle

Repeat：

```text
off
one
all
```

Shuffle 推荐作用于 Queue 的可播放顺序，而不是永久修改 Playlist。

## 13. 后期功能

P2 再考虑：

- Gapless Playback
- Crossfade
- ReplayGain
- Karaoke 逐字高亮
- Waveform seek
- Sleep Timer
- Smart queue
