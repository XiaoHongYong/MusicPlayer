import { audioEngine } from '@/features/player/audio-engine';
import { emptyAnalysisFrame, type AudioAnalysisFrame } from './types';

function mapLogBands(bins: Uint8Array, sampleRate: number, bandCount: number): Float32Array {
  const out = new Float32Array(bandCount);
  const binCount = bins.length;
  if (binCount === 0 || sampleRate <= 0) return out;
  const nyquist = sampleRate * 0.5;
  const fMin = 20;
  const fMax = Math.min(18000, nyquist);
  const logMin = Math.log(fMin);
  const logMax = Math.log(fMax);
  for (let b = 0; b < bandCount; b++) {
    const t0 = b / bandCount;
    const t1 = (b + 1) / bandCount;
    const f0 = Math.exp(logMin + (logMax - logMin) * t0);
    const f1 = Math.exp(logMin + (logMax - logMin) * t1);
    const i0 = Math.min(binCount - 1, Math.floor((f0 / nyquist) * binCount));
    const i1 = Math.min(binCount, Math.ceil((f1 / nyquist) * binCount));
    let acc = 0;
    let c = 0;
    for (let i = i0; i < i1; i++) {
      acc += bins[i] / 255;
      c++;
    }
    let level = c > 0 ? acc / c : 0;
    const kLowShelfGain0 = 0.80;
    const kLowShelfGainMid = 0.80;
    // 与桌面一致：压制最低约 20% 频带
    if (bandCount > 1) {
      const t = b / (bandCount - 1);
      let shelf = 1;
      if (t < 0.1) shelf = kLowShelfGain0 + (kLowShelfGainMid - kLowShelfGain0) * (t / 0.1);
      else if (t < 0.2) shelf = kLowShelfGainMid + (1 - kLowShelfGainMid) * ((t - 0.1) / 0.1);
      level *= shelf;
    }
    out[b] = Math.min(1, Math.max(0, level));
  }
  return out;
}

/**
 * 浏览器自播分析（单例）：同一 HTMLAudioElement 只能 createMediaElementSource 一次。
 */
class WebAudioAnalyzerImpl {
  private ctx: AudioContext | null = null;
  private source: MediaElementAudioSourceNode | null = null;
  private analyser: AnalyserNode | null = null;
  private freq = new Uint8Array(0);
  private time = new Uint8Array(0);
  private bandCount = 64;
  private sessionId = 1;
  private sequence = 0;
  private refCount = 0;

  acquire(bandCount = 64) {
    this.bandCount = bandCount;
    this.refCount += 1;
    const el = audioEngine.ensureMediaElement();
    if (!this.ctx) {
      this.ctx = new AudioContext();
      this.analyser = this.ctx.createAnalyser();
      this.analyser.fftSize = 2048;
      this.analyser.smoothingTimeConstant = 0.55;
      this.freq = new Uint8Array(this.analyser.frequencyBinCount);
      this.time = new Uint8Array(this.analyser.fftSize);
      try {
        this.source = this.ctx.createMediaElementSource(el);
        this.source.connect(this.analyser);
        this.analyser.connect(this.ctx.destination);
      } catch {
        this.source = null;
      }
      this.sessionId += 1;
      this.sequence = 0;
    }
    void this.ctx.resume();
  }

  release() {
    this.refCount = Math.max(0, this.refCount - 1);
    if (this.refCount === 0) {
      void this.ctx?.suspend();
    }
  }

  sample(): AudioAnalysisFrame {
    if (!this.analyser || !this.ctx) {
      return emptyAnalysisFrame(this.bandCount);
    }
    if (this.ctx.state === 'suspended') {
      void this.ctx.resume();
    }
    this.analyser.getByteFrequencyData(this.freq);
    this.analyser.getByteTimeDomainData(this.time);

    let peak = 0;
    let rmsAcc = 0;
    for (let i = 0; i < this.time.length; i++) {
      const v = (this.time[i] - 128) / 128;
      rmsAcc += v * v;
      const a = Math.abs(v);
      if (a > peak) peak = a;
    }
    const spectrum = mapLogBands(this.freq, this.ctx.sampleRate, this.bandCount);
    const n = spectrum.length;
    const b1 = Math.max(1, Math.floor(n / 6));
    const b2 = Math.max(b1 + 1, Math.floor((n * 2) / 3));
    const avg = (from: number, to: number) => {
      let s = 0;
      let c = 0;
      for (let i = from; i < to && i < n; i++) {
        s += spectrum[i];
        c++;
      }
      return c ? s / c : 0;
    };

    const waveform = new Float32Array(Math.min(256, this.time.length));
    const step = this.time.length / waveform.length;
    for (let i = 0; i < waveform.length; i++) {
      waveform[i] = (this.time[Math.floor(i * step)] - 128) / 128;
    }

    this.sequence += 1;
    return {
      sessionId: this.sessionId,
      sequence: this.sequence,
      samplePosition: Math.floor(audioEngine.position * this.ctx.sampleRate),
      sampleRate: this.ctx.sampleRate,
      spectrum,
      waveform,
      rms: Math.sqrt(rmsAcc / Math.max(1, this.time.length)),
      peak,
      bass: avg(0, b1),
      mid: avg(b1, b2),
      treble: avg(b2, n),
      beat: 0,
      onset: 0,
      centroid: 0,
      bandwidth: 0,
      flux: 0,
      timestamp: performance.now(),
    };
  }
}

export const webAudioAnalyzer = new WebAudioAnalyzerImpl();
