# C++ 播放器音频分析层（Audio Analysis Stream）设计

> 本文档定义 **Fourier/音频分析在“真正播放音频的一端”怎么算、算完怎么送给 Web 端做可视化**。它和 [audio-visualization-engine.md](web-console/audio-visualization-engine.md) 是配套的两篇：那一篇定 Web Visualizer 侧的协议（`AudioAnalysisFrame`、`Visualizer`、`VisualizerPreset`、`VisualTheme`），这一篇定 C++ 播放器侧的**分析来源与传输方式**。
>
> 核心原则一句话：
>
> > **谁真正播放音频，谁的 PCM 就旁路给 Audio Analyzer；Web 播放器拿到分析结果做渲染，而不是重新分析音频。**
>
> 引擎内线程模型、领域帧字段与 `IPlayerCore` 接口以 [mplayer-engine.md](mplayer-engine.md) §5 为准。本文侧重**传输**：领域帧如何编码后推给 Web。领域帧 ≠ 线上 ABI。

---

## 1. 背景：为什么要把分析放在真正的播放端

本播放器现有两层播放能力：

- **桌面端**：`MPlayerEngine/`（`mac|win32|linux` 各自解码输出），是真正拿到 PCM、真正输出声音的一端。
- **Web 端**：网页里的 `HTMLAudioElement` 通过 HTTP Range 从 `LocalServer` 流式拉文件播放（`docs/web-console/plan.md` §2 的现状）。

远程控制的逻辑关系是：

```text
Web Player (浏览器界面)
    │
    │ HTTP / WebSocket（控制 + 接收分析结果）
    ▼
播放器应用（真正播放音频）
    │
    ▼
本地音乐文件
```

因此 **FFT 最好由真正负责播放音频的那一端计算**。这一端已经有了从解码器出来的 PCM 数据，顺带做 FFT 几乎不增加 I/O 成本；与其让 Web 端重新下载、重新解码、重新 FFT，不如直接送分析结果。

但**不要**把“完整 FFT 原始数据”当高频数据整包传输。为此设计一个专门的 **Audio Analysis Stream**：播放端算好一帧一帧的 `AudioAnalysisFrame`，低延迟少量地推给 Web 端。

---

## 2. 推荐架构

播放核实时路径只做 `decode → write`；分析在独立线程（详见 [mplayer-engine.md](mplayer-engine.md) §5.2）。PCM **共享引用**入有界队列，**按声卡播放头门控消费**（容量需覆盖输出预缓冲；满则丢最新，不丢待播旧块）。

```text
                    播放器应用
                         │
                      decode
                         │
                         ▼
                  Shared PCM Buffer
                    /            \
                   ▼              ▼
              Audio Output   Analysis Queue（有界）
                   │              │
                   ▼              ▼
                  声卡       Analyzer Thread
                                  │
                           AudioAnalysisFrame（领域帧）
                                  │
                           编码为传输消息（见 §5 / §10）
                                  │
                                  ▼
                              Web Player
                                  │
                                  ▼
                             Visualizer
```

这样 Web 播放器只负责：

> **拿分析结果做视觉渲染，而不是重新分析音频。**

分析层与视觉层之间只透传 `Analysis Frame`，两端完全解耦（与 [audio-visualization-engine.md](web-console/audio-visualization-engine.md) §2 的“Audio 分析与 UI 渲染解耦”是同一个原则，这里落到 C++ 播放端）。

---

## 3. FFT 到底什么时候计算？

> 播放时实时计算为主，必要的数据可以提前预计算。不是二选一，而是分两类。

### 实时计算

适合：

- Spectrum
- RMS
- Peak
- Bass / Mid / Treble
- Beat
- Onset
- 实时波形
- 音乐驱动动画

这些数据和**当前播放位置**强相关，没有提前把整首每一帧 FFT 存下来的必要：

```text
00:31.200

PCM
 ↓
FFT
 ↓
AnalysisFrame
 ↓
传输通道
 ↓
Web Visualizer
```

### 预计算

适合：

- Waveform overview（整首波形概览）
- 整首歌曲的频谱概览
- 音乐能量曲线
- BPM
- Loudness
- Key
- ReplayGain
- 歌曲级特征
- 封面主色

这些和播放位置无关，适合在扫描音乐库时后台分析一次、写入数据库：

```text
Song
 │
 ├── metadata
 ├── duration
 ├── bpm
 ├── loudness
 ├── key
 ├── waveform overview
 └── spectrum overview

扫描音乐库
     ↓
后台分析
     ↓
写入数据库
```

---

## 4. 不要把完整 FFT 当数据库数据

估算一下“把每帧 FFT 存下来”的成本：

```text
FFT size = 2048
每秒 30 帧
一首 4 分钟
```

```text
30 × 240 = 7200 frames
每帧存 1024 个频率 bin：
7200 × 1024 × 4 bytes ≈ 28 MB / 首
一万首 ≈ 280 GB
```

完全没有必要。所以：

### 数据库只存“歌曲级特征”

```text
song_id
waveform_summary
bpm
loudness
key
dominant_color
...
```

### 播放过程中实时生成“播放级特征”

