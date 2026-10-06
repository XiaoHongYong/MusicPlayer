import { useMemo } from 'react';
import { useLibrarySnapshot } from './SongsPage';
import { usePlayerStore } from '@/features/player/store';
import { CoverImage } from '@/components/CoverImage';

export function AlbumsPage() {
  const { data } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const albums = useMemo(() => {
    const map = new Map<string, { name: string; artist: string; count: number; coverId: number }>();
    for (const s of data?.songs ?? []) {
      const key = `${s.album}::${s.artist}`;
      const cur = map.get(key);
      if (cur) cur.count += 1;
      else map.set(key, { name: s.album || 'Unknown', artist: s.artist, count: 1, coverId: s.id });
    }
    return [...map.values()].sort((a, b) => a.name.localeCompare(b.name));
  }, [data]);

  return (
    <div className="p-6">
      <h1 className="mb-6 text-2xl font-semibold">Albums</h1>
      <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
        {albums.map((a) => (
          <button
            key={`${a.name}-${a.artist}`}
            className="text-left"
            onClick={() => {
              const songs = (data?.songs ?? []).filter((s) => s.album === a.name && s.artist === a.artist);
              playSongs(songs, 0);
            }}
          >
            <CoverImage songId={a.coverId} className="aspect-square w-full rounded-lg" />
            <div className="mt-2 truncate text-sm font-medium">{a.name}</div>
            <div className="truncate text-xs text-muted-foreground">
              {a.artist} · {a.count} 首
            </div>
          </button>
        ))}
      </div>
    </div>
  );
}

export function ArtistsPage() {
  const { data } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const artists = useMemo(() => {
    const map = new Map<string, { name: string; count: number; albums: Set<string>; coverId: number }>();
    for (const s of data?.songs ?? []) {
      const cur = map.get(s.artist);
      if (cur) {
        cur.count += 1;
        cur.albums.add(s.album);
      } else {
        map.set(s.artist, { name: s.artist || 'Unknown', count: 1, albums: new Set([s.album]), coverId: s.id });
      }
    }
    return [...map.values()].sort((a, b) => a.name.localeCompare(b.name));
  }, [data]);

  return (
    <div className="p-6">
      <h1 className="mb-6 text-2xl font-semibold">Artists</h1>
      <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
        {artists.map((a) => (
          <button
            key={a.name}
            className="rounded-lg bg-card p-4 text-left hover:bg-accent"
            onClick={() => playSongs((data?.songs ?? []).filter((s) => s.artist === a.name), 0)}
          >
            <CoverImage songId={a.coverId} kind="artist" className="aspect-square w-full rounded-lg" />
            <div className="mt-3 truncate text-center text-sm font-medium">{a.name}</div>
            <div className="text-center text-xs text-muted-foreground">
              {a.albums.size} 张专辑 · {a.count} 首
            </div>
          </button>
        ))}
      </div>
    </div>
  );
}

export function GenresPage() {
  const { data } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const genres = useMemo(() => {
    const map = new Map<string, number>();
    for (const s of data?.songs ?? []) {
      map.set(s.genre || 'Unknown', (map.get(s.genre || 'Unknown') ?? 0) + 1);
    }
    return [...map.entries()].sort((a, b) => b[1] - a[1]);
  }, [data]);

  return (
    <div className="p-6">
      <h1 className="mb-6 text-2xl font-semibold">Genres</h1>
      <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
        {genres.map(([name, count]) => (
          <button
            key={name}
            className="rounded-xl bg-card p-6 text-left hover:bg-accent"
            onClick={() => playSongs((data?.songs ?? []).filter((s) => (s.genre || 'Unknown') === name), 0)}
          >
            <div className="text-lg font-semibold">{name || 'Unknown'}</div>
            <div className="text-sm text-muted-foreground">{count} 首</div>
          </button>
        ))}
      </div>
    </div>
  );
}
