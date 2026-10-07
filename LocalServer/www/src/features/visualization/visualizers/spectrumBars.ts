import { smoothSpectrum } from '../smooth';
import { readVisualizerThemeColors } from '../themeColors';
import type { AudioAnalysisFrame, Visualizer, VisualizerContext } from '../types';

export class SpectrumBarsVisualizer implements Visualizer {
  id = 'spectrum-bars';
  name = 'Spectrum Bars';

  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private width = 0;
  private height = 0;
  private smoothed = new Float32Array(0);
  private peaks = new Float32Array(0);

  private barColor = 'hsl(263, 70%, 66%)';
  private peakColor = 'hsla(263, 70%, 90%, 0.92)';
  private lastTime = 0;
  // 桌面约 30Hz × 0.02/帧 → 0.6/秒；按时间衰减，避免 rAF 60Hz 掉得过快
  private static readonly PEAK_FALL_PER_SEC = 0.02 * 30;

  init(context: VisualizerContext): void {
    this.canvas = context.canvas;
    this.ctx = context.canvas.getContext('2d');
    this.lastTime = 0;
    this.resize(context.width, context.height);
  }

  resize(width: number, height: number): void {
    this.width = width;
    this.height = height;
    if (this.canvas) {
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      this.canvas.width = Math.max(1, Math.floor(width * dpr));
      this.canvas.height = Math.max(1, Math.floor(height * dpr));
      this.canvas.style.width = `${width}px`;
      this.canvas.style.height = `${height}px`;
      this.ctx?.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
  }

  render(frame: AudioAnalysisFrame, time: number): void {
    const ctx = this.ctx;
    if (!ctx || this.width <= 0 || this.height <= 0) return;

    const dtSec =
      this.lastTime > 0 ? Math.min(0.05, Math.max(0, (time - this.lastTime) / 1000)) : 1 / 60;
    this.lastTime = time;
    const peakFall = SpectrumBarsVisualizer.PEAK_FALL_PER_SEC * dtSec;

    const bands = frame.spectrum;
    // 列数按宽度自适应（与桌面 AudioVisualizer 一致）
    const gap = 2;
    const minBarWidth = 3;
    const cols = Math.max(8, Math.min(128, Math.floor((this.width + gap) / (minBarWidth + gap))));
    if (this.smoothed.length !== cols) {
      this.smoothed = new Float32Array(cols);
      this.peaks = new Float32Array(cols);
    }
    const mapped = new Float32Array(cols);
    for (let i = 0; i < cols; i++) {
      if (bands.length === 0) {
        mapped[i] = 0;
        continue;
      }
      const srcPos = (i * bands.length) / cols;
      const i0 = Math.min(bands.length - 1, Math.floor(srcPos));
      const i1 = Math.min(bands.length - 1, i0 + 1);
      const t = srcPos - i0;
      mapped[i] = bands[i0] * (1 - t) + bands[i1] * t;
    }
    this.smoothed = smoothSpectrum(this.smoothed, mapped, 0.75, 0.35);
    for (let i = 0; i < cols; i++) {
      if (this.smoothed[i] > this.peaks[i]) this.peaks[i] = this.smoothed[i];
      else this.peaks[i] = Math.max(0, this.peaks[i] - peakFall);
    }

    const theme = readVisualizerThemeColors();
    this.barColor = theme.bar;
    this.peakColor = theme.peak;

    ctx.clearRect(0, 0, this.width, this.height);

    const barW = Math.max(1, (this.width - gap * (cols - 1)) / cols);
    const peakH = Math.max(1, this.height / 24);

    for (let i = 0; i < cols; i++) {
      const x = i * (barW + gap);
      const h = Math.max(0, this.smoothed[i] * this.height);
      if (h > 0) {
        const y = this.height - Math.max(2, h);
        ctx.fillStyle = this.barColor;
        ctx.fillRect(x, y, barW, Math.max(2, h));
      }

      if (this.peaks[i] > 0.02) {
        const py = this.height - this.peaks[i] * this.height - peakH;
        ctx.fillStyle = this.peakColor;
        ctx.fillRect(x, Math.max(0, py), barW, peakH);
      }
    }
  }

  destroy(): void {
    this.canvas = null;
    this.ctx = null;
  }
}
