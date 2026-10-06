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

export function useLibrarySnapshot() {
  return useQuery({
    queryKey: ['library-snapshot'],
    queryFn: api.snapshot,
    staleTime: 30_000,
  });
}

function useFilteredSongs(songs: Song[] | undefined, q: string, genre: string, hasLyrics: string) {
  return useMemo(() => {
    const list = songs ?? [];
    const query = q.trim().toLowerCase();
    return list.filter((s) => {
      if (genre && s.genre !== genre) return false;
      if (hasLyrics === 'yes' && !s.has_lyrics) return false;
      if (hasLyrics === 'no' && s.has_lyrics) return false;
      if (!query) return true;
      return [s.title, s.artist, s.album].some((x) => x.toLowerCase().includes(query));
    });
  }, [songs, q, genre, hasLyrics]);
}

export function SongsPage() {
  const { data, isLoading, error, refetch } = useLibrarySnapshot();
  const qc = useQueryClient();
  const [q, setQ] = useState('');
  const [genre, setGenre] = useState('');
  const [hasLyrics, setHasLyrics] = useState('');
  const [playlistSong, setPlaylistSong] = useState<Song | null>(null);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const songs = useFilteredSongs(data?.songs, q, genre, hasLyrics);
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

  if (isLoading) return <p className="p-8 text-muted-foreground">加载媒体库…</p>;
  if (error) return <p className="p-8 text-red-500">无法加载媒体库</p>;

  return (
    <div className="flex h-full flex-col gap-4 p-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-semibold">Songs</h1>
        <Button onClick={startScan}>Scan</Button>
      </div>
      <div className="flex flex-wrap gap-2">
        <Input placeholder="搜索歌曲…" value={q} onChange={(e) => setQ(e.target.value)} className="max-w-xs" />
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={genre}
          onChange={(e) => setGenre(e.target.value)}
        >
          <option value="">全部类型</option>
          {(data?.genres ?? []).map((g) => (
            <option key={g} value={g}>
              {g}
            </option>
          ))}
        </select>
        <select
          className="h-9 rounded-md border border-border bg-background px-2 text-sm"
          value={hasLyrics}
          onChange={(e) => setHasLyrics(e.target.value)}
        >
          <option value="">歌词：全部</option>
          <option value="yes">有歌词</option>
          <option value="no">无歌词</option>
        </select>
      </div>
      <div className="grid grid-cols-[40px_1.4fr_1fr_1fr_72px_88px_56px_52px_52px] gap-2 px-2 text-xs uppercase text-muted-foreground">
        <span />
        <span>Title</span>
        <span>Artist</span>
        <span>Album</span>
        <span>时长</span>
        <span>评分</span>
        <span>歌词</span>
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
                        有
                      </Badge>
                    </button>
                  ) : (
                    <span className="text-xs text-muted-foreground">无</span>
                  )}
                </span>
                <Button variant="ghost" className="h-7 px-2 text-xs" onClick={() => addToQueue([song])}>
                  队列
                </Button>
                <Button variant="ghost" className="h-7 px-1" onClick={() => setPlaylistSong(song)} title="加入歌单">
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
