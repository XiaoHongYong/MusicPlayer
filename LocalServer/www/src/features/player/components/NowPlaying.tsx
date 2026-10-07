import { ChevronDown, Pause, Play, SkipBack, SkipForward } from 'lucide-react';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration } from '@/lib/utils';
import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from '../store';
import { LyricsPanel } from './LyricsPanel';
import { Button } from '@/components/ui/button';
import { PlaybackExtraControls } from './PlaybackExtraControls';
import { VisualizerHost } from '@/features/visualization/VisualizerHost';
import { VISUALIZER_IDS, type VisualizerId } from '@/features/visualization/registry';
import { useT } from '@/i18n';
import { cn } from '@/lib/utils';

function visualizerLabel(id: VisualizerId, t: (key: string) => string) {
  if (id === 'spectrum-bars') return t('Spectrum');
  if (id === 'spectrum-circle') return t('Circle');
  if (id === 'none') return t('Off');
  return t('Waveform');
}

export function NowPlaying() {
  const t = useT();
  const open = useUiStore((s) => s.nowPlayingOpen);
  const setOpen = useUiStore((s) => s.setNowPlayingOpen);
  const visualizerId = useUiStore((s) => s.visualizerId);
  const setVisualizerId = useUiStore((s) => s.setVisualizerId);
  const song = usePlayerStore((s) => s.current());
  const playing = usePlayerStore((s) => s.playing);
  const position = usePlayerStore((s) => s.position);
  const duration = usePlayerStore((s) => s.duration);
  const playPause = usePlayerStore((s) => s.playPause);
  const next = usePlayerStore((s) => s.next);
  const prev = usePlayerStore((s) => s.prev);
  const seek = usePlayerStore((s) => s.seek);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-40 flex flex-col bg-background">
      <div className="flex items-center justify-between px-6 py-4">
        <Button variant="ghost" onClick={() => setOpen(false)}>
          <ChevronDown className="mr-2" size={18} />
          {t('Back')}
        </Button>
        <div className="flex items-center gap-1 rounded-full border border-border bg-card/80 p-1">
          {VISUALIZER_IDS.map((id) => (
            <button
              key={id}
              type="button"
              onClick={() => setVisualizerId(id)}
              className={cn(
                'rounded-full px-3 py-1 text-xs transition-colors',
                visualizerId === id
                  ? 'bg-primary text-primary-foreground'
                  : 'text-muted-foreground hover:text-foreground',
              )}
            >
              {visualizerLabel(id, t)}
            </button>
          ))}
        </div>
      </div>
      <div className="grid min-h-0 flex-1 grid-cols-1 gap-8 px-8 pb-10 lg:grid-cols-2">
        <div className="flex flex-col items-center justify-center gap-6">
          <CoverImage
            songId={song?.id}
            className="aspect-square w-full max-w-md rounded-2xl shadow-2xl"
          />
          <div className="w-full max-w-md text-center">
            <h1 className="text-2xl font-semibold">{song?.title ?? t('Not playing')}</h1>
            <p className="mt-1 text-muted-foreground">{song?.artist}</p>
          </div>
          <div className="flex w-full max-w-md items-center gap-3 text-xs text-muted-foreground">
            <span>{formatDuration(position)}</span>
            <input
              type="range"
              min={0}
              max={Math.max(duration, 0.1)}
              step={0.1}
              value={Math.min(position, duration || 0)}
              onChange={(e) => seek(Number(e.target.value))}
              className="h-1 flex-1 accent-[hsl(var(--primary))]"
            />
            <span>{formatDuration(duration)}</span>
          </div>
          <div className="flex items-center gap-6">
            <button onClick={prev}>
              <SkipBack />
            </button>
            <button
              onClick={playPause}
              className="flex h-14 w-14 items-center justify-center rounded-full bg-primary text-primary-foreground"
            >
              {playing ? <Pause /> : <Play />}
            </button>
            <button onClick={next}>
              <SkipForward />
            </button>
          </div>
          <PlaybackExtraControls />
        </div>
        <div className="flex min-h-0 flex-col gap-4">
          <VisualizerHost
            active={open}
            visualizerId={visualizerId}
            className={cn(
              'w-full shrink-0',
              visualizerId === 'spectrum-circle' ? 'h-44' : 'h-24',
            )}
          />
          <div className="min-h-0 flex-1 rounded-2xl bg-card">
            <LyricsPanel songId={song?.id ?? null} position={position} />
          </div>
        </div>
      </div>
    </div>
  );
}
