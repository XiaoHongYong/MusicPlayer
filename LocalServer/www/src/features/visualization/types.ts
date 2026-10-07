/** 与 C++ / Web Analyser 共用的领域帧（Web 视图模型）. */
export interface AudioAnalysisFrame {
  sessionId: number;
  sequence: number;
  samplePosition: number;
  sampleRate: number;
  /** 对数频带，0..1 */
  spectrum: Float32Array;
  /** 可选时域波形，约 -1..1；桌面流可能为空 */
  waveform?: Float32Array;
  rms: number;
  peak: number;
  bass: number;
  mid: number;
  treble: number;
  beat: number;
  onset: number;
  centroid: number;
  bandwidth: number;
  flux: number;
  /** 墙钟 ms，仅用于插值/丢帧；桌面帧可用 0 */
  timestamp: number;
}

export interface VisualizerContext {
  canvas: HTMLCanvasElement;
  width: number;
  height: number;
}

export interface Visualizer {
  id: string;
  name: string;
  init(context: VisualizerContext): void;
  resize(width: number, height: number): void;
  render(frame: AudioAnalysisFrame, time: number): void;
  destroy(): void;
}

export function emptyAnalysisFrame(bandCount = 64): AudioAnalysisFrame {
  return {
    sessionId: 0,
    sequence: 0,
    samplePosition: 0,
    sampleRate: 0,
    spectrum: new Float32Array(bandCount),
    rms: 0,
    peak: 0,
    bass: 0,
    mid: 0,
    treble: 0,
    beat: 0,
    onset: 0,
    centroid: 0,
    bandwidth: 0,
    flux: 0,
    timestamp: performance.now(),
  };
}
