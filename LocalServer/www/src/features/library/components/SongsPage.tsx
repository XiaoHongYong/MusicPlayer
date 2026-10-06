import { useEffect, useMemo, useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { ArrowDown, ArrowUp, ArrowUpDown, ListPlus, Mic2, Play } from 'lucide-react';
import { api } from '@/api/client';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { TablePagination } from '@/components/TablePagination';
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
import {
  clampPage,
  pageSlice,
  parseStatsPageSize,
  type StatsPageSize,
} from '@/features/statistics/pagination';
import { compareSongs, nextSongSort, type SongSort, type SongSortKey } from '../songSort';

const SONGS_PAGE_SIZE_KEY = 'pmc.songsPageSize';

function loadSongsPageSize(): StatsPageSize {
  try {
    return parseStatsPageSize(localStorage.getItem(SONGS_PAGE_SIZE_KEY));
  } catch {
    return 20;
  }
}

function saveSongsPageSize(size: StatsPageSize) {
  try {
    localStorage.setItem(SONGS_PAGE_SIZE_KEY, String(size));
  } catch {
    /* ignore */
  }
}

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
  sort: SongSort,
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
    return [...filtered].sort((a, b) => compareSongs(a, b, sort));
  }, [songs, q, genre, artist, rating, hasLyrics, sort]);
}

function SortHeader({
  label,
  column,
  sort,
  onSort,
  className = '',
  align = 'left',
}: {
  label: string;
  column: SongSortKey;
  sort: SongSort;
  onSort: (key: SongSortKey) => void;
  className?: string;
  align?: 'left' | 'right';
}) {
  const active = sort.key === column;
  const Icon = !active ? ArrowUpDown : sort.dir === 'asc' ? ArrowUp : ArrowDown;
  return (
    <th
      className={`px-2 py-2 font-medium ${className}`}
      aria-sort={active ? (sort.dir === 'asc' ? 'ascending' : 'descending') : 'none'}
    >
      <button
        type="button"
        className={`inline-flex w-full items-center gap-1 hover:text-foreground ${align === 'right' ? 'justify-end' : ''}`}
        onClick={() => onSort(column)}
      >
        <span>{label}</span>
        <Icon size={13} className={active ? 'text-foreground' : 'opacity-40'} />
      </button>
    </th>
  );
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
  const [sort, setSort] = useState<SongSort>({ key: 'title', dir: 'asc' });
  const [pageSize, setPageSize] = useState<StatsPageSize>(loadSongsPageSize);
  const [page, setPage] = useState(1);
  const [playlistSong, setPlaylistSong] = useState<Song | null>(null);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songs = useFilteredSongs(data?.songs, q, genre, artist, rating, hasLyrics, sort);
  const playSongs = usePlayerStore((s) => s.playSongs);
  const addToQueue = usePlayerStore((s) => s.addToQueue);
  const openLyrics = useUiStore((s) => s.openLyrics);
  const rate = useMutation({
    mutationFn: ({ id, rating }: { id: number; rating: number }) => api.setRating(id, rating),
    onSuccess: () => {
      void qc.invalidateQueries({ queryKey: ['library-snapshot'] });
      void qc.invalidateQueries({ queryKey: ['statistics-snapshot'] });
    },
  });
  const startScan = async () => {
    await api.startScan();
    void refetch();
  };

  useEffect(() => {
    setPage(1);
  }, [q, genre, artist, rating, hasLyrics, sort.key, sort.dir, songs.length]);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading library…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load library')}</p>;

  const currentPage = clampPage(page, songs.length, pageSize);
  const rows = pageSlice(songs, currentPage, pageSize);

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
          value={hasLyrics}
          onChange={(e) => setHasLyrics(e.target.value)}
        >
          <option value="">{t('Lyrics: all')}</option>
          <option value="yes">{t('Has lyrics')}</option>
          <option value="no">{t('No lyrics')}</option>
        </select>
      </div>
      <div className="min-h-0 flex-1 overflow-auto rounded-xl bg-card">
        <table className="w-full min-w-[860px] text-left text-sm">
          <thead className="sticky top-0 z-10 bg-card text-xs uppercase text-muted-foreground">
            <tr className="border-b border-border">
              <th className="w-10 px-2 py-2 font-medium" />
              <SortHeader label={t('Title')} column="title" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Artist')} column="artist" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Album')} column="album" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Duration')} column="duration" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Plays')} column="plays" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Rating')} column="rating" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <SortHeader label={t('Lyrics')} column="lyrics" sort={sort} onSort={(key) => setSort((s) => nextSongSort(s, key))} />
              <th className="w-16 px-2 py-2 font-medium" />
              <th className="w-10 px-2 py-2 font-medium" />
            </tr>
          </thead>
          <tbody>
            {rows.map((song, i) => {
              const index = (currentPage - 1) * pageSize + i;
              return (
                <tr
                  key={song.id}
                  className="cursor-pointer border-b border-border/60 hover:bg-accent"
                  onClick={() => playSongs(songs, index)}
                  onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, [song]))}
                >
                  <td className="px-2 py-2">
                    <button type="button" className="text-muted-foreground hover:text-foreground">
                      <Play size={14} />
                    </button>
                  </td>
                  <td className="max-w-[14rem] truncate px-2 py-2">{song.title}</td>
                  <td className="max-w-[10rem] truncate px-2 py-2 text-muted-foreground">{song.artist}</td>
                  <td className="max-w-[10rem] truncate px-2 py-2 text-muted-foreground">{song.album}</td>
                  <td className="px-2 py-2 text-xs text-muted-foreground">{formatDuration(song.duration)}</td>
                  <td className="px-2 py-2 text-xs tabular-nums text-muted-foreground">{song.play_count}</td>
                  <td className="px-2 py-2" onClick={(e) => e.stopPropagation()}>
                    <RatingStars value={song.rating} onChange={(next) => rate.mutate({ id: song.id, rating: next })} />
                  </td>
                  <td className="px-2 py-2" onClick={(e) => e.stopPropagation()}>
                    {song.has_lyrics ? (
                      <button type="button" onClick={() => openLyrics(song.id)}>
                        <Badge className="bg-primary/15 text-primary">
                          <Mic2 size={12} className="mr-1" />
                          {t('Yes')}
                        </Badge>
                      </button>
                    ) : (
                      <span className="text-xs text-muted-foreground">{t('No')}</span>
                    )}
                  </td>
                  <td className="px-2 py-2" onClick={(e) => e.stopPropagation()}>
                    <Button variant="ghost" className="h-7 px-2 text-xs" onClick={() => addToQueue([song])}>
                      {t('Queue')}
                    </Button>
                  </td>
                  <td className="px-2 py-2" onClick={(e) => e.stopPropagation()}>
                    <Button variant="ghost" className="h-7 px-1" onClick={() => setPlaylistSong(song)} title={t('Add to playlist')}>
                      <ListPlus size={14} />
                    </Button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
        {songs.length === 0 && <p className="p-4 text-sm text-muted-foreground">{t('No matches found.')}</p>}
      </div>
      {songs.length > 0 && (
        <TablePagination
          total={songs.length}
          page={currentPage}
          pageSize={pageSize}
          onPageChange={setPage}
          onPageSizeChange={(size) => {
            setPageSize(size);
            saveSongsPageSize(size);
            setPage(1);
          }}
        />
      )}
      <AddToPlaylistDialog
        songs={playlistSong ? [playlistSong] : []}
        open={playlistSong != null}
        onClose={() => setPlaylistSong(null)}
      />
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
