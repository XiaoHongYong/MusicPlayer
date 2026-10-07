# Audio Visualization Engine 设计规范

> 面向 Web Player 的「频谱分析 + 可视化特效」统一设计。核心目标：不要把它做成一个单纯的 EQ 频谱柱，而是作为播放器独立的一个 **Audio Visualization Engine**，让频谱、波形、粒子、圆形频谱、专辑封面联动等效果可以持续增加，而不会把 Player 越写越乱。
>
> 配套文档：[audio-analysis-stream.md](../audio-analysis-stream.md) 定义 C++ 播放器侧的音频分析来源与传输（谁真播放谁算 FFT）；[mplayer-engine.md](../mplayer-engine.md) 总结播放核结构、线程模型与引擎侧分析接口。

本文档定义了 `AudioAnalysisFrame`、`Visualizer`、`VisualizerPreset`、`VisualTheme` 等核心接口。这些接口是提前固化的设计规范，后续由 AI coding agent 逐个实现 `SpectrumBars → Circle → Waveform → Particles → Shader` 时都遵循这里，保证不跑偏。

---

## 1. 需求上先划分三类可视化

### A. 音频分析型

主要表达声音本身：

- FFT 频谱柱
- 平滑频谱
- 波形
- RMS 音量
- Peak 峰值
- Bass / Mid / Treble 能量
- Beat / Onset
- 频谱质心（Spectral Centroid）
- 频谱带宽（Spectral Bandwidth）
- Spectral Flux

```text
        ▂
      ▅██▅
    ▃██████▃
  ▂██████████▂
████████████████
──────────────────
  低频 → 高频
```

这是最基础、也最值得首先实现的一类。

### B. 音乐视觉化

把音频分析结果转换成视觉效果：

- 频谱柱呼吸
- 圆形频谱
- 径向波形
- Bass 驱动的粒子
- Beat 驱动的缩放
- 频谱驱动的背景光晕
- 专辑封面周围的动态光环
- 粒子爆发
- 波浪
- 星空
- 流体 / 烟雾效果

```text
             ╱╲
        ╱╲  ╱  ╲  ╱╲
      ╱   ╲      ╲   ╲
     │       ◎       │
      ╲   ╱      ╲   ╱
        ╲╱        ╲╱
```

这类效果更适合播放器的 **Fullscreen Player / Visualizer Mode**。

### C. 音乐信息型

把音乐本身的信息也放进视觉效果：

- BPM
- Key
- Loudness
- Energy
- Genre
- Album Art 主色
- 当前播放进度
- Track 结构

```text
             128 BPM

       ╭──────────────╮
       │              │
       │   ALBUM ART  │
       │              │
       ╰──────────────╯

  ▁▂▃▅██████████▅▃▂▁
        02:31 / 04:12
```

这个方向非常适合本播放器：它可以把播放器 UI 和可视化效果结合起来。

---

## 2. 推荐的技术架构

不要让 React 直接做 FFT 和动画。建议：

```text
                ┌───────────────┐
                │ Audio Element │
                └───────┬───────┘
                        │
                        ▼
                Web Audio API
                        │
             ┌──────────┴──────────┐
             │                     │
          AnalyserNode          Audio Graph
             │
             ▼
        Audio Analysis
             │
       ┌─────┼─────┐
       │     │     │
      FFT   RMS   Beat
       │     │     │
       └─────┼─────┘
             │
             ▼
     Visualization State
             │
       ┌─────┴─────┐
       │           │
    Canvas 2D    WebGL
       │           │
       ▼           ▼
   Spectrum     Particles
   Waveform     Fluid
   Bars         Shader
```

核心原则：

> **Audio 分析和 UI 渲染完全解耦。**

---

## 3. Web Audio API 是第一选择

本播放器是 Web Player，因此没必要自己实现 FFT，直接用 Web Audio API：

```ts
const audioContext = new AudioContext();

const source =
  audioContext.createMediaElementSource(audio);

const analyser = audioContext.createAnalyser();

source.connect(analyser);
analyser.connect(audioContext.destination);
```

然后：

```ts
analyser.fftSize = 2048;

const bufferLength = analyser.frequencyBinCount;
const data = new Uint8Array(bufferLength);

analyser.getByteFrequencyData(data);
```

得到：

```text
0Hz ─────────────────────── 20kHz
 │
 │ █
 │ ██
 │ ███
 │ █████
 │ █████████
 └────────────────────────────
```

这里有一个非常重要的问题需要处理。

### 不要直接把 FFT bin 映射成柱子

FFT 是线性频率分布：

```text
0
10
20
30
40
...
10000
...
20000
```

但人耳对频率的感知更接近对数尺度。因此应该转换：

```text
FFT bins
   ↓
Log frequency bands
   ↓
20Hz
30Hz
40Hz
...
100Hz
...
1kHz
...
10kHz
20kHz
```

