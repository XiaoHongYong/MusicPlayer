import { useEffect, useRef } from 'react';
import { cn } from '@/lib/utils';
import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from '@/features/player/store';
import { desktopAnalysisStream } from './desktopAnalysisStream';
import { createVisualizer, type VisualizerId } from './registry';
import { emptyAnalysisFrame, type AudioAnalysisFrame } from './types';
import { webAudioAnalyzer } from './webAudioAnalyzer';

type Props = {
  className?: string;
  /** 仅在全屏 Now Playing 等需要时启用，避免空耗 */
  active?: boolean;
  visualizerId?: VisualizerId;
};

/**
 * 谁真播放谁供帧：
 * - browser → WebAudio AnalyserNode
 * - desktop → /api/v1/audio-analysis SSE
 */
export function VisualizerHost({ className, active = true, visualizerId }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const target = useUiStore((s) => s.playbackTarget);
  const playing = usePlayerStore((s) => s.playing);
  const storeId = useUiStore((s) => s.visualizerId);
  const id = visualizerId ?? storeId;

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !active || id === 'none') return;

    const vis = createVisualizer(id);
    if (!vis) return;

    let latest: AudioAnalysisFrame = emptyAnalysisFrame(64);
    let raf = 0;
    let unsubDesktop: (() => void) | null = null;
    let ro: ResizeObserver | null = null;

    const layout = () => {
      const parent = canvas.parentElement;
      const w = parent?.clientWidth ?? canvas.clientWidth;
      const h = parent?.clientHeight ?? canvas.clientHeight;
      vis.resize(Math.max(1, w), Math.max(1, h));
    };

    vis.init({ canvas, width: 1, height: 1 });
    layout();
    ro = new ResizeObserver(layout);
    if (canvas.parentElement) ro.observe(canvas.parentElement);

    if (target === 'browser') {
      webAudioAnalyzer.acquire(64);
    } else {
      unsubDesktop = desktopAnalysisStream.subscribe((frame) => {
        latest = frame;
      });
    }

    const tick = (time: number) => {
      if (target === 'browser') {
        latest = webAudioAnalyzer.sample();
      }
      vis.render(latest, time);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);

    return () => {
      cancelAnimationFrame(raf);
      ro?.disconnect();
      unsubDesktop?.();
      if (target === 'browser') webAudioAnalyzer.release();
      vis.destroy();
    };
  }, [active, target, playing, id]);

  if (!active || id === 'none') return null;

  return (
    <div className={cn('relative overflow-hidden', className)}>
      <canvas ref={canvasRef} className="block h-full w-full" />
    </div>
  );
}
