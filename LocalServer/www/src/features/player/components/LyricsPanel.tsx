import { useEffect, useRef } from 'react';
import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { cn } from '@/lib/utils';
import { currentLyricsLineIndex } from '../utils';
import { useT } from '@/i18n';

export function LyricsPanel({ songId, position, compact = false }: { songId: number | null; position: number; compact?: boolean }) {
  const t = useT();
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
    return <Empty text={t('Nothing playing')} />;
  }
  if (isLoading) {
    return <Empty text={t('Loading lyrics…')} />;
  }
  if (!data?.has_lyrics) {
    return <Empty text={t('No lyrics')} />;
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
