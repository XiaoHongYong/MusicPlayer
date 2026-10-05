# Personal Music Center — AI Coding 规范

## 1. 目标

项目需要长期由 AI Coding Agent 参与开发，因此工程规则优先解决：

- 上下文边界清晰。
- 修改范围可控。
- 类型单一来源。
- UI 风格一致。
- Feature 间低耦合。
- 避免重复实现。

## 2. Agent 开发原则

### 2.1 先理解 Feature，再写代码

每次任务优先定位：

```text
docs/
feature/
api contract
```

不要先修改公共组件。

### 2.2 Feature-first

业务代码必须尽量放到对应 feature：

```text
features/player
features/library
features/statistics
```

公共代码只有满足“至少两个 Feature 真实复用”时才提升到 `shared`。

### 2.3 不跨层偷取状态

- React Component 不直接操作 SQL/API。
- Chart Component 不直接请求 API。
- Player UI 不直接控制 AudioElement。
- Audio Engine 不负责 React UI 状态。

## 3. API Contract

OpenAPI 是唯一来源：

```text
Rust Types
   ↓
OpenAPI
   ↓
Generated TypeScript Types
   ↓
Frontend
```

禁止手工复制 Response interface。

## 4. DB Migration

任何数据库 schema 改动必须新增 migration：

```text
migrations/
001_init.sql
002_playlist.sql
003_history.sql
...
```

不允许 AI 为了快速修测试而直接修改生产数据库 schema。

## 5. UI 规则

优先使用 shadcn/ui。

优先修改已有 component，而不是创建：

```text
BeautifulButton
PrettyButton
PrimaryActionButton
ModernButton
```

这类重复组件。

## 6. Theme 规则

业务 UI 不得硬编码颜色。

使用：

```text
background
foreground
primary
secondary
muted
accent
border
ring
```

新增颜色必须先判断是否可以使用现有 semantic token。

## 7. State 规则

### Server State

使用 TanStack Query。

### Player/UI State

使用 Zustand。

### Local Analytics

使用独立 `analytics` 模块，不把 crossfilter 对象散落在页面组件。

## 8. Library 规则

Library Snapshot 是主要数据源。

页面加载后：

```text
API → Query Cache → LibraryStore/derived data
```

筛选、排序、分页：

```text
Frontend memory
```

只有以下情况使用服务器：

- Global FTS Search。
- Detail deep-link 在 snapshot 不可用时的 fallback。
- 主动刷新 library。

## 9. Statistics 规则

Statistics Snapshot 一次加载。

所有普通 Dashboard filter：

```text
Crossfilter
```

所有 ECharts：

```text
data → chart adapter → option → ECharts
```

不允许 Chart 自己调用 API。

## 10. Player 规则

任何页面都不能自己创建第二个 AudioElement。

项目只允许一个主 Audio Engine。

```text
AudioEngine singleton
```

多个 Controller 可以连接同一个 server，但同一 Player 目标只有一个真实播放实例。

## 11. WebSocket 规则

全局只建立一个事件连接层：

```text
WebSocketManager
   ↓
EventHub
   ↓
Store / Query Cache
```

页面禁止各自建立独立 WebSocket。

## 12. 命名

推荐：

```text
SongRow
SongTable
AlbumCard
ArtistCard
PlayerBar
QueueDrawer
StatisticsOverview
RatingDistributionChart
```

不推荐：

```text
MusicItem2
NewCard
TempChart
TestPlayer
```

## 13. API 与 Feature 文件模板

```text
feature/
├── api.ts
├── types.ts
├── hooks.ts
├── selectors.ts
├── components/
│   ├── FeaturePage.tsx
│   └── FeatureCard.tsx
└── utils.ts
```

## 14. 测试规则

### Rust

每个 domain service 至少有：

- happy path
- invalid input
- persistence test

### Frontend

至少测试：

- player reducer/store
- filters/selectors
- statistics aggregations

### E2E

核心路径：

```text
open app
→ play song
→ pause
→ next
→ seek
→ rating
→ add playlist
→ remote control
```

## 15. AI 修改前后的检查

AI Agent 在修改前必须回答：

```text
1. Which feature?
2. Which contract?
3. Which state owner?
4. Which existing component can be reused?
5. Does this change DB/API contract?
```

修改后至少执行：

```text
Frontend:
  typecheck
  lint
  unit test

Backend:
  cargo fmt --check
  cargo clippy
  cargo test
```

## 16. 不允许的“捷径”

禁止：

- 用 `any` 绕过 TypeScript 错误。
- 用 `unwrap()` 处理长期运行的服务器请求路径，除非有明确不变量。
- 在组件里直接 fetch。
- 在组件里直接持有全局 WebSocket。
- 为一个小需求新建一个巨大的公共 abstraction。
- 在后端增加一个 API 只是为了支持一个前端已有数据可以完成的筛选。
- 为统计页面创建 N 个近似的 SQL endpoint。

## 17. Definition of Done

一个功能只有满足以下条件才算完成：

```text
需求实现
+ 类型正确
+ UI 与现有主题一致
+ loading/empty/error 完整
+ API contract 更新
+ 测试通过
+ docs 更新（如架构/数据/API发生变化）
```

## 18. 优先级规则

AI Agent 优先级必须遵守：

```text
P0 correctness
P1 user-visible UX
P2 performance
P3 abstraction / refactor
P4 visual polish
```

不要为了提前优化，而牺牲代码简单性。

## 19. 推荐的 AI Agent 工作单元

一次任务控制在一个明确的垂直切片，例如：

```text
“实现 Rating Feature”
```

而不是：

```text
“把整个 Library 做完”
```

垂直切片的标准：

```text
DB → API → store → UI → test
```

这样 AI 每次都能完成一个闭环。

## 20. 第一阶段 AI 实施顺序

```text
1. Project scaffold
2. Theme + App Shell
3. Database + migrations
4. Library scanner
5. Library snapshot API
6. Songs / Albums / Artists UI
7. Audio engine + Mini Player
8. Queue
9. Playlists
10. Rating
11. History
12. Statistics snapshot + Crossfilter + ECharts
13. WebSocket remote control
14. Fullscreen Player
```

这个顺序可以让每一阶段都保持可运行，而不是在最后才出现可用播放器。