```text
FFT
RMS
Peak
Beat
...
```

实时生成后通过传输通道发给 Web，不落库。

---

## 5. 传输时也不要传 1024 个 FFT bin

Web Visualizer 通常根本用不到 1024 个 bin。在播放端内部先做一次归并再发送：

```text
PCM
 ↓
FFT 2048
 ↓
1024 bins
 ↓
Log Frequency Mapping（对数频带）
 ↓
64 / 96 / 128 bands
 ↓
Smoothing（attack/release，见 engine 文档 §6）
 ↓
AnalysisFrame
```

### 文本示例（JSON，方便读）

```json
{
  "type": "audio_analysis",
  "sessionId": 102,
  "sequence": 931,
  "samplePosition": 1440000,
  "sampleRate": 48000,
  "bands": [0.12, 0.15, 0.31, "..."],
  "rms": 0.63,
  "peak": 0.91,
  "bass": 0.72,
  "mid": 0.51,
  "treble": 0.32,
  "beat": 0.87
}
```

### 生产建议：用二进制协议，不要每次 JSON

高频约 30 Hz 下 JSON 的字节数与解析开销都偏高。建议定义**传输消息**（不要把 C++ `AudioAnalysisFrame` 内存布局当线上 ABI）：

```text
WebAudioAnalysisMessage（示意）

sessionId      u64
sequence       u64
samplePosition u64
sampleRate     u32
rms            f32
peak           f32
bass           f32
mid            f32
treble         f32
beat           f32
bandCount      u16
bands[N]       u8 或 f32   // N 由协商/选项决定，常见 64
```

`positionSec` 可由 `samplePosition / sampleRate` 在接收端换算；墙钟时间不要当核心时间轴。`sessionId` 与当前播放 session 不一致时整帧丢弃。

如果 `bands` 用 `u8` 归一化：

```text
64 bytes
+ header
≈ 100+ bytes/frame
```

即使每秒 30 帧，也只有约：

```text
≈ 3 KB/s
```

非常轻。领域帧字段定义见 [mplayer-engine.md](mplayer-engine.md) §5.4。

---

## 6. 频率分析分两层

播放端内部建议把“裸频谱”和“特征提取”分开：

```text
                    PCM
                     │
                     ▼
                  FFT 2048
                     │
              ┌──────┴──────┐
              │             │
              ▼             ▼
        Raw Spectrum    Feature Extractor
              │             │
              │        ┌────┼────┐
              │        │    │    │
              │       RMS  Beat  Energy
              │
              ▼
       Log Frequency Bands（64/96/128）
              │
              ▼
       传输通道（Audio Analysis Stream）
```

这样以后

- Web Visualizer
- Desktop Visualizer
- Mobile Visualizer
- 外部控制器

都可以消费同一个 `AudioAnalysisFrame`，播放端只产一次。

---

## 7. 播放端用什么音频库 / 解码器

关键约束：**不要为了 FFT 把歌曲再从磁盘读一遍**。

如果播放流程已经拿到 PCM：

```text
File
 ↓
Decoder
 ↓
PCM（共享引用，尽量不拷贝）
 ├──────────────→ Audio Output（真正发声的路径，实时线程）
 │
 └──────────────→ 有界 Analysis Queue → Analyzer 线程
                       （FFT / RMS / Peak / Beat）
```

几乎没有额外的 I/O 成本。输出与分析共用同一份 PCM 引用；分析线程仅在播放头临近该块时才 FFT，避免声卡大缓冲时频谱超前数秒。

> 更准确的现状与线程模型见 [mplayer-engine.md](mplayer-engine.md)：解码在平台无关的 `MD*`，输出在 `mac|win32|linux`；mac 默认整核是 `CoreAVPlayer`（无 PCM）。有 PCM 时，工作线程只把共享 PCM 送入**有界分析队列**，由 **Analyzer 线程**计算；**不要**在 `decode → write` 上同步跑 FFT。三平台共享同一 `AudioAnalyzer`。

---

## 8. 预计算的实用场景

提前计算对**非实时场景**有用，主要是音乐库扫描：

```text
Library Scan
     │
     ▼
Song Analysis
     │
     ├── Metadata
     ├── Duration
     ├── Loudness
     ├── BPM
     ├── Key
     ├── Waveform
     └── Spectrum Thumbnail
```

可生成低分辨率的整首频谱概览，例如把整首压成：

```text
1000 × 64
```

数据量很小。用途是：

- 歌曲详情页
- Waveform
- Seek bar
- 音乐分析页面
- 音频预览
- 统计页面

而不是实时 Visualizer。

---

## 9. 数据计算策略总表

| 数据                | 计算方式       | 是否持久化 |
| ----------------- | ---------- | ----- |
| Metadata          | 扫描时        | ✅     |
| Duration          | 扫描时        | ✅     |
| Loudness          | 扫描时        | ✅     |
| BPM               | 扫描时        | ✅     |
| Key               | 扫描时        | ✅     |
| Album Color       | 扫描时        | ✅     |
| Waveform Overview | 扫描时        | ✅     |
| Spectrum Overview | 扫描时        | 可选    |
| FFT Raw           | **播放实时计算** | ❌     |
| Spectrum Bands    | **播放实时计算** | ❌     |
| RMS               | **播放实时计算** | ❌     |
| Peak              | **播放实时计算** | ❌     |
| Beat              | **播放实时计算** | ❌     |
| Onset             | **播放实时计算** | ❌     |

