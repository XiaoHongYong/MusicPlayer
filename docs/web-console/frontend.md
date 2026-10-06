# Personal Music Center — Frontend 设计文档

## 1. Frontend 技术架构

```text
React
├── Routing
├── UI Components
├── Feature Modules
├── Query Cache
├── App Stores
├── Local Analytics Engine
└── Audio Engine
```

技术：

- React + TypeScript
- Vite
- shadcn/ui
- Tailwind CSS
- Lucide
- Zustand
- TanStack Query
- crossfilter2
- ECharts
- Playwright

## 2. UI Framework 原则

首选 shadcn/ui + Tailwind，不在第一版引入庞大的全家桶 UI Framework。

优先复用：

```text
Button
Card
Dialog
Drawer
DropdownMenu
Popover
Command
Tabs
Tooltip
Slider
Select
Sheet
Table
Badge
Skeleton
Toast
```

只在产品真正产生独特交互时新增自定义组件。

## 3. 主题系统

使用 CSS Variables，不把颜色直接写死在业务组件。

```text
Theme
├── mode: light | dark | system
├── color: blue | violet | green | orange | rose | ...
└── density: comfortable | compact
```

页面组件只使用语义 token：

```text
bg-background
text-foreground
bg-card
text-muted-foreground
bg-primary
text-primary-foreground
border-border
```

不能到处出现：

```css
background: #111827
```

## 4. Layout

### Desktop

左侧 Sidebar、顶部 Header、主内容、底部 Mini Player。

### Tablet

Sidebar 可缩为 icon rail；Mini Player 保持。

### Mobile

Sidebar 改为 Drawer；底部 Mini Player 上方保留 4～5 项 Bottom Navigation。

## 5. Feature-first 目录

```text
src/
├── app/
│   ├── router.tsx
│   ├── providers.tsx
│   └── shell/
├── components/
│   └── ui/
├── features/
│   ├── home/
│   ├── library/
│   ├── player/          # Mini/Full Player、Queue、LyricsPanel
│   ├── playlists/
│   ├── history/
│   ├── statistics/
│   ├── search/
│   └── settings/
├── api/
├── stores/
├── hooks/
├── analytics/
└── lib/
```

每个 Feature 建议：

```text
feature/
├── api.ts
├── types.ts
├── hooks.ts
├── selectors.ts
├── components/
└── utils.ts
```

## 6. Server State 与 UI State

明确分工：

### TanStack Query

管理：

- Library Snapshot
- Statistics Snapshot
- Search result
- Playlist detail
- Bootstrap
- Song lyrics（按 song_id 缓存）

### Zustand

管理：

- Player state
- Queue UI
- Sidebar collapsed
- Theme / appearance
- 当前 active filters
- Dialog / drawer state

## 7. Library Data Store

库加载后进入内存：

```text
LibrarySnapshot
├── artists
├── albums
├── genres
└── songs
```

建议额外构建：

```text
SongById: Map<number, Song>
AlbumById: Map<number, Album>
ArtistById: Map<number, Artist>
```

主要页面不再重复向 API 请求列表数据。

## 8. Library Filtering

建立 `LibraryExplorer`：

```text
LibrarySnapshot
    ↓
Denormalize SongFact
    ↓
Crossfilter
    ├── artist
    ├── album
    ├── genre
    ├── year
    ├── rating
    ├── play_count
    └── duration
```

Filter state：

```ts
interface LibraryFilters {
  search?: string;
  artistId?: number;
  albumId?: number;
  genreId?: number;
  minRating?: number;
  year?: number;
}
```

搜索文本可以先使用内存索引或简单字符串匹配；只有全局搜索、跨字段模糊搜索、命中量极大时才回退到服务器 FTS。

## 9. Table Performance

100,000 songs 不应该真的渲染 100,000 DOM rows。

必须虚拟化：

```text
react-window / TanStack Virtual / equivalent
```

渲染路径：

```text
Crossfilter result IDs
        ↓
Virtualized List
        ↓
Visible SongRow only
```

## 10. Statistics Data Flow

```text
GET /statistics/snapshot
          ↓
TanStack Query
          ↓
StatisticsStore
          ↓
Crossfilter
          ↓
Derived Groups
          ↓
ECharts Adapter
          ↓
Chart
```

### 10.1 Crossfilter 设计

统计页面不应该让每张图自己保存一份过滤逻辑。

统一一个 `StatisticsContext`：

```text
filters
crossfilter instance
dimensions
groups
selection state
```

点击某一个图表时更新 Context；其它图表自动重新计算。

## 11. ECharts 设计

图表组件必须只接受“已经整理好的 chart option / series data”。

例如：

```tsx
<PlayCountByArtistChart data={data} />
```

组件内部只负责：

- tooltip
- axis
- legend
- responsive
- animation

不在 Chart component 内部直接读取 API。

## 12. Global Search

使用 shadcn `Command` / Command Palette。

快捷键：

```text
Cmd/Ctrl + K
```

显示：

```text
Songs
Albums
Artists
Playlists
Actions
```

## 13. Audio Engine

建议独立为：

```text
features/player/audio-engine.ts
```

封装：

```text
load(song)
play()
pause()
seek(position)
setVolume(value)
setSrc(url)
```

React 组件不能直接到处操作 `audio.currentTime`。

## 14. Media Session

通过 Media Session API 映射：

```text
play
pause
previoustrack
nexttrack
seekbackward
seekforward
```

同步：

- title
- artist
- album
- artwork

这样耳机、系统媒体面板等控制可以复用播放器。

## 15. Realtime Event Hub

```text
EventSource /api/v1/events
   ↓
Event parser
   ↓
Event Hub
   ├── Player Store（含本地进度插值，不轮询 position）
   ├── Library Query Cache
   ├── Playlist Query Cache
   ├── History Query Cache
   └── Toast / Notification
```

UI 不应该直接在各页面建立 EventSource。页面禁止用 `refetchInterval` / `setInterval` 拉播放器或库状态。

## 16. Loading / Empty / Error

所有页面都必须定义三态：

```text
loading
empty
error
```

并有统一 Skeleton，避免 AI 开发时每页随意设计加载态。

## 17. UI 视觉基线

目标：现代音乐应用，而不是后台管理系统。

推荐：

- 大留白
- 中等圆角
- 封面图片作为主要视觉信息
- Muted secondary text
- Hover elevation / background transition
- 少量强调色
- 图表和主按钮共享 Theme Token

避免：

- 过度渐变
- 大量边框
- 每个区域都放 Card
- 每个操作都放彩色 Button
- 复杂动画
