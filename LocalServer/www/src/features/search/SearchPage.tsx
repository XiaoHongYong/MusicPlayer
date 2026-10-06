import { useMemo, useState } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import { ListMusic } from 'lucide-react';
import { api } from '@/api/client';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration, cn } from '@/lib/utils';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from '@/features/library/components/MediaContextMenu';
import { albumPath, artistPath } from '@/features/library/groups';
import { usePlayerStore } from '@/features/player/store';
import {
  SEARCH_TABS,
  normalizeQuery,
  parseSearchTab,
  searchHref,
  songMatchesQuery,
  textMatches,
  type SearchTab,
} from './match';
import { useT, type TranslateFn } from '@/i18n';

function tabLabel(id: SearchTab, t: TranslateFn) {
  if (id === 'songs') return t('Songs');
  if (id === 'albums') return t('Albums');
  if (id === 'artists') return t('Artists');
  return t('Playlists');
}

export function SearchPage() {
  const t = useT();
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const q = params.get('q') ?? '';
  const tab = parseSearchTab(params.get('tab'));
  const nq = normalizeQuery(q);
  const { data: snapshot, isLoading, error } = useLibrarySnapshot();
  const { data: playlists = [] } = useQuery({ queryKey: ['playlists'], queryFn: api.playlists });
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songs = snapshot?.songs ?? [];

  const songHits = useMemo(() => {
    if (!nq) return [];
    return songs.filter((s) => songMatchesQuery(s, nq));
  }, [songs, nq]);

  const albumHits = useMemo(() => {
    if (!nq) return [];
    const map = new Map<string, { name: string; artist: string; count: number; coverId: number }>();
    for (const s of songs) {
      if (!textMatches(nq, s.album, s.artist)) continue;
      const key = `${s.album}::${s.artist}`;
      const cur = map.get(key);
      if (cur) cur.count += 1;
      else map.set(key, { name: s.album || 'Unknown', artist: s.artist, count: 1, coverId: s.id });
    }
    return [...map.values()];
  }, [songs, nq]);

  const artistHits = useMemo(() => {
    if (!nq) return [];
    const map = new Map<string, { name: string; count: number; coverId: number }>();
    for (const s of songs) {
      if (!textMatches(nq, s.artist)) continue;
      const cur = map.get(s.artist);
      if (cur) cur.count += 1;
      else map.set(s.artist, { name: s.artist || 'Unknown', count: 1, coverId: s.id });
    }
    return [...map.values()];
  }, [songs, nq]);

  const playlistHits = useMemo(() => {
    if (!nq) return [];
    return playlists.filter((p) => textMatches(nq, p.name));
  }, [playlists, nq]);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading library…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load library')}</p>;

  const empty = !nq;
  const counts: Record<SearchTab, number> = {
    songs: songHits.length,
    albums: albumHits.length,
    artists: artistHits.length,
    playlists: playlistHits.length,
  };
  const noHits =
    !empty &&
    counts.songs === 0 &&
    counts.albums === 0 &&
    counts.artists === 0 &&
    counts.playlists === 0;
  const tabEmpty = !empty && counts[tab] === 0;

  const setTab = (next: SearchTab) => {
    navigate(searchHref(q, next), { replace: true });
  };

  return (
    <div className="space-y-6 p-6">
      <div
        className="flex gap-1 border-b border-border"
        role="tablist"
        aria-label={t('Search')}
      >
        {SEARCH_TABS.map((id) => {
          const label = tabLabel(id, t);
          const selected = tab === id;
          return (
            <button
              key={id}
              type="button"
              role="tab"
              aria-selected={selected}
              className={cn(
                '-mb-px border-b-2 px-3 py-2.5 text-sm font-medium transition',
                selected
                  ? 'border-primary text-foreground'
                  : 'border-transparent text-muted-foreground hover:border-border hover:text-foreground',
              )}
              onClick={() => setTab(id)}
            >
              {label}
              {!empty && (
                <span className={cn('ml-1.5 tabular-nums', selected ? 'text-muted-foreground' : 'opacity-70')}>
                  {counts[id]}
                </span>
              )}
            </button>
          );
        })}
      </div>

      {empty && <p className="text-sm text-muted-foreground">{t('Start typing to search the whole library.')}</p>}
      {noHits && <p className="text-sm text-muted-foreground">{t('No matches found.')}</p>}
      {!empty && !noHits && tabEmpty && (
        <p className="text-sm text-muted-foreground">{t('No matches found.')}</p>
      )}

      {tab === 'songs' && songHits.length > 0 && (
        <div className="space-y-1">
          {songHits.map((s, i) => (
            <button
              key={s.id}
              className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
              onClick={() => playSongs(songHits, i)}
              onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, [s]))}
            >
              <CoverImage songId={s.id} className="h-10 w-10 rounded" />
              <div className="min-w-0 flex-1">
                <div className="truncate text-sm">{s.title}</div>
                <div className="truncate text-xs text-muted-foreground">
                  {s.artist} · {s.album}
                </div>
              </div>
              <span className="text-xs text-muted-foreground">{formatDuration(s.duration)}</span>
            </button>
          ))}
        </div>
      )}

      {tab === 'albums' && albumHits.length > 0 && (
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          {albumHits.map((a) => {
            const albumSongs = songs.filter((s) => s.album === a.name && s.artist === a.artist);
            return (
              <Link
                key={`${a.name}-${a.artist}`}
                to={albumPath(a.artist, a.name)}
                className="text-left"
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, albumSongs))}
              >
                <CoverImage songId={a.coverId} className="aspect-square w-full rounded-lg" />
                <div className="mt-2 truncate text-sm font-medium">{a.name}</div>
                <div className="truncate text-xs text-muted-foreground">
                  {a.artist} · {t('{n} tracks', { n: a.count })}
                </div>
              </Link>
            );
          })}
        </div>
      )}

      {tab === 'artists' && artistHits.length > 0 && (
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          {artistHits.map((a) => {
            const artistSongs = songs.filter((s) => s.artist === a.name);
            return (
              <Link
                key={a.name}
                to={artistPath(a.name)}
                className="rounded-lg bg-card p-4 text-left hover:bg-accent"
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, artistSongs))}
              >
                <CoverImage songId={a.coverId} kind="artist" className="aspect-square w-full rounded-lg" />
                <div className="mt-3 truncate text-center text-sm font-medium">{a.name}</div>
                <div className="text-center text-xs text-muted-foreground">{t('{n} tracks', { n: a.count })}</div>
              </Link>
            );
          })}
        </div>
      )}

      {tab === 'playlists' && playlistHits.length > 0 && (
        <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
          {playlistHits.map((p) => (
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
      )}

      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
