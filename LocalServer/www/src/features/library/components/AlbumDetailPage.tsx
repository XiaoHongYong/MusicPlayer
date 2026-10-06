import { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Play, Shuffle } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { CoverImage } from '@/components/CoverImage';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { MediaContextMenu, type MediaMenuTarget } from './MediaContextMenu';
import { TrackList } from './TrackList';
import { useLibrarySnapshot } from './SongsPage';
import { artistPath, shuffleSongs } from '../groups';
import { useT } from '@/i18n';

export function AlbumDetailPage() {
  const t = useT();
  const { artist = '', album = '' } = useParams();
  const artistName = decodeURIComponent(artist);
  const albumName = decodeURIComponent(album);
  const { data, isLoading } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);

  const songs = useMemo(() => {
    return (data?.songs ?? [])
      .filter((s) => s.artist === artistName && s.album === albumName)
      .sort((a, b) => a.title.localeCompare(b.title));
  }, [data, artistName, albumName]);

  const year = songs.find((s) => s.year > 0)?.year;
  const total = songs.reduce((n, s) => n + (s.duration > 0 ? s.duration : 0), 0);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading album…')}</p>;

  return (
    <div className="space-y-6 p-6">
      <Link to="/library/albums" className="text-sm text-muted-foreground hover:text-foreground">
        ← {t('Albums')}
      </Link>
      <div className="flex flex-col gap-6 sm:flex-row">
        <CoverImage songId={songs[0]?.id} className="h-48 w-48 shrink-0 rounded-xl" />
        <div className="min-w-0 flex-1">
          <h1 className="text-2xl font-semibold">{albumName}</h1>
          <p className="mt-1 text-sm text-muted-foreground">
            <Link className="hover:text-foreground" to={artistPath(artistName)}>
              {artistName}
            </Link>
            {year ? ` · ${year}` : ''} · {t('{n} tracks · {duration}', { n: songs.length, duration: formatDuration(total) })}
          </p>
          <div className="mt-4 flex flex-wrap gap-2">
            <Button onClick={() => playSongs(songs, 0)} disabled={!songs.length}>
              <Play size={14} className="mr-1" />
              {t('Play')}
            </Button>
            <Button variant="outline" onClick={() => playSongs(shuffleSongs(songs), 0)} disabled={!songs.length}>
              <Shuffle size={14} className="mr-1" />
              {t('Shuffle')}
            </Button>
          </div>
        </div>
      </div>
      <TrackList songs={songs} onMenu={setMenu} />
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
