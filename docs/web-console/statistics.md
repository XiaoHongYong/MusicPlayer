# Personal Music Center — Statistics 设计文档

## 1. 统计定位

统计不是独立的数据仓库项目，而是 Media Library 的一个分析视图。

核心策略：

> **后端负责准备一个完整但紧凑的统计 Snapshot，前端负责实时过滤、聚合和可视化。**

这样可以避免为 Dashboard 的每一个控件设计一个复杂 SQL API。

## 2. 统计数据分层

### Layer A — Library Facts

粒度：Song。

字段：

```text
song_id
artist_id
album_id
genre_id
year
duration_ms
rating
play_count
last_played_at
```

规模：最多约 100,000 条。

### Layer B — Aggregates

后端直接准备：

```text
artists
albums
genres
rating_distribution
daily_play_counts
```

### Layer C — Frontend Derived Groups

由 Crossfilter 动态得到：

```text
selected artist
selected genre
selected rating
selected year
```

下钻后的：

- Top Songs
- Top Artists
- Top Albums
- Genre share
- Rating distribution
- Year distribution

## 3. 为什么不把过滤交给 API

例如以下交互：

```text
Genre = Rock
Rating >= 4
Artist = A
Year = 2015
```

在传统服务端模式下，页面可能连续调用：

```text
/stats?genre=...
/stats?artist=...
/stats?rating=...
```

在本项目中改成：

```text
all facts loaded once
        ↓
crossfilter dimensions
        ↓
instant local aggregation
        ↓
charts update
```

优势：

- 交互延迟极低。
- API 数量少。
- 前后端职责清晰。
- AI Agent 修改页面不会频繁改 SQL。

## 4. Snapshot API

```http
GET /api/v1/statistics/snapshot
```

建议：

```json
{
  "version": 184,
  "overview": {
    "song_count": 12580,
    "album_count": 820,
    "artist_count": 1240,
    "total_duration_ms": 5823420000,
    "total_play_count": 38291
  },
  "song_facts": [],
  "artist_aggregates": [],
  "album_aggregates": [],
  "genre_aggregates": [],
  "rating_distribution": [],
  "daily_play_counts": []
}
```

## 5. 统计页面布局

### Top Summary

```text
Tracks | Albums | Artists | Total Plays
```

### Listening Activity

因为原始播放历史只保留 30 天，因此图表只展示最近 30 天：

```text
Plays
│        ▆
│  ▄ ▄   █     ▆
│  █ █ ▆ █ ▄ ▄ █
└──────────────────────
  30 days
```

### Top Artists

横向 Bar Chart，默认 Top 10。

### Genre Distribution

Donut / Bar（二选一，避免同时出现两个相同信息的图）。

### Rating Distribution

`0 ~ 5` 的分布柱状图。

### Top Songs

用榜单比图表更有效：

```text
#  Song                      Plays   Rating
1  Song A                    182     ★★★★★
2  Song B                    161     ★★★★☆
```

## 6. 交互联动

页面顶部统一 Filter Bar：

```text
[Time] [Artist] [Album] [Genre] [Rating] [Year]
```

图表上的点击也作为 filter：

```text
Click Rock bar
   ↓
filter genre=Rock
   ↓
All charts update
```

再次点击取消过滤。

## 7. 时间范围

由于原始历史只保留 30 天，统计页面的 Listening Activity 第一版只支持：

```text
7 days
30 days
```

`All time` 只能用于基于歌曲永久累计字段的指标，例如：

- play_count
- rating
- library size

## 8. 后端统计职责

后端只负责：

1. 读取 Library。
2. 统计永久累计字段。
3. 读取最近 30 天 play_history。
4. 生成紧凑 snapshot。

不负责：

- 当前浏览器筛选条件下的 Top Artist API。
- 当前 Genre + Rating 组合的临时 SQL。
- 每张 ECharts 图单独查询。

## 9. Snapshot 缓存

统计 Snapshot 可以缓存，并绑定 `library snapshot version`。

```text
library version = 184
statistics version = 184
```

Library 更新后再生成统计快照。

第一版可以简单地在扫描完成后使其失效，下次请求重新生成。

## 10. ECharts 图表清单

MVP：

- Listening Activity：Bar/Line
- Top Artists：Horizontal Bar
- Genre Distribution：Donut/Bar
- Rating Distribution：Bar
- Top Songs：List

不建议第一版加入：

- 雷达图
- 3D 图
- Sankey
- 大量动态图表

目标是“有信息价值”，不是“把页面塞满图表”。

## 11. 性能设计

100,000 Song Facts：

- Snapshot 一次加载。
- Crossfilter 保存在内存。
- 图表只对聚合后的几十/几百条数据渲染。
- Song list 使用虚拟滚动。

因此性能瓶颈通常不在 ECharts，而在过度频繁的 React render 和大 DOM 数量。

## 12. 历史页面与统计页面边界

### History

回答：

> “我最近听了什么？”

只展示 30 天列表。

### Statistics

回答：

> “我的音乐库是什么样的？”

展示：

- Library structure
- Ratings
- Lifetime play_count
- Recent 30-day activity

两者不重复建设复杂 BI。
