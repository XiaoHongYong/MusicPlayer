import { smoothToward } from '../smooth';
import type { AudioAnalysisFrame, Visualizer, VisualizerContext } from '../types';

/** 波形：优先用 frame.waveform；否则用 spectrum 拼一条能量曲线 */
export class WaveformVisualizer implements Visualizer {
  id = 'waveform';
  name = 'Waveform';

  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private width = 0;
  private height = 0;
  private smoothed = new Float32Array(0);

  private stroke = 'hsl(166 78% 60%)';
  private fill = 'hsla(166 78% 54% / 0.25)';

  init(context: VisualizerContext): void {
    this.canvas = context.canvas;
    this.ctx = context.canvas.getContext('2d');
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

  private sourceSamples(frame: AudioAnalysisFrame): Float32Array {
    if (frame.waveform && frame.waveform.length > 8) {
      return frame.waveform;
    }
    // 桌面帧无 waveform 时：用频谱做对称能量波
    const bands = frame.spectrum;
    const n = Math.max(32, bands.length * 2);
    const out = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const t = i / (n - 1);
      const bi = Math.min(bands.length - 1, Math.floor(t * bands.length));
      const mag = bands[bi] ?? 0;
      const phase = Math.sin(t * Math.PI * 2 * 3 + frame.sequence * 0.15);
      out[i] = mag * phase * (0.55 + frame.rms);
    }
    return out;
  }

  render(frame: AudioAnalysisFrame, _time: number): void {
    const ctx = this.ctx;
    if (!ctx || this.width <= 0 || this.height <= 0) return;

    const src = this.sourceSamples(frame);
    if (this.smoothed.length !== src.length) {
      this.smoothed = new Float32Array(src.length);
    }
    for (let i = 0; i < src.length; i++) {
      this.smoothed[i] = smoothToward(this.smoothed[i], src[i], 0.5, 0.25);
    }

    ctx.clearRect(0, 0, this.width, this.height);
    const mid = this.height / 2;
    const amp = this.height * 0.42;

    ctx.beginPath();
    ctx.moveTo(0, mid);
    for (let i = 0; i < this.smoothed.length; i++) {
      const x = (i / Math.max(1, this.smoothed.length - 1)) * this.width;
      const y = mid - this.smoothed[i] * amp;
      ctx.lineTo(x, y);
    }
    for (let i = this.smoothed.length - 1; i >= 0; i--) {
      const x = (i / Math.max(1, this.smoothed.length - 1)) * this.width;
      const y = mid + this.smoothed[i] * amp * 0.85;
      ctx.lineTo(x, y);
    }
    ctx.closePath();
    ctx.fillStyle = this.fill;
    ctx.fill();

    ctx.beginPath();
    for (let i = 0; i < this.smoothed.length; i++) {
      const x = (i / Math.max(1, this.smoothed.length - 1)) * this.width;
      const y = mid - this.smoothed[i] * amp;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = this.stroke;
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  destroy(): void {
    this.canvas = null;
    this.ctx = null;
  }
}
