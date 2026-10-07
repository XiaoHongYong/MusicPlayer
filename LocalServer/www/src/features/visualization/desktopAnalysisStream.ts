import { emptyAnalysisFrame, type AudioAnalysisFrame } from './types';

type FrameHandler = (frame: AudioAnalysisFrame) => void;

type WireFrame = {
  sessionId?: number;
  sequence?: number;
  samplePosition?: number;
  sampleRate?: number;
  rms?: number;
  peak?: number;
  bass?: number;
  mid?: number;
  treble?: number;
  beat?: number;
  bands?: number[];
};

function parseWire(data: WireFrame): AudioAnalysisFrame {
  const bands = Array.isArray(data.bands) ? data.bands : [];
  const spectrum = new Float32Array(bands.length);
  for (let i = 0; i < bands.length; i++) {
    spectrum[i] = Math.min(1, Math.max(0, (bands[i] ?? 0) / 255));
  }
  return {
    sessionId: Number(data.sessionId ?? 0),
    sequence: Number(data.sequence ?? 0),
    samplePosition: Number(data.samplePosition ?? 0),
    sampleRate: Number(data.sampleRate ?? 0),
    spectrum,
    rms: Number(data.rms ?? 0),
    peak: Number(data.peak ?? 0),
    bass: Number(data.bass ?? 0),
    mid: Number(data.mid ?? 0),
    treble: Number(data.treble ?? 0),
    beat: Number(data.beat ?? 0),
    onset: 0,
    centroid: 0,
    bandwidth: 0,
    flux: 0,
    timestamp: performance.now(),
  };
}

/**
 * 桌面真播放：订阅 GET /api/v1/audio-analysis（独立 SSE）。
 * 有连接时 C++ 才会开启 Analyzer。
 */
export class DesktopAnalysisStream {
  private es: EventSource | null = null;
  private handlers = new Set<FrameHandler>();
  private latest: AudioAnalysisFrame = emptyAnalysisFrame();
  private sessionId = 0;

  getLatest() {
    return this.latest;
  }

  subscribe(fn: FrameHandler): () => void {
    this.handlers.add(fn);
    this.ensureConnected();
    return () => {
      this.handlers.delete(fn);
      if (this.handlers.size === 0) this.disconnect();
    };
  }

  private ensureConnected() {
    if (this.es) return;
    const es = new EventSource('/api/v1/audio-analysis');
    this.es = es;
    es.addEventListener('audio_analysis', (ev) => {
      try {
        const wire = JSON.parse((ev as MessageEvent).data) as WireFrame;
        const frame = parseWire(wire);
        if (this.sessionId && frame.sessionId !== this.sessionId && frame.sessionId !== 0) {
          // 新 session：接受并更新
        }
        this.sessionId = frame.sessionId;
        this.latest = frame;
        this.handlers.forEach((fn) => fn(frame));
      } catch {
        /* ignore */
      }
    });
    es.onerror = () => {
      /* EventSource 会自动重连；无订阅者时 disconnect */
    };
  }

  private disconnect() {
    if (this.es) {
      this.es.close();
      this.es = null;
    }
    this.latest = emptyAnalysisFrame();
  }
}

export const desktopAnalysisStream = new DesktopAnalysisStream();