转换后视觉效果会明显好很多。

---

## 4. 建立自己的 Audio Analysis Layer

不要让 Visualizer 直接调用 `analyser.getByteFrequencyData()`，而是做一层独立分析：

```ts
interface AudioAnalysisFrame {
  timestamp: number;

  spectrum: Float32Array;

  rms: number;
  peak: number;

  bass: number;
  mid: number;
  treble: number;

  centroid: number;
  bandwidth: number;
  flux: number;

  beat: number;
  onset: number;
}
```

这样 Visualizer 完全不关心音频分析怎么实现。例如：

```text
                AudioAnalysisFrame
                       │
       ┌───────────────┼───────────────┐
       │               │               │
   Spectrum         Energy           Beat
       │               │               │
       ▼               ▼               ▼
 SpectrumBars      Background       Particles
 Circle            Glow             Pulse
 Waveform          Album Art        Camera
```

这是整个设计里最值得提前做好的抽象。

---

## 5. FFT 不需要每一帧都做全部计算

假设渲染 60 FPS，不代表分析也必须跑 60 次 FFT。可以：

```text
Audio Analysis: 30~60 Hz
Rendering:      60 FPS
```

例如：

```text
Audio
  │
  ├── Analysis @ 30Hz
  │       │
  │       ▼
  │   AnalysisFrame
  │
  └── Render @ 60Hz
          │
          ▼
       interpolate
```

分析和渲染分成两个频率，视觉效果反而更平滑。

---

## 6. 一定要做平滑

原始 FFT 数据会非常抖，直接渲染很难看。应该做 attack / release 平滑：

```text
raw
 │
 ▼
attack / release smoothing
 │
 ▼
visual value
```

```ts
if (target > current) {
  current += (target - current) * attack;
} else {
  current += (target - current) * release;
}
```

通常 `attack` 快、`release` 慢，视觉上会产生「撑起来慢、掉下去更慢」的效果，而不是瞬间掉下去：

```text
       █
       █
       █
       █
      █
     █
    █
   █
```

---

## 7. Beat Detection 很值得做

想做真正漂亮的 Visualizer，**Beat 比 FFT 本身更重要**。

流程：

```text
Bass Energy
     │
     ▼
Low-pass
     │
     ▼
Envelope
     │
     ▼
Peak Detection
     │
     ▼
Beat
```

最终接口：

```ts
interface BeatEvent {
  timestamp: number;
  strength: number;
}
```

然后 Beat 驱动各种效果：

```text
Beat
 │
 ├── Album Art scale
 │
 ├── Background glow
 │
 ├── Particle burst
 │
 ├── Spectrum pulse
 │
 └── Camera zoom
```

例如：

```text
Normal

      [ Album ]

Beat

      [ ALBUM ]
        ↑
      scale 1.05
```

这比单纯的频谱柱有更强、更明显的「音乐感」。

---

## 8. Visualizer 采用插件式架构

这是特别建议做好的点。播放器只声明要哪个 Visualizer，不在一堆 `if/else` 里手工切换。

接口：

```ts
interface Visualizer {
  id: string;
  name: string;

  init(context: VisualizerContext): void;

  resize(width: number, height: number): void;

  render(
    frame: AudioAnalysisFrame,
    time: number
  ): void;

  destroy(): void;
}
```

目录结构：

```text
visualizers/

├── spectrum-bars
├── spectrum-circle
├── waveform
├── radial-wave
├── particles
├── album-glow
└── fluid
```

切换方式：

```ts
visualizerManager.set("spectrum-circle");
```

而不是：

```ts
if (mode === "circle") ...
else if (mode === "bars") ...
```

---

## 9. Canvas 2D vs WebGL

这个问题建议明确分层，不混在一起。

### Canvas 2D

适合：

- 频谱柱
- 波形
- 简单圆形频谱
- 简单粒子
- UI 型 Visualizer

优点：

- 简单
- AI 很容易写
- 调试简单
- 性能足够

**第一版强烈推荐 Canvas 2D。**

### WebGL

适合：

- 大量粒子
- 流体
- Shader
- Glow
- Bloom
- Noise
- 3D
- GPU particle
- 音频驱动的 shader

例如：

```text
Audio
  │
  ▼
FFT
  │
  ▼
GPU Uniform
  │
  ▼
Fragment Shader
  │
  ▼
实时视觉
```

可以做到非常漂亮的效果，但**不要第一版就上 WebGL**。

---

## 10. 专辑封面 Visualizer（核心特色）

这个应该成为本播放器的一个核心特色：封面决定视觉风格，音乐决定视觉运动。

```text
              ╭────────────╮
           ╭──┤            ├──╮
         ╱    │  ALBUM ART │    ╲
        │     │            │     │
         ╲    │            │    ╱
           ╰──┤            ├──╯
              ╰────────────╯
                 ╲██████╱
              ╱████████████╲
           ╱██████████████████╲
```

