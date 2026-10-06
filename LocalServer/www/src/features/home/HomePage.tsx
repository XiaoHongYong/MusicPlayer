import { Link } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import { ListMusic } from 'lucide-react';
import { api } from '@/api/client';
import type { Song } from '@/api/types';
import { CoverImage } from '@/components/CoverImage';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import { usePlayerStore } from '@/features/player/store';
import { formatDuration } from '@/lib/utils';
import { useT } from '@/i18n';

export function HomePage() {
  const t = useT();
  const { data: boot } = useQuery({ queryKey: ['bootstrap'], queryFn: api.bootstrap });
  const { data: snapshot } = useLibrarySnapshot();
  const { data: history } = useQuery({
    queryKey: ['history-recent'],
    queryFn: () => api.recentHistory(30),
  });
  const { data: playlists = [] } = useQuery({ queryKey: ['playlists'], queryFn: api.playlists });
  const playSongs = usePlayerStore((s) => s.playSongs);
  const songs = snapshot?.songs ?? [];
  const songMap = new Map(songs.map((s) => [s.id, s]));

  const recentPlayed: Song[] = [];
  const seen = new Set<number>();
  for (const day of history?.days ?? []) {
    for (const item of day.items) {
      if (seen.has(item.song_id)) continue;
      const song = songMap.get(item.song_id);
      if (!song) continue;
      seen.add(item.song_id);
      recentPlayed.push(song);
      if (recentPlayed.length >= 8) break;
    }
    if (recentPlayed.length >= 8) break;
  }

  const recentAdded = [...songs].sort((a, b) => b.timeAdded - a.timeAdded).slice(0, 8);
  const topRated = [...songs]
    .filter((s) => s.rating >= 4)
    .sort((a, b) => b.rating - a.rating || b.play_count - a.play_count)
    .slice(0, 8);
  const mostPlayed = [...songs].sort((a, b) => b.play_count - a.play_count).slice(0, 8);
  const recentPlaylists = [...playlists].sort((a, b) => b.time_modified - a.time_modified).slice(0, 6);

  return (
    <div className="space-y-10 p-6">
      <p className="text-muted-foreground">
        {t('{songs} songs · {albums} albums', {
          songs: boot?.library.song_count ?? '—',
          albums: boot?.library.album_count ?? '—',
        })}
      </p>

      {recentPlayed.length > 0 && (
        <section>
          <div className="mb-4 flex items-end justify-between">
            <h2 className="text-lg font-medium">{t('Recently played')}</h2>
            <Link to="/history" className="text-sm text-muted-foreground hover:text-foreground">
              {t('See all')}
            </Link>
          </div>
          <div className="grid grid-cols-2 gap-4 md:grid-cols-4">
            {recentPlayed.map((s, i) => (
              <button key={s.id} className="text-left" onClick={() => playSongs(recentPlayed, i)}>
                <CoverImage songId={s.id} className="aspect-square w-full rounded-lg" />
                <div className="mt-2 truncate text-sm">{s.title}</div>
                <div className="truncate text-xs text-muted-foreground">{s.artist}</div>
              </button>
            ))}
          </div>
        </section>
      )}

      <section>
        <h2 className="mb-4 text-lg font-medium">{t('Recently added')}</h2>
        <div className="grid grid-cols-2 gap-4 md:grid-cols-4">
          {recentAdded.map((s) => (
            <button key={s.id} className="text-left" onClick={() => playSongs(recentAdded, recentAdded.indexOf(s))}>
              <CoverImage songId={s.id} className="aspect-square w-full rounded-lg" />
              <div className="mt-2 truncate text-sm">{s.title}</div>
              <div className="truncate text-xs text-muted-foreground">{s.artist}</div>
            </button>
          ))}
        </div>
      </section>

      <section>
        <h2 className="mb-4 text-lg font-medium">{t('Top rated')}</h2>
        {topRated.length === 0 ? (
          <p className="text-sm text-muted-foreground">{t('No songs rated 4 stars or higher yet.')}</p>
        ) : (
          <div className="grid grid-cols-2 gap-4 md:grid-cols-4">
            {topRated.map((s, i) => (
              <button key={s.id} className="text-left" onClick={() => playSongs(topRated, i)}>
                <CoverImage songId={s.id} className="aspect-square w-full rounded-lg" />
                <div className="mt-2 truncate text-sm">{s.title}</div>
                <div className="truncate text-xs text-muted-foreground">
                  {t('{artist} · {rating} stars', { artist: s.artist, rating: s.rating })}
                </div>
              </button>
            ))}
          </div>
        )}
      </section>

      <section>
        <h2 className="mb-4 text-lg font-medium">{t('Most played')}</h2>
        <div className="space-y-1">
          {mostPlayed.map((s, i) => (
            <button
              key={s.id}
              className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
              onClick={() => playSongs(mostPlayed, i)}
            >
              <span className="w-6 text-xs text-muted-foreground">{i + 1}</span>
              <CoverImage songId={s.id} className="h-10 w-10 rounded" />
              <div className="min-w-0 flex-1">
                <div className="truncate text-sm">{s.title}</div>
                <div className="truncate text-xs text-muted-foreground">{s.artist}</div>
              </div>
              <span className="text-xs text-muted-foreground">{t('{n} plays', { n: s.play_count })}</span>
            </button>
          ))}
        </div>
      </section>

      {recentPlaylists.length > 0 && (
        <section>
          <div className="mb-4 flex items-end justify-between">
            <h2 className="text-lg font-medium">{t('Playlists')}</h2>
            <Link to="/playlists" className="text-sm text-muted-foreground hover:text-foreground">
              {t('See all')}
            </Link>
          </div>
          <div className="grid grid-cols-1 gap-3 md:grid-cols-2 lg:grid-cols-3">
            {recentPlaylists.map((p) => (
              <Link
                key={p.id}
                to={`/playlists/${p.id}`}
                className="flex items-center gap-3 rounded-xl bg-card p-4 hover:bg-accent"
              >
                <div className="flex h-12 w-12 items-center justify-center rounded-lg bg-muted">
                  <ListMusic size={20} className="text-muted-foreground" />
                </div>
                <div className="min-w-0">
                  <div className="truncate font-medium">{p.name}</div>
                  <div className="text-xs text-muted-foreground">
                    {t('{n} tracks · {duration}', { n: p.count, duration: formatDuration(p.duration) })}
                  </div>
                </div>
              </Link>
            ))}
          </div>
        </section>
      )}
    </div>
  );
}
