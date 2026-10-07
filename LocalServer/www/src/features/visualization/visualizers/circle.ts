import { smoothSpectrum } from '../smooth';
import type { AudioAnalysisFrame, Visualizer, VisualizerContext } from '../types';

/** 圆形频谱：径向柱 + bass 驱动外环 */
export class CircleVisualizer implements Visualizer {
  id = 'spectrum-circle';
  name = 'Circle';

  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private width = 0;
  private height = 0;
  private smoothed = new Float32Array(0);
  private ring = 0;

  private barColor = 'hsl(166 78% 54%)';
  private ringColor = 'hsla(280 70% 60% / 0.55)';

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

  render(frame: AudioAnalysisFrame, time: number): void {
    const ctx = this.ctx;
    if (!ctx || this.width <= 0 || this.height <= 0) return;

    const bands = frame.spectrum;
    if (this.smoothed.length !== bands.length) {
      this.smoothed = new Float32Array(bands.length);
    }
    this.smoothed = smoothSpectrum(this.smoothed, bands);
    const targetRing = 0.35 + frame.bass * 0.45 + frame.rms * 0.2;
    this.ring += (targetRing - this.ring) * 0.2;

    ctx.clearRect(0, 0, this.width, this.height);

    const cx = this.width / 2;
    const cy = this.height / 2;
    const minDim = Math.min(this.width, this.height);
    const baseR = minDim * 0.22 * (0.92 + this.ring * 0.18);
    const maxLen = minDim * 0.28;
    const n = Math.max(1, this.smoothed.length);
    const spin = time * 0.00015;

    ctx.beginPath();
    ctx.arc(cx, cy, baseR * 0.72, 0, Math.PI * 2);
    ctx.strokeStyle = this.ringColor;
    ctx.lineWidth = 2 + frame.bass * 4;
    ctx.stroke();

    for (let i = 0; i < n; i++) {
      const a0 = spin + (i / n) * Math.PI * 2;
      const a1 = spin + ((i + 0.7) / n) * Math.PI * 2;
      const len = Math.max(2, this.smoothed[i] * maxLen);
      const x0 = cx + Math.cos(a0) * baseR;
      const y0 = cy + Math.sin(a0) * baseR;
      const x1 = cx + Math.cos(a0) * (baseR + len);
      const y1 = cy + Math.sin(a0) * (baseR + len);
      const x2 = cx + Math.cos(a1) * (baseR + len);
      const y2 = cy + Math.sin(a1) * (baseR + len);
      const x3 = cx + Math.cos(a1) * baseR;
      const y3 = cy + Math.sin(a1) * baseR;

      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x3, y3);
      ctx.closePath();
      const alpha = 0.45 + this.smoothed[i] * 0.55;
      ctx.fillStyle = `hsla(166 78% 54% / ${alpha})`;
      ctx.fill();
    }

    void this.barColor;
  }

  destroy(): void {
    this.canvas = null;
    this.ctx = null;
  }
}