逻辑：

```text
Album Art
    │
    ├── Extract dominant colors
    │
    ▼
Color Palette
    │
    ├── Background gradient
    ├── Glow
    ├── Particle color
    └── Spectrum color

Audio
    │
    ├── Bass
    ├── Beat
    └── Spectrum
         │
         ▼
       Animation
```

---

## 11. 主题自动生成

从专辑封面提取颜色，生成视觉主题：

```text
        Album Art
           │
           ▼
    Color Extraction
           │
    ┌──────┼──────┐
    │      │      │
 primary secondary dark
    │      │      │
    └──────┼──────┘
           ▼
     Visual Theme
```

接口：

```ts
interface VisualTheme {
  primary: Color;
  secondary: Color;
  background: Color;
  glow: Color;
}
```

主题同时作用于 UI 和 Visualizer：

```text
                Album Art
                    │
        ┌───────────┴───────────┐
        ▼                       ▼
    UI Theme               Visualizer
        │                       │
    Buttons                 Spectrum
    Progress                Particles
    Background              Glow
```

这样每首歌都能自动形成自己的视觉效果。

---

## 12. 需求分成三个版本

### V1：基础能力

必须做：

- FFT
- Log frequency bands
- RMS
- Peak
- Bass / Mid / Treble
- Spectrum Bar
- Waveform
- 平滑
- 60FPS Canvas
- Visualizer 开关
- 全屏 Visualizer

```text
Audio
  ↓
FFT
  ↓
AnalysisFrame
  ↓
Canvas
  ├── Bars
  ├── Waveform
  └── Circle
```

### V2：音乐驱动

增加：

- Beat detection
- Onset
- Bass pulse
- Album color extraction
- Album glow
- Particle
- Radial visualizer
- Beat synchronization
- Visualizer presets

例如：

```text
Classic
Spectrum
Circle
Pulse
Particles
Album
```

### V3：高级视觉

最后再考虑：

- WebGL
- Shader
- GPU Particle
- Bloom
- Fluid
- Noise field
- 3D
- Audio-reactive shader
- 自定义 Visualizer
- Visualizer preset JSON

甚至可以做成可配置系统：

```json
{
  "type": "radial",
  "spectrum": "log",
  "smoothing": 0.82,
  "beat": {
    "scale": 1.08
  },
  "particles": {
    "count": 2000,
    "bassInfluence": 0.7
  }
}
```

这让 Visualizer 本身成为一个可配置系统。

---

## 13. 最终建议的目标架构

```text
                    Player
                      │
             ┌────────┴────────┐
             │                 │
        HTMLAudioElement    Player State
             │
             ▼
       Web Audio API
             │
             ▼
       Audio Analyzer
             │
             ▼
      AudioAnalysisFrame
             │
      ┌──────┼────────┐
      │      │        │
      ▼      ▼        ▼
    Player  Visualizer  Stats
    UI      Engine      Engine
             │
       ┌─────┼──────────────┐
       │     │              │
       ▼     ▼              ▼
     Canvas 2D           WebGL
       │                    │
       ├─ Spectrum          ├─ Shader
       ├─ Waveform          ├─ Fluid
       ├─ Circle            ├─ Particle
       └─ Album Glow        └─ 3D
```

一个关键设计原则：

> **`AudioAnalysisFrame` 是播放器音频层和视觉层之间唯一的核心协议。**

这样即使以后把 Web 前端换成桌面端、Canvas 换成 WebGL，甚至未来用 Rust/WGPU 实现 Visualizer，上层音频分析模型都不用重做。

---

## 待实现接口清单

以下接口是本文档的落点，也是后续 AI coding agent 逐个实现时的契约：

| 接口 | 说明 | 版本 |
| --- | --- | --- |
| [`AudioAnalysisLayer`](#4-建立自己的-audio-analysis-layer) | 从 Web Audio API 产出分析帧，屏蔽 FFT 细节 | V1 |
| `AudioAnalysisFrame` | 核心协议，音频层与视觉层之间传输的数据结构 | V1 |
| `Visualizer` | 插件的统一生命周期接口 | V1 |
| `VisualizerManager` | 按 id 切换 Visualizer，替代 if/else | V1 |
| `BeatDetector` | 输出 `BeatEvent`，驱动各种脉冲效果 | V2 |
| `ColorExtractor` | 从专辑封面提取主色/辅色/暗色 | V2 |
| `VisualTheme` | 由封面颜色生成的 UI + Visualizer 主题 | V2 |
| `VisualizerPreset` | 可配置的 Visualizer 参数（JSON） | V3 |

实现顺序建议：`SpectrumBars → Circle → Waveform → Particles → Shader`，每一档都建立在前一档稳定之上。