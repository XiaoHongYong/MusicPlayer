import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from '../store';
import { formatDuration } from '@/lib/utils';

export function QueueDrawer() {
  const open = useUiStore((s) => s.queueOpen);
  const setOpen = useUiStore((s) => s.setQueueOpen);
  const queue = usePlayerStore((s) => s.queue);
  const index = usePlayerStore((s) => s.index);
  const playSongs = usePlayerStore((s) => s.playSongs);

  if (!open) return null;
  return (
    <div className="fixed inset-0 z-40 bg-black/40" onClick={() => setOpen(false)}>
      <aside
        className="absolute right-0 top-0 h-full w-full max-w-md overflow-auto bg-card p-4 shadow-xl"
        onClick={(e) => e.stopPropagation()}
      >
        <h2 className="mb-4 text-sm font-semibold">播放队列</h2>
        <div className="space-y-1">
          {queue.map((song, i) => (
            <button
              key={`${song.id}-${i}`}
              className={`flex w-full items-center justify-between rounded-md px-3 py-2 text-left text-sm ${
                i === index ? 'bg-accent' : 'hover:bg-accent/60'
              }`}
              onClick={() => playSongs(queue, i)}
            >
              <span className="truncate">
                {song.title} <span className="text-muted-foreground">· {song.artist}</span>
              </span>
              <span className="text-xs text-muted-foreground">{formatDuration(song.duration)}</span>
            </button>
          ))}
          {queue.length === 0 && <p className="text-sm text-muted-foreground">队列为空</p>}
        </div>
      </aside>
    </div>
  );
}
