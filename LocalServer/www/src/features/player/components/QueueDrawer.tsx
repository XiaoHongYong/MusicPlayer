import { useMemo, useState } from 'react';
import { Search, X } from 'lucide-react';
import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from '../store';
import { formatDuration } from '@/lib/utils';
import { Input } from '@/components/ui/input';
import { songMatchesQuery } from '@/features/search/match';

export function QueueDrawer() {
  const open = useUiStore((s) => s.queueOpen);
  const setOpen = useUiStore((s) => s.setQueueOpen);
  const queue = usePlayerStore((s) => s.queue);
  const index = usePlayerStore((s) => s.index);
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [filter, setFilter] = useState('');

  const totalDuration = useMemo(
    () => queue.reduce((sum, s) => sum + (s.duration > 0 ? s.duration : 0), 0),
    [queue],
  );
  const filtered = useMemo(() => {
    return queue
      .map((song, originalIndex) => ({ song, originalIndex }))
      .filter(({ song }) => songMatchesQuery(song, filter));
  }, [queue, filter]);

  if (!open) return null;

  const filtering = filter.trim().length > 0;

  return (
    <aside className="flex w-72 shrink-0 flex-col border-l border-border bg-card">
      <div className="border-b border-border p-4">
        <div className="flex items-start justify-between gap-2">
          <div>
            <h2 className="text-sm font-semibold">播放队列</h2>
            <p className="mt-1 text-xs text-muted-foreground">
              {queue.length} 首 · {formatDuration(totalDuration)}
              {filtering ? ` · 显示 ${filtered.length} 首` : ''}
            </p>
          </div>
          <button
            className="rounded-md p-1 text-muted-foreground hover:bg-accent hover:text-foreground"
            onClick={() => setOpen(false)}
            aria-label="关闭播放队列"
          >
            <X size={16} />
          </button>
        </div>
        <div className="relative mt-3">
          <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground" />
          <Input
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            placeholder="搜索队列中的歌曲…"
            className="pl-9"
            type="search"
            autoComplete="off"
          />
        </div>
      </div>
      <div className="min-h-0 flex-1 space-y-1 overflow-auto p-3">
        {filtered.map(({ song, originalIndex }) => (
          <button
            key={`${song.id}-${originalIndex}`}
            className={`flex w-full items-center justify-between rounded-md px-3 py-2 text-left text-sm ${
              originalIndex === index ? 'bg-accent' : 'hover:bg-accent/60'
            }`}
            onClick={() => playSongs(queue, originalIndex)}
          >
            <span className="truncate">
              {song.title} <span className="text-muted-foreground">· {song.artist}</span>
            </span>
            <span className="ml-2 shrink-0 text-xs text-muted-foreground">
              {formatDuration(song.duration)}
            </span>
          </button>
        ))}
        {queue.length === 0 && <p className="text-sm text-muted-foreground">队列为空</p>}
        {queue.length > 0 && filtered.length === 0 && (
          <p className="text-sm text-muted-foreground">没有匹配的歌曲</p>
        )}
      </div>
    </aside>
  );
}
