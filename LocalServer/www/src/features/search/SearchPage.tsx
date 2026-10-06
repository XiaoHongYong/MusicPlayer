import { useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import { ListMusic } from 'lucide-react';
import { api } from '@/api/client';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration } from '@/lib/utils';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from '@/features/library/components/MediaContextMenu';
import { usePlayerStore } from '@/features/player/store';
import { normalizeQuery, songMatchesQuery, textMatches } from './match';

const SONG_LIMIT = 80;
const GROUP_LIMIT = 16;

export function SearchPage() {
  const [params] = useSearchParams();
  const q = params.get('q') ?? '';
  const nq = normalizeQuery(q);
  const { data: snapshot, isLoading, error } = useLibrarySnapshot();
  const { data: playlists = [] } = useQuery({ queryKey: ['playlists'], queryFn: api.playlists });
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songs = snapshot?.songs ?? [];

  const songHits = useMemo(() => {
    if (!nq) return [];
    return songs.filter((s) => songMatchesQuery(s, nq)).slice(0, SONG_LIMIT);
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
    return [...map.values()].slice(0, GROUP_LIMIT);
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
    return [...map.values()].slice(0, GROUP_LIMIT);
  }, [songs, nq]);

  const playlistHits = useMemo(() => {
    if (!nq) return [];
    return playlists.filter((p) => textMatches(nq, p.name)).slice(0, GROUP_LIMIT);
  }, [playlists, nq]);

  if (isLoading) return <p className="p-8 text-muted-foreground">加载媒体库…</p>;
  if (error) return <p className="p-8 text-red-500">无法加载媒体库</p>;

  const empty = !nq;
  const noHits =
    !empty &&
    songHits.length === 0 &&
    albumHits.length === 0 &&
    artistHits.length === 0 &&
    playlistHits.length === 0;

  return (
    <div className="space-y-8 p-6">
      <div>
        <h1 className="text-2xl font-semibold">搜索</h1>
        <p className="mt-1 text-sm text-muted-foreground">
          {empty ? '在顶部输入歌曲、艺人、专辑或歌单名称' : `“${q.trim()}” 的结果`}
        </p>
      </div>

      {empty && <p className="text-sm text-muted-foreground">开始输入以搜索整个媒体库。</p>}
      {noHits && <p className="text-sm text-muted-foreground">没有找到匹配项。</p>}

      {songHits.length > 0 && (
        <section>
          <h2 className="mb-3 text-sm font-medium text-muted-foreground">歌曲</h2>
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
        </section>
      )}

      {albumHits.length > 0 && (
        <section>
          <h2 className="mb-3 text-sm font-medium text-muted-foreground">专辑</h2>
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
            {albumHits.map((a) => {
              const albumSongs = songs.filter((s) => s.album === a.name && s.artist === a.artist);
              return (
                <button
                  key={`${a.name}-${a.artist}`}
                  className="text-left"
                  onClick={() => playSongs(albumSongs, 0)}
                  onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, albumSongs))}
                >
                  <CoverImage songId={a.coverId} className="aspect-square w-full rounded-lg" />
                  <div className="mt-2 truncate text-sm font-medium">{a.name}</div>
                  <div className="truncate text-xs text-muted-foreground">
                    {a.artist} · {a.count} 首
                  </div>
                </button>
              );
            })}
          </div>
        </section>
      )}

      {artistHits.length > 0 && (
        <section>
          <h2 className="mb-3 text-sm font-medium text-muted-foreground">艺人</h2>
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
            {artistHits.map((a) => {
              const artistSongs = songs.filter((s) => s.artist === a.name);
              return (
                <button
                  key={a.name}
                  className="rounded-lg bg-card p-4 text-left hover:bg-accent"
                  onClick={() => playSongs(artistSongs, 0)}
                  onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, artistSongs))}
                >
                  <CoverImage songId={a.coverId} kind="artist" className="aspect-square w-full rounded-lg" />
                  <div className="mt-3 truncate text-center text-sm font-medium">{a.name}</div>
                  <div className="text-center text-xs text-muted-foreground">{a.count} 首</div>
                </button>
              );
            })}
          </div>
        </section>
      )}

      {playlistHits.length > 0 && (
        <section>
          <h2 className="mb-3 text-sm font-medium text-muted-foreground">歌单</h2>
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
                    {p.count} 首 · {formatDuration(p.duration)}
                  </div>
                </div>
              </Link>
            ))}
          </div>
        </section>
      )}

      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
