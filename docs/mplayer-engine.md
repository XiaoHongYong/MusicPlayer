# MPlayerEngine 播放器核心设计

> 本文档总结当前 `MPlayerEngine/` 的播放核结构、线程模型，以及为可视化（Visualizer）需要补上的分析接口。配套文档：[audio-analysis-stream.md](audio-analysis-stream.md)（分析从哪来、怎么传）、[web-console/audio-visualization-engine.md](web-console/audio-visualization-engine.md)（Web 侧怎么渲染）。

---

## 1. 定位

`MPlayerEngine` 只负责「把一首歌播出来」：打开文件、解码、写声卡、seek、音量。歌单、媒体库、皮肤 UI、LocalServer 都不在这里。

应用入口通过 `CPlayer`（`MPlayer/Player.cpp`）持有一个 `IPlayerCore *`，所有播放控制都走这个抽象：

```text
UI / 快捷键 / LocalServer
        │
        ▼
     CPlayer
        │
        ▼
   IPlayerCore          ← 播放核统一接口
        │
   ┌────┴────┐
   │         │
MPlayerCore  CoreAVPlayer（仅 mac 默认）
```

`IPlayerCore` 的设计意图（见 `IPlayerCore.hpp` 注释）：**与具体插件无关**，既可以用系统播放器实现，也可以用自建 Input/Decoder/Output 处理流程实现。

---

## 2. 两套播放核

| 实现 | 路径 | 平台 | 是否有 PCM | 说明 |
|---|---|---|---|---|
| **`MPlayerCore`** | `MPlayerEngine/MPlayerCore.{h,cpp}` | Win / mac /（Linux 输出未完全接好） | ✅ | 自建：Input → Decoder → Output |
| **`CoreAVPlayer`** | `MPlayerEngine/mac/CoreAVPlayer.{h,mm}` | 仅 mac | ❌ | 系统 `AVPlayer`，黑盒解码与输出 |

当前选用（`MPlayer/Player.cpp`）：

```text
macOS  → CoreAVPlayer（默认）；MPlayerCore 已实现但被注释掉
其他   → MPlayerCore
```

若 Visualizer 是重要能力，**不建议长期保持「mac 默认无分析」**。推荐 mac 默认切到（或设置项可选）`MPlayerCore`；给 `CoreAVPlayer` 做 `MTAudioProcessingTap` 可作为备选，但维护成本更高，不作 V1 主路径。详见 §5.8。

### 2.1 `IPlayerCore` 控制接口

只覆盖播放控制与状态，**当前没有** PCM / FFT / 分析相关 API（§5 为待加接口）：

| 类别 | 方法 |
|---|---|
| 生命周期 | `quit` |
| 能力说明 | `getDescription` / `getFileExtentions` / `getMediaInfo` |
| 播放控制 | `play` / `pause` / `unpause` / `stop` / `seek` / `isSeekable` |
| 状态查询 | `getDuration` / `getPos` / `getState` |
| 设置 | `setVolume` / `getVolume` / `setBalance` / `getBalance` / `setEQ` / `getEQ` |
| 回调 | `setCallback(IPlayerCoreCallback *)` → `onEndOfPlaying` / `onErrorOccured` |

### 2.2 `CoreAVPlayer`（mac 默认）

```text
文件 URL
  → AVURLAsset / AVPlayerItem
  → AVPlayer play / pause / seek
  → 系统内部解码、混音、输出
```

应用层只碰控制与 `CMTime` 进度。**没有** `AudioBufferList`、PCM 回调，也没有接 `MTAudioProcessingTap` / `AVAudioEngine`。EQ、Balance 当前未实现。

因此：在保持 `CoreAVPlayer` 的前提下，**无法从播放路径旁路做 FFT**。

### 2.3 `MPlayerCore`（自建处理流程）

平台无关的解码 + 平台相关的输出：

```text
MediaInputFile          IMediaInput：按文件读字节
      │
      ▼
MDMiniMp3 / MDFaad / MDFlac   IMediaDecoder：按扩展名选解码器
      │
      ▼
   IFBuffer / FBuffer         PCM（多为 16-bit interleaved）
      │
      ▼
CoreAudioOutput / MOSoundCard IMediaOutput：写声卡
```

| 组件 | 接口 | 主要实现 |
|---|---|---|
| Input | `IMediaInput` | `MediaInputFile` |
| Decoder | `IMediaDecoder` | `MDMiniMp3`、`MDFaad`、`MDFlac` |
| Output | `IMediaOutput` | mac:`CoreAudioOutput`，win32:`MOSoundCard` |
| PCM 缓冲 | `IFBuffer` / `FBuffer` | `bps` / `channels` / `sampleRate` + 字节 |

解码器选择在 `MPlayerCore::newMediaDecoder`（扩展名）；输出设备在 `newMediaOutput`（编译期平台宏）。历史插件式接口见 `IMPlayer.h`（含未接入现网的 `IDSP` / `IVisualizer`）。

---

## 3. `MPlayerCore` 线程模型（现状）

设计约束写在源码注释里：

> 为了简化 decoder/output 的实现，`threadRun()` 负责这些组件的状态管理、接收并处理用户命令。**整个 `MPlayerCore`（含 decoder/output）只有一条工作线程。**

这条实时路径是：

```text
decode → write
```

**不要把 FFT / Beat 等分析同步塞进这条路径。** 分析应走独立线程与有界队列（§5.2）。

### 3.1 线程划分（现状：播放）

```text
调用线程（UI / CPlayer / 其它）
  │
  │  play / pause / stop / seek …
  │  只写 _command + notify _cv
  ▼
┌─────────────────────────────────────┐
│  单一工作线程 threadRun()            │
│                                     │
│  waitForCommand → 打开 Input/Decoder│
│  循环：waitForWrite → decode → write│
│  处理 pause / seek / stop / 换歌    │
└─────────────────────────────────────┘
          │
          ▼ write(IFBuffer)
   IMediaOutput（平台实现）
          │
          ▼
   声卡 / AudioQueue / waveOut …
```

| 线程 | 做什么 | 不做什么 |
|---|---|---|
| 调用线程 | 改 `_command`、改 `_curMediaUrl` / `_seekPos`、`notify` | 不直接碰 decoder/output 的 open/decode/write |
| `threadRun` | 状态机、打开关闭解码器、decode、write、seek/flush | 不跑 UI；**不算重 FFT** |

输出层内部可能还有设备回调线程（例如 mac `AudioQueue` 缓冲完成回调递减 `_bufferCount` 并唤醒 `waitForWrite`），那是输出设备自己的事，不属于 `MPlayerCore` 状态机。

### 3.2 命令队列（极简）

没有完整命令队列，只有一个 `volatile Command _command`：

```text
CMD_NONE | CMD_PLAY | CMD_PAUSE | CMD_UNPAUSE | CMD_STOP | CMD_SEEK | CMD_QUIT | CMD_SET_OUTPUT
```

- 调用方：赋值 `_command` → `_cv.notify_one()`
- 工作线程：`waitForCommand()` 在 `_command == CMD_NONE` 时 `cv.wait`，取出后清回 `CMD_NONE`

并发约定偏简单：多数控制路径没有对 `_command` 做细粒度锁；`getPos` / `setVolume` 等对 `_output` 的访问用 `_mutex`。

### 3.3 播放循环（逻辑顺序）

```text
构造 → 启动 threadRun
  │
  ▼
停止态：waitForCommand，直到 CMD_PLAY
  │
  ▼
open Input → open Decoder → output.play()
  │
  ▼
┌─ 播放循环 ──────────────────────────┐
│  若 PAUSED 或有命令 → 处理命令       │
│  否则：                              │
│    waitForWrite(100ms)               │
│    若有新命令 → 跳出到命令处理        │
│    decode → write(PCM)               │
│    解码结束且 output 不再播放 → 结束  │
└─────────────────────────────────────┘
  │
  ▼
notifyEndOfPlaying → 回到停止态等下一首
```

启用分析时，工作线程在 `write` 前后只做**入队引用**（见 §5.2），不在此同步跑 Analyzer。

### 3.4 `CoreAVPlayer` 的线程

没有自建 decode 循环。播放由系统 `AVPlayer` 内部线程完成；应用侧主要是 ObjC 包装里的 KVO（`rate` / `status`）回调到 `notifyEndOfPlaying`。对可视化而言：**没有可旁路的 PCM 边**。

---

## 4. 历史可视化残留（现状）

`IMPlayer.h` 里仍有旧接口，现网播放核**未接入**：