核心原则：

> **“歌曲级特征预计算，播放级特征实时计算。”**

---

## 10. 传输协议与通道（结合本项目现状）

本项目的实时事件已用 **SSE**（`GET /api/v1/events`，见 [plan.md](web-console/plan.md) §8），播放控制走 REST，去掉了 WebSocket。设计分析流时要区分两类数据的特性：

| 数据通道 | 内容 | 更新频率 | 推荐通道 |
|---|---|---|---|
| Player State / Events | play / pause / position / volume / queue / song_changed / seek | 低频（状态变化才推） | **SSE**（沿用现状） |
| Audio Analysis | spectrum / rms / beat / energy | **高频（30fps 二进制帧）** | **独立低延迟通道** |

两类数据**在协议层分开**，因为 Audio Analysis 的更新频率远高于播放器状态，混在一起会被高频帧拖垮。

如果不希望音视频分析牵动现有 SSE/HTTP 结构，也可以拆两个端点：

```text
/ws/player
/ws/audio-analysis
```

这样 Web 页面只在需要时才连分析通道：普通播放器页面只连 `/ws/player`；打开 Fullscreen Visualizer 时才连 `/ws/audio-analysis`。避免用户只是听歌、没开频谱时，播放端和 Web 端还持续传高频分析数据。

> 说明：SSE 是文本事件流，长期跑 30fps 的二进制分析帧性价比不高；分析帧建议复用现有连接框架做一条二进制低延迟流，或按上面拆独立通道。跑谁心中以“需要时才开采”为准，常态听歌不开。

---

## 11. 与当前双播放路径的结合（关键决策）

上面是“播放端真播放、分析端随播放产出”的通用设计。但本项目的 Web 端**也可能自己承担播放**（`HTMLAudioElement` 流式拉文件）。因此要明确一条规则，避免两处都分析、或取错数据：

```text
哪个端真正输出声音，就由哪个端算 FFT。
```

- **桌面端在放（桌面 App 就是真播放器）**：由 `MPlayerEngine` 的 PCM 旁路计算 FFT → 通过 §10 的通道把 `AudioAnalysisFrame` 推给 Web/全屏 Visualizer。这对应本文档的架构。
- **Web 端自播（浏览器 `HTMLAudioElement` 在放）**：真正的 PCM 在浏览器里，由浏览器用 Web Audio `AnalyserNode` 计算分析，走 [audio-visualization-engine.md](web-console/audio-visualization-engine.md) 的 Web 侧设计。这对应那一篇的架构。

注意桌面端向 Web 推分析帧时，时间轴以**桌面播放端**的 `samplePosition` / `sampleRate` 为准；每帧带 `sessionId`，切歌后旧帧丢弃。与浏览器内 `currentTime` 对得上（如需跨端联动）是后续可选工作，V1 不必做。

---

## 12. 实施建议与接口清单

沿用 [audio-visualization-engine.md](web-console/audio-visualization-engine.md) 的版本划分，本文档的 C++ 侧同样分三步：

| 接口/能力 | 说明 | 版本 |
|---|---|---|
| 领域 `AudioAnalysisFrame` + `sessionId` | 播放器领域协议；非线上 ABI。字段见 mplayer-engine.md §5.4 | V1 |
| 有界 PCM 队列 + Analyzer 线程 | 不阻塞 `decode → write`；按播放头门控；满则丢最新 | V1 |
| ring buffer + hop + mono FFT → log bands | `fftSize`/`hopSize`/`outputRate` 可配 | V1 |
| 传输消息编码 | 低频 SSE 与高频二进制流分离；含 `sessionId`；按需开启 | V1 |
| `BeatDetector` | 输出 `BeatEvent`（Bass → 低通 → 包络 → 峰值检测），V2 音乐驱动 | V2 |
| 歌曲级特征预计算 | 扫描时写入 waveform/bpm/loudness/key/主色等，落库 | V2 |

实现顺序与引擎侧步骤以 [mplayer-engine.md](mplayer-engine.md) §6 为准：先跑通 **PCM 共享旁路 → Analyzer 线程 → 领域帧 → 编码推送 Web**，再补 Beat、再补扫描期预计算。

---

## 相关文档

- [mplayer-engine.md](mplayer-engine.md) — `MPlayerEngine` 播放核结构、线程模型，以及引擎侧可视化分析接口。
- [audio-visualization-engine.md](web-console/audio-visualization-engine.md) — Web Visualizer 侧设计：`AudioAnalysisFrame` / `Visualizer` / `VisualizerPreset` / `VisualTheme` 与版本划分。
- [player.md](web-console/player.md) — Web 播放器领域（队列、Mini/全屏播放器）。
- [plan.md](web-console/plan.md) — web-console 实施计划与现有 C++ `LocalServer` 实施方式。