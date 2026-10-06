import { useMemo, useRef, useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useVirtualizer } from '@tanstack/react-virtual';
import { Mic2, Play, ListPlus } from 'lucide-react';
import { api } from '@/api/client';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { useUiStore } from '@/stores/ui-store';
import type { Song } from '@/api/types';
import { RatingStars } from './RatingStars';
import { AddToPlaylistDialog } from '@/features/playlists/components/AddToPlaylistDialog';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from './MediaContextMenu';
import { useT } from '@/i18n';

export function useLibrarySnapshot() {
  return useQuery({
    queryKey: ['library-snapshot'],
    queryFn: api.snapshot,
    staleTime: 30_000,
  });
}

function useFilteredSongs(
  songs: Song[] | undefined,
  q: string,
  genre: string,
  artist: string,
  rating: string,
  hasLyrics: string,
  sort: string,
) {
  return useMemo(() => {
    const list = songs ?? [];
    const query = q.trim().toLowerCase();
    const minRating = rating ? Number(rating) : 0;
    const filtered = list.filter((s) => {
      if (genre && s.genre !== genre) return false;
      if (artist && s.artist !== artist) return false;
      if (minRating && s.rating < minRating) return false;
      if (hasLyrics === 'yes' && !s.has_lyrics) return false;
      if (hasLyrics === 'no' && s.has_lyrics) return false;
      if (!query) return true;
      return [s.title, s.artist, s.album].some((x) => x.toLowerCase().includes(query));
    });
    return [...filtered].sort((a, b) => {
      if (sort === 'artist') return a.artist.localeCompare(b.artist) || a.title.localeCompare(b.title);
      if (sort === 'album') return a.album.localeCompare(b.album) || a.title.localeCompare(b.title);
      if (sort === 'rating') return b.rating - a.rating || a.title.localeCompare(b.title);
      if (sort === 'plays') return b.play_count - a.play_count || a.title.localeCompare(b.title);
      return a.title.localeCompare(b.title);
    });
  }, [songs, q, genre, artist, rating, hasLyrics, sort]);
}

export function SongsPage() {
  const t = useT();
  const { data, isLoading, error, refetch } = useLibrarySnapshot();
  const qc = useQueryClient();
  const [q, setQ] = useState('');
  const [genre, setGenre] = useState('');
  const [artist, setArtist] = useState('');
  const [rating, setRating] = useState('');
  const [hasLyrics, setHasLyrics] = useState('');
  const [sort, setSort] = useState('title');
  const [playlistSong, setPlaylistSong] = useState<Song | null>(null);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songs = useFilteredSongs(data?.songs, q, genre, artist, rating, hasLyrics, sort);
  const playSongs = usePlayerStore((s) => s.playSongs);
  const addToQueue = usePlayerStore((s) => s.addToQueue);
  const openLyrics = useUiStore((s) => s.openLyrics);
  const rate = useMutation({
    mutationFn: ({ id, rating }: { id: number; rating: number }) => api.setRating(id, rating),
    onSuccess: () => void qc.invalidateQueries({ queryKey: ['library-snapshot'] }),
  });
  const startScan = async () => {
    await api.startScan();
    void refetch();
  };

  const parentRef = useRef<HTMLDivElement>(null);
  const virtualizer = useVirtualizer({
    count: songs.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 44,
    overscan: 12,
  });

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading library…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load library')}</p>;

  return (
    <div className="flex h-full flex-col gap-4 p-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-semibold">{t('Songs')}</h1>
        <Button onClick={startScan}>{t('Scan')}</Button>
      </div>
      <div className="flex flex-wrap gap-2">
        <Input placeholder={t('Search songs…')} value={q} onChange={(e) => setQ(e.target.value)} className="max-w-xs" />
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={genre}
          onChange={(e) => setGenre(e.target.value)}
        >
          <option value="">{t('All genres')}</option>
          {(data?.genres ?? []).map((g) => (
            <option key={g} value={g}>
              {g}
            </option>
          ))}
        </select>
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={artist}
          onChange={(e) => setArtist(e.target.value)}
        >
          <option value="">{t('All artists')}</option>
          {(data?.artists ?? []).map((a) => (
            <option key={a} value={a}>
              {a}
            </option>
          ))}
        </select>
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={rating}
          onChange={(e) => setRating(e.target.value)}
        >
          <option value="">{t('All ratings')}</option>
          <option value="5">{t('5 stars')}</option>
          <option value="4">{t('4 stars and up')}</option>
          <option value="3">{t('3 stars and up')}</option>
          <option value="1">{t('Rated')}</option>
        </select>
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={sort}
          onChange={(e) => setSort(e.target.value)}
        >
          <option value="title">{t('Sort by title')}</option>
          <option value="artist">{t('Sort by artist')}</option>
          <option value="album">{t('Sort by album')}</option>
          <option value="rating">{t('Sort by rating')}</option>
          <option value="plays">{t('Sort by plays')}</option>
        </select>
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={hasLyrics}
          onChange={(e) => setHasLyrics(e.target.value)}
        >
          <option value="">{t('Lyrics: all')}</option>
          <option value="yes">{t('Has lyrics')}</option>
          <option value="no">{t('No lyrics')}</option>
        </select>
      </div>
      <div className="grid grid-cols-[40px_1.4fr_1fr_1fr_72px_88px_56px_52px_52px] gap-2 px-2 text-xs uppercase text-muted-foreground">
        <span />
        <span>{t('Title')}</span>
        <span>{t('Artist')}</span>
        <span>{t('Album')}</span>
        <span>{t('Duration')}</span>
        <span>{t('Rating')}</span>
        <span>{t('Lyrics')}</span>
        <span />
        <span />
      </div>
      <div ref={parentRef} className="min-h-0 flex-1 overflow-auto">
        <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
          {virtualizer.getVirtualItems().map((row) => {
            const song = songs[row.index];
            return (
              <div
                key={song.id}
                className="absolute left-0 grid w-full grid-cols-[40px_1.4fr_1fr_1fr_72px_88px_56px_52px_52px] items-center gap-2 rounded-md px-2 hover:bg-accent"
                style={{ height: 44, transform: `translateY(${row.start}px)` }}
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, [song]))}
              >
                <button onClick={() => playSongs(songs, row.index)} className="text-muted-foreground hover:text-foreground">
                  <Play size={14} />
                </button>
                <button className="truncate text-left text-sm" onClick={() => playSongs(songs, row.index)}>
                  {song.title}
                </button>
                <span className="truncate text-sm text-muted-foreground">{song.artist}</span>
                <span className="truncate text-sm text-muted-foreground">{song.album}</span>
                <span className="text-xs text-muted-foreground">{formatDuration(song.duration)}</span>
                <RatingStars value={song.rating} onChange={(rating) => rate.mutate({ id: song.id, rating })} />
                <span>
                  {song.has_lyrics ? (
                    <button onClick={() => openLyrics(song.id)}>
                      <Badge className="bg-primary/15 text-primary">
                        <Mic2 size={12} className="mr-1" />
                        {t('Yes')}
                      </Badge>
                    </button>
                  ) : (
                    <span className="text-xs text-muted-foreground">{t('No')}</span>
                  )}
                </span>
                <Button variant="ghost" className="h-7 px-2 text-xs" onClick={() => addToQueue([song])}>
                  {t('Queue')}
                </Button>
                <Button variant="ghost" className="h-7 px-1" onClick={() => setPlaylistSong(song)} title={t('Add to playlist')}>
                  <ListPlus size={14} />
                </Button>
              </div>
            );
          })}
        </div>
      </div>
      <AddToPlaylistDialog
        songs={playlistSong ? [playlistSong] : []}
        open={playlistSong != null}
        onClose={() => setPlaylistSong(null)}
      />
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