```text
VisParam          512 点 uchar spectrum / waveform
IVisualizer       render(VisParam *)
IDSP              process(IFBuffer *)   // 接口在，处理流程未挂
MPlayerEngine/fft.c   固定 512 点、char 输入
```

皮肤侧 `MPSkinVis` 等仍可能引用 Visualizer 注册，但 `Player.cpp` 里相关注册已注释，`m_visParamCur` 常为空。这些**不作为**新可视化协议的基础。

---

## 5. 可视化需求：引擎侧分析接口

目标：

> 谁真正播放音频，谁的 PCM 旁路给 Analyzer；`AudioAnalysisFrame` 是**播放器领域协议**（桌面 Vis / Web / 将来其它端共用），不是 Web 专用结构。Web 传输层再编码成自己的消息格式。

### 5.1 原则

1. **实时播放路径只负责 decode → write**；分析在独立线程，避免拖慢出声。
2. **PCM 尽量共享所有权，不要为分析再拷一份整块**（`shared_ptr` / 引用计数；必要时再引入轻量 `AudioPcmBlock`）。
3. **分析队列有界**（容量需覆盖声卡预缓冲，如 ≥256）：按 **DAC 播放头**门控消费，禁止频谱超前出声；满则丢**最新**，不丢待播旧块。
4. **`IAudioAnalysisSink` 保证不在实时播放线程上调用**；网络发送、UI 更新不得堵播放线程。
5. **`AudioAnalysisFrame` 是播放器领域协议**；Web 侧另有 `WebAudioAnalysisMessage`（或等价编码），不要把内核 struct 当网络 ABI。
6. **时间轴用音频时间**：`samplePosition` + `sampleRate`；`positionSec` 可由上层换算，不要用墙钟当核心时间轴。
7. **V1 必须带 `sessionId`**：切歌后旧帧一律丢弃，避免 Web 远程控制时「歌已换、频谱还是上一首」。
8. **FFT 对 mono downmix**（`0.5L+0.5R`）；Analyzer 自维护 PCM ring buffer + hop，不按 decoder 缓冲边界各做一次 FFT。
9. **分析能力挂在 `IPlayerCore`**（V1 够用）；项目变大后再抽 `IAudioAnalysisProvider` 亦可。
10. **按需开启**：无 sink 时不入队、不算 FFT。
11. 旧 `VisParam` / `fft.c` 仅作参考。

### 5.2 目标线程与数据流

```text
                    MPlayerCore Worker（实时）
                           │
                         decode
                           │
                           ▼
                    Shared PCM Buffer
                           │
                    ┌──────┴──────┐
                    │             │
                    ▼             ▼
                  Output     Analysis Queue（有界；按播放头拉取）
                    │             │
                    ▼             ▼
                   DAC ──pos──▶ Analyzer Thread
                                  │
                           PCM Ring Buffer（连续流）
                                  │
                           mono downmix → hop → FFT / Features
                                  │
                                  ▼
                         AudioAnalysisFrame
                                  │
                           Bounded Queue（给消费者）
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
              Desktop Visualizer            CPlayer / LocalServer
                                                │
                                                ▼
                                      Web 传输编码（另协议）
                                                │
                                                ▼
                                           Web Visualizer
```

工作线程侧建议顺序（逻辑上等价即可）：

```text
decode(buf)
  → write(buf)                 // 先保证出声
  → enqueue_analysis(buf)      // 共享引用入有界队列；无 sink 则跳过
```

**不要**复制整块 PCM；队列持有 buffer 的共享所有权。若现有 `FBuffer` 不便共享，可加轻量 `AudioPcmBlock`。

分析跟随**耳放时间轴**，不是「解码有多快就算多快」。声卡可预缓冲数秒（如 mac `BUFFER_COUNT=124`）；若解码时立刻 FFT，会出现「还在听静音片头、频谱已是副歌」。因此 Analyzer 仅在 `playhead + 小铅锤 ≥ block.samplePosition` 时消费该块；队列满时丢弃尚未入队的最新块，保证待播旧块不被挤掉。

### 5.3 建议目录

```text
MPlayerEngine/
  analysis/
    AudioAnalysisFrame.h       # 播放器领域帧（非网络 ABI）
    AudioAnalysisOptions.h     # fftSize / hopSize / bandCount / outputRate
    Fft.h / Fft.cpp            # float FFT，尺寸可配
    AudioAnalyzer.h/.cpp       # ring buffer + downmix + hop + 特征 → Frame
```

