import { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Play, Shuffle } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { MediaContextMenu, type MediaMenuTarget } from './MediaContextMenu';
import { TrackList } from './TrackList';
import { useLibrarySnapshot } from './SongsPage';
import { shuffleSongs } from '../groups';

export function GenreDetailPage() {
  const { name = '' } = useParams();
  const genreName = decodeURIComponent(name);
  const { data, isLoading } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);

  const songs = useMemo(
    () => (data?.songs ?? []).filter((s) => (s.genre || 'Unknown') === genreName),
    [data, genreName],
  );
  const total = songs.reduce((n, s) => n + (s.duration > 0 ? s.duration : 0), 0);
  const plays = songs.reduce((n, s) => n + s.play_count, 0);

  if (isLoading) return <p className="p-8 text-muted-foreground">加载类型…</p>;

  return (
    <div className="space-y-6 p-6">
      <Link to="/library/genres" className="text-sm text-muted-foreground hover:text-foreground">
        ← Genres
      </Link>
      <div>
        <h1 className="text-2xl font-semibold">{genreName}</h1>
        <p className="mt-1 text-sm text-muted-foreground">
          {songs.length} 首 · 播放 {plays} 次 · {formatDuration(total)}
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
      <TrackList songs={songs} onMenu={setMenu} />
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
