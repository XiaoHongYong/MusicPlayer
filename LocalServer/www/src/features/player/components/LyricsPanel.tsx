import { useEffect, useRef } from 'react';
import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { cn } from '@/lib/utils';
import { currentLyricsLineIndex } from '../utils';

export function LyricsPanel({ songId, position, compact = false }: { songId: number | null; position: number; compact?: boolean }) {
  const { data, isLoading } = useQuery({
    queryKey: ['lyrics', songId],
    queryFn: () => api.lyrics(songId!),
    enabled: songId != null,
  });
  const activeRef = useRef<HTMLDivElement | null>(null);
  const lines = data?.lines ?? [];
  const active = data?.synced ? currentLyricsLineIndex(lines, position) : -1;

  useEffect(() => {
    activeRef.current?.scrollIntoView({ block: 'center', behavior: 'smooth' });
  }, [active]);

  if (!songId) {
    return <Empty text="未在播放" />;
  }
  if (isLoading) {
    return <Empty text="加载歌词…" />;
  }
  if (!data?.has_lyrics) {
    return <Empty text="暂无歌词" />;
  }

  return (
    <div className={cn('h-full overflow-y-auto px-4 py-8', compact && 'py-4')}>
      {lines.length > 0 ? (
        <div className="space-y-3">
          {lines.map((line, i) => (
            <div
              key={`${i}-${line.time ?? 'x'}`}
              ref={i === active ? activeRef : undefined}
              className={cn(
                'text-center transition-all',
                i === active
                  ? 'scale-105 text-lg font-semibold text-foreground'
                  : 'text-muted-foreground',
              )}
            >
              {line.text || ' '}
            </div>
          ))}
        </div>
      ) : (
        <pre className="whitespace-pre-wrap text-sm leading-7 text-muted-foreground">{data.content}</pre>
      )}
    </div>
  );
}

function Empty({ text }: { text: string }) {
  return <div className="flex h-full items-center justify-center text-sm text-muted-foreground">{text}</div>;
}