### 5.4 领域协议与选项

`AudioAnalysisFrame` 是播放器内核与各类 Visualizer 之间的**领域帧**。传输到 Web 时由 LocalServer 编成独立消息（可降采样 bands、`float→u8` 等），不反绑内核布局。

```cpp
struct AudioAnalysisOptions {
    uint16_t fftSize = 2048;
    uint16_t hopSize = 1024;
    uint16_t bandCount = 64;   // 实际有效频带数；bands 数组容量可固定 128
    uint16_t outputRate = 30;  // 每秒最多产出多少帧（Hz），不要叫 targetFps
};

struct AudioAnalysisFrame {
    uint64_t sessionId;        // 每次 play / 换歌递增；消费者丢弃不匹配帧
    uint64_t sequence;         // 本 session 内递增

    uint64_t samplePosition;   // 本帧窗口对应的音频样点位置（mono 流上）
    uint32_t sampleRate;

    float rms;
    float peak;
    float bass;
    float mid;
    float treble;
    float beat;                // V1 可先恒为 0；V2 接 BeatDetector

    uint16_t bandCount;
    std::array<float, 128> bands;  // 内部容量；有效长度为 bandCount
};
```

说明：

| 字段 / 概念 | 用途 |
|---|---|
| `sessionId` | 切歌 / 新一次 `play` 时 `++`；队列与网络上残留的旧帧全部作废 |
| `sequence` | 同 session 内排序、去重 |
| `samplePosition` + `sampleRate` | 音频时间轴；`positionSec ≈ samplePosition / sampleRate` |
| `outputRate` | 上限产出频率（如 30 Hz），不是视频 FPS |
| `fftSize` / `hopSize` | 分开配置，见下节 |

不要把该 struct 的内存布局直接当 WebSocket/二进制线上格式；线上协议见 [audio-analysis-stream.md](audio-analysis-stream.md)。

### 5.5 FFT / Hop / Output Rate

三个概念分开，不要写死「Analyzer = FFT 2048」：

```text
sampleRate = 48000
fftSize    = 2048   → 窗长 ≈ 42.7 ms
hopSize    = 1024   → 步长 ≈ 21.3 ms → 理论分析更新 ≈ 46.9 Hz
outputRate = 30     → 对外最多 30 Frame/s（多余 hop 可合并或丢弃）
```

Analyzer **自己维护 PCM ring buffer**：把陆续到来的 decode 块拼成连续 mono 流，按 hop 滑窗做 FFT，而不是 `FFT(每个 decode buffer)` 一次（否则窗口绑在解码器缓冲边界上）。

立体声默认：

```text
L、R → 0.5L + 0.5R → mono → FFT
```

以后若要做立体声 Visualizer，再扩展 `leftBands` / `rightBands`，V1 不做。

### 5.6 `IPlayerCore` 扩展（V1）

V1 直接挂在 `IPlayerCore` 上即可，不必先抽 `IAudioAnalysisProvider`：

```cpp
class IAudioAnalysisSink {
public:
    virtual ~IAudioAnalysisSink() = default;

    // 保证：不在 MPlayerCore 实时播放线程（threadRun）上调用。
    // 实现方可做转发、编码、入自己的队列；勿在此做阻塞网络/重计算而不自备线程。
    virtual void onAudioAnalysisFrame(const AudioAnalysisFrame &frame) = 0;
};

class IPlayerCore {
public:
    // ... 现有控制接口 ...

    virtual bool supportsAudioAnalysis() const { return false; }

    // sink == nullptr：关闭分析（停入队、停 Analyzer 线程工作）
    virtual void setAudioAnalysisSink(IAudioAnalysisSink *sink) { (void)sink; }

    virtual void setAudioAnalysisOptions(const AudioAnalysisOptions &options) {
        (void)options;
    }
};
```

各核行为：

| 核 | `supportsAudioAnalysis` | 行为 |
|---|---|---|
| `MPlayerCore` | `true` | 有 sink 时：共享 PCM 入有界队列 → Analyzer 线程 → 再经有界队列调 sink |
| `CoreAVPlayer` | `false` | 空操作；产品上应切到 `MPlayerCore` 才能开桌面/远程 Vis |

可选后续抽象（非 V1 必做）：

