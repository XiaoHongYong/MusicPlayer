# Personal Music Center — Web Console 文档索引

> 面向本地音乐库的 Web 管理/播放中心。后端基于现有 **C++ `LocalServer`**（`docs/web-console/plan.md` 已说明如何把设计落地到现有代码）。

## 设计文档

| 文档 | 内容 |
|---|---|
| [product.md](product.md) | 产品愿景、页面信息架构、MVP 优先级 |
| [architecture.md](architecture.md) | 总体架构、技术选型、数据加载策略、快照版本 |
| [frontend.md](frontend.md) | 前端技术栈、UI 组件、主题、Feature 目录划分 |
| [api.md](api.md) | 完整 REST + SSE 事件契约、错误模型 |
| [player.md](player.md) | 播放器领域、队列、Mini/全屏播放器、历史触发规则 |
| [statistics.md](statistics.md) | 统计分层（快照 + Crossfilter + ECharts）、图表清单 |
| [data-model.md](data-model.md) | 数据模型 / schema / 索引 / View Model |
| [metadata.md](metadata.md) | 元数据推断与整理（C++ 独立模块，标签/路径推断、垃圾过滤、置信度） |
| [audio-visualization-engine.md](audio-visualization-engine.md) | Web Visualizer 设计：`AudioAnalysisFrame` / 插件式 Visualizer / 主题 / 版本划分 |
| [audio-analysis-stream.md](../audio-analysis-stream.md) | C++ 播放器侧音频分析层：谁真播放谁算 FFT，播放级特征实时流、歌曲级特征预计算 |

## 落地文档

| 文档 | 内容 |
|---|---|
| [plan.md](plan.md) | **实施计划**：现状差距分析、分阶段落地路线、首个后端纵切细节 |

## 相关顶层文档

- [docs/design.md](../design.md) — LocalServer HTTP + SSE 通信说明。
- [docs/ai-coding.md](../ai-coding.md) — AI Agent 开发规范（本计划的执行准则）。