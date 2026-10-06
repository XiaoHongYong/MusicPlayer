import { Pause, Play, SkipBack, SkipForward, ListMusic, Monitor } from 'lucide-react';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '../store';
import { useUiStore } from '@/stores/ui-store';

export function MiniPlayer() {
  const song = usePlayerStore((s) => s.current());
  const playing = usePlayerStore((s) => s.playing);
  const position = usePlayerStore((s) => s.position);
  const duration = usePlayerStore((s) => s.duration);
  const playPause = usePlayerStore((s) => s.playPause);
  const next = usePlayerStore((s) => s.next);
  const prev = usePlayerStore((s) => s.prev);
  const seek = usePlayerStore((s) => s.seek);
  const setNowPlayingOpen = useUiStore((s) => s.setNowPlayingOpen);
  const setQueueOpen = useUiStore((s) => s.setQueueOpen);
  const target = useUiStore((s) => s.playbackTarget);

  return (
    <div className="flex h-20 items-center gap-4 border-t border-border bg-card px-4">
      <button className="flex min-w-0 items-center gap-3" onClick={() => setNowPlayingOpen(true)}>
        <CoverImage songId={song?.id} className="h-12 w-12 rounded-md" />
        <div className="min-w-0 text-left">
          <div className="truncate text-sm font-medium">{song?.title ?? '未播放'}</div>
          <div className="flex items-center gap-1.5 truncate text-xs text-muted-foreground">
            {target === 'desktop' && <Monitor size={11} className="shrink-0" />}
            <span className="truncate">{song?.artist ?? '—'}</span>
            {target === 'desktop' && <span className="shrink-0 text-[10px]">· 播放器</span>}
          </div>
        </div>
      </button>
      <div className="flex flex-1 flex-col items-center gap-1">
        <div className="flex items-center gap-3">
          <button onClick={prev} className="text-muted-foreground hover:text-foreground">
            <SkipBack size={18} />
          </button>
          <button
            onClick={playPause}
            className="flex h-9 w-9 items-center justify-center rounded-full bg-primary text-primary-foreground"
          >
            {playing ? <Pause size={16} /> : <Play size={16} />}
          </button>
          <button onClick={next} className="text-muted-foreground hover:text-foreground">
            <SkipForward size={18} />
          </button>
        </div>
        <div className="flex w-full max-w-xl items-center gap-2 text-[11px] text-muted-foreground">
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
      </div>
      <button className="text-muted-foreground hover:text-foreground" onClick={() => setQueueOpen(true)}>
        <ListMusic size={18} />
      </button>
    </div>
  );
}
