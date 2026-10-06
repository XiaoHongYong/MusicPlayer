import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { CoverImage } from '@/components/CoverImage';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from '@/features/library/components/MediaContextMenu';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import { usePlayerStore } from '@/features/player/store';
import { useT } from '@/i18n';

export function HistoryPage() {
  const t = useT();
  const { data, isLoading, error } = useQuery({
    queryKey: ['history-recent'],
    queryFn: () => api.recentHistory(30),
  });
  const { data: snapshot } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songMap = new Map((snapshot?.songs ?? []).map((s) => [s.id, s]));

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading history…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load history')}</p>;

  const days = data?.days ?? [];

  return (
    <div className="space-y-8 p-6">
      <div>
        <h1 className="text-2xl font-semibold">{t('History')}</h1>
        <p className="mt-1 text-sm text-muted-foreground">
          {t('Valid plays from the last 30 days, shared with the desktop app. A play is recorded after about 10 seconds or 20% progress.')}
        </p>
      </div>
      {days.length === 0 ? (
        <p className="text-sm text-muted-foreground">{t('No history yet. Play something in the browser or desktop app.')}</p>
      ) : (
        days.map((day) => (
          <section key={day.date}>
            <h2 className="mb-3 text-sm font-medium text-muted-foreground">{day.date}</h2>
            <div className="space-y-1">
              {day.items.map((item) => {
                const song = songMap.get(item.song_id);
                return (
                  <button
                    key={`${day.date}-${item.song_id}`}
                    className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
                    onClick={() => song && playSongs([song], 0)}
                    onContextMenu={(e) => song && setMenu(mediaMenuFromEvent(e, [song]))}
                    disabled={!song}
                  >
                    <CoverImage songId={song?.id} className="h-10 w-10 rounded" />
                    <div className="min-w-0 flex-1">
                      <div className="truncate text-sm">{song?.title ?? t('Song #{id}', { id: item.song_id })}</div>
                      <div className="truncate text-xs text-muted-foreground">{song?.artist ?? '—'}</div>
                    </div>
                    <span className="text-xs text-muted-foreground">{t('{n} plays', { n: item.count })}</span>
                  </button>
                );
              })}
            </div>
          </section>
        ))
      )}
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
