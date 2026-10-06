import { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Play, Shuffle } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { MediaContextMenu, mediaMenuFromEvent, type MediaMenuTarget } from './MediaContextMenu';
import { TrackList } from './TrackList';
import { useLibrarySnapshot } from './SongsPage';
import { albumPath, groupAlbums, shuffleSongs } from '../groups';

export function ArtistDetailPage() {
  const { name = '' } = useParams();
  const artistName = decodeURIComponent(name);
  const { data, isLoading } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);

  const songs = useMemo(
    () => (data?.songs ?? []).filter((s) => (s.artist || 'Unknown') === artistName),
    [data, artistName],
  );
  const albums = useMemo(() => groupAlbums(songs).sort((a, b) => b.year - a.year || a.name.localeCompare(b.name)), [songs]);
  const topSongs = useMemo(
    () => [...songs].sort((a, b) => b.play_count - a.play_count).slice(0, 8),
    [songs],
  );
  const total = songs.reduce((n, s) => n + (s.duration > 0 ? s.duration : 0), 0);

  if (isLoading) return <p className="p-8 text-muted-foreground">加载艺人…</p>;

  return (
    <div className="space-y-8 p-6">
      <Link to="/library/artists" className="text-sm text-muted-foreground hover:text-foreground">
        ← Artists
      </Link>
      <div className="flex flex-col gap-6 sm:flex-row">
        <CoverImage songId={songs[0]?.id} kind="artist" className="h-40 w-40 shrink-0 rounded-xl" />
        <div className="min-w-0 flex-1">
          <h1 className="text-2xl font-semibold">{artistName}</h1>
          <p className="mt-1 text-sm text-muted-foreground">
            {albums.length} 张专辑 · {songs.length} 首 · {formatDuration(total)}
          </p>
          <div className="mt-4 flex flex-wrap gap-2">
            <Button onClick={() => playSongs(songs, 0)} disabled={!songs.length}>
              <Play size={14} className="mr-1" />
              播放
            </Button>
            <Button variant="outline" onClick={() => playSongs(shuffleSongs(songs), 0)} disabled={!songs.length}>
              <Shuffle size={14} className="mr-1" />
              随机播放
            </Button>
          </div>
        </div>
      </div>
      {topSongs.length > 0 && (
        <section>
          <h2 className="mb-3 text-sm font-medium text-muted-foreground">常听</h2>
          <TrackList songs={topSongs} onMenu={setMenu} />
        </section>
      )}
      <section>
        <h2 className="mb-3 text-sm font-medium text-muted-foreground">专辑</h2>
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          {albums.map((a) => {
            const albumSongs = songs.filter((s) => s.album === a.name);
            return (
              <Link
                key={a.name}
                to={albumPath(a.artist, a.name)}
                className="text-left"
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, albumSongs))}
              >
                <CoverImage songId={a.coverId} className="aspect-square w-full rounded-lg" />
                <div className="mt-2 truncate text-sm font-medium">{a.name}</div>
                <div className="truncate text-xs text-muted-foreground">
                  {a.year || '—'} · {a.count} 首
                </div>
              </Link>
            );
          })}
        </div>
      </section>
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