```text
IPlayerCore::getAudioAnalysisProvider() → IAudioAnalysisProvider
  isSupported / subscribe / unsubscribe / setOptions
```

### 5.7 Sink 与分发

```text
Analyzer Thread
     ↓
Analysis Frame（有界队列）
     ↓
Dispatcher（仍非播放线程）
     ↓
IAudioAnalysisSink::onAudioAnalysisFrame
     ↓
Desktop Vis / CPlayer → LocalServer → Web 编码发送
```

即使 V1 为简单先把 Analyzer 算在「次要线程」里，**Sink 也绝不能直接跑在 `threadRun` 上**。否则 `websocket.send` 一类阻塞会卡住出声。

### 5.8 mac / `CoreAVPlayer` 产品决策

| 方案 | 说明 | 建议 |
|---|---|---|
| **A. mac 默认或可选 `MPlayerCore`** | 与 Win 同一套 PCM 旁路 + Analyzer | **推荐，V1 主路径** |
| B. `AVPlayer` + `MTAudioProcessingTap` | 两核都能分析，维护两套取 PCM | 备选，成本高 |
| C. 长期 mac 无 Vis | 默认播放器反而没有核心 Visualizer | **不建议** |

### 5.9 与旧接口 / Web 协议的关系

| 旧或外部 | 引擎侧 |
|---|---|
| `VisParam` 512 uchar | `AudioAnalysisFrame`（领域帧） |
| `IVisualizer::render` | 渲染在 UI/Web；引擎只产帧 |
| `IDSP::process` | 不拿来做可视化 |
| `fft.c` 512 / char | 新 `Fft`：可配尺寸 / float |
| Web `AudioAnalysisFrame`（TS） | Web 视图模型；可由领域帧映射，**不是**内核 ABI |
| 线上二进制/JSON | `WebAudioAnalysisMessage`（名称随意），含 `sessionId` 等 |

---

## 6. 实施顺序（引擎侧）

| 步骤 | 内容 |
|---|---|
| 1 | `analysis/`：`AudioAnalysisOptions` / `AudioAnalysisFrame` / `Fft` / 带 ring buffer 的 `AudioAnalyzer`（可单测） |
| 2 | `IPlayerCore`：`supportsAudioAnalysis` / `setAudioAnalysisSink` / `setAudioAnalysisOptions` |
| 3 | `MPlayerCore`：共享 PCM 入有界分析队列 + Analyzer 线程；`sessionId` 在 play/换歌时递增；无 sink 跳过 |
| 4 | mac：切到或设置项可选 `MPlayerCore` |
| 5 | Sink 适配：桌面 Vis；LocalServer 编码传输（勿把 C++ struct 当线上 ABI） |
| 6 | V2：Beat、扫描期歌曲级特征（复用 Analyzer；离线只 decode 不 write） |

---

## 7. 关键文件索引

| 路径 | 作用 |
|---|---|
| `MPlayerEngine/IPlayerCore.hpp` | 播放核统一控制接口 |
| `MPlayerEngine/IMPlayer.h` | Input / Decoder / Output / 旧 Vis·DSP |
| `MPlayerEngine/MPlayerCore.{h,cpp}` | 自建核 + 单线程状态机 |
| `MPlayerEngine/FBuffer.{hpp,cpp}` | PCM 缓冲 |
| `MPlayerEngine/MDMiniMp3.*` / `MDFaad.*` / `MDFlac.*` | 解码器 |
| `MPlayerEngine/MediaInputFile.*` | 文件输入 |
| `MPlayerEngine/mac/CoreAVPlayer.*` | mac 系统 AVPlayer 核 |
| `MPlayerEngine/mac/CoreAudioOutput.*` | mac AudioQueue 输出 |
| `MPlayerEngine/win32/MOSoundCard.*` | Win 输出 |
| `MPlayerEngine/fft.c` / `fft.h` | 旧 FFT（勿作新协议基底） |
| `MPlayer/Player.cpp` | 选用哪个 `IPlayerCore` |

---

## 相关文档

- [audio-analysis-stream.md](audio-analysis-stream.md) — 分析帧怎么传给 Web（传输层，与领域帧分离）
- [web-console/audio-visualization-engine.md](web-console/audio-visualization-engine.md) — Web Visualizer 协议与渲染
- [web-console/player.md](web-console/player.md) — Web 播放器领域
