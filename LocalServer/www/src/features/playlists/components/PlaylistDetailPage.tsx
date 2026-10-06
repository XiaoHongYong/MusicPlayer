import { useMemo, useState } from 'react';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { ArrowDown, ArrowUp, Play, Trash2 } from 'lucide-react';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import { Dialog } from '@/components/ui/dialog';
import type { Song } from '@/api/types';
import { useT } from '@/i18n';

export function PlaylistDetailPage() {
  const t = useT();
  const { id } = useParams();
  const playlistId = Number(id);
  const nav = useNavigate();
  const qc = useQueryClient();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const [rename, setRename] = useState('');
  const [addOpen, setAddOpen] = useState(false);
  const [q, setQ] = useState('');
  const { data, isLoading, error } = useQuery({
    queryKey: ['playlist', playlistId],
    queryFn: () => api.playlist(playlistId),
    enabled: Number.isFinite(playlistId),
  });
  const { data: snapshot } = useLibrarySnapshot();

  const patch = useMutation({
    mutationFn: () => api.patchPlaylist(playlistId, { name: rename.trim() }),
    onSuccess: () => {
      setRename('');
      void qc.invalidateQueries({ queryKey: ['playlist', playlistId] });
      void qc.invalidateQueries({ queryKey: ['playlists'] });
    },
  });
  const remove = useMutation({
    mutationFn: () => api.deletePlaylist(playlistId),
    onSuccess: () => {
      void qc.invalidateQueries({ queryKey: ['playlists'] });
      nav('/playlists');
    },
  });
  const removeSong = useMutation({
    mutationFn: (songId: number) => api.removePlaylistSong(playlistId, songId),
    onSuccess: () => void qc.invalidateQueries({ queryKey: ['playlist', playlistId] }),
  });
  const reorder = useMutation({
    mutationFn: (songIds: number[]) => api.reorderPlaylistSongs(playlistId, songIds),
    onSuccess: () => void qc.invalidateQueries({ queryKey: ['playlist', playlistId] }),
  });
  const addSongs = useMutation({
    mutationFn: (songIds: number[]) => api.addPlaylistSongs(playlistId, songIds),
    onSuccess: () => {
      setAddOpen(false);
      setQ('');
      void qc.invalidateQueries({ queryKey: ['playlist', playlistId] });
      void qc.invalidateQueries({ queryKey: ['playlists'] });
    },
  });

  const candidates = useMemo(() => {
    const existing = new Set((data?.songs ?? []).map((s) => s.id));
    const query = q.trim().toLowerCase();
    return (snapshot?.songs ?? []).filter((s) => {
      if (existing.has(s.id)) return false;
      if (!query) return true;
      return [s.title, s.artist, s.album].some((x) => x.toLowerCase().includes(query));
    });
  }, [snapshot, data, q]);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading playlists…')}</p>;
  if (error || !data) return <p className="p-8 text-red-500">{t('Playlist not found')}</p>;

  const move = (index: number, dir: -1 | 1) => {
    const next = [...data.songs];
    const j = index + dir;
    if (j < 0 || j >= next.length) return;
    const tmp = next[index];
    next[index] = next[j];
    next[j] = tmp;
    reorder.mutate(next.map((s) => s.id));
  };

  return (
    <div className="flex h-full flex-col gap-4 p-6">
      <div className="text-sm text-muted-foreground">
        <Link to="/playlists" className="hover:text-foreground">
          {t('Playlists')}
        </Link>
        <span className="mx-2">/</span>
        <span>{data.name}</span>
      </div>
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold">{data.name}</h1>
          <p className="text-sm text-muted-foreground">
            {t('{n} tracks · {duration}', { n: data.count, duration: formatDuration(data.duration) })}
          </p>
        </div>
        <div className="flex flex-wrap gap-2">
          <Button onClick={() => playSongs(data.songs, 0)} disabled={!data.songs.length}>
            <Play size={14} className="mr-1" />
            {t('Play')}
          </Button>
          <Button variant="outline" onClick={() => setAddOpen(true)}>
            {t('Add songs')}
          </Button>
          <Button variant="ghost" onClick={() => remove.mutate()}>
            <Trash2 size={14} className="mr-1" />
            {t('Delete')}
          </Button>
        </div>
      </div>
      <form
        className="flex max-w-md gap-2"
        onSubmit={(e) => {
          e.preventDefault();
          if (rename.trim()) patch.mutate();
        }}
      >
        <Input
          className="min-w-0 flex-1"
          placeholder={t('Rename…')}
          value={rename}
          onChange={(e) => setRename(e.target.value)}
        />
        <Button type="submit" variant="outline" disabled={!rename.trim()}>
          {t('Save')}
        </Button>
      </form>
      <div className="min-h-0 flex-1 overflow-auto">
        {data.songs.length === 0 ? (
          <p className="text-sm text-muted-foreground">{t('Empty playlist. Tap Add songs to pick tracks from the library.')}</p>
        ) : (
          data.songs.map((song, i) => (
            <SongRow
              key={song.id}
              song={song}
              index={i}
              total={data.songs.length}
              onPlay={() => playSongs(data.songs, i)}
              onRemove={() => removeSong.mutate(song.id)}
              onUp={() => move(i, -1)}
              onDown={() => move(i, 1)}
            />
          ))
        )}
      </div>
      <Dialog open={addOpen} title={t('Add songs')} onClose={() => setAddOpen(false)} className="max-w-2xl">
        <Input placeholder={t('Search…')} value={q} onChange={(e) => setQ(e.target.value)} className="mb-3" />
        <div className="max-h-[50vh] space-y-1 overflow-auto">
          {candidates.slice(0, 80).map((s) => (
            <button
              key={s.id}
              className="flex w-full items-center justify-between rounded-md px-2 py-2 text-left hover:bg-accent"
              onClick={() => addSongs.mutate([s.id])}
            >
              <span className="min-w-0 truncate text-sm">
                {s.title}
                <span className="ml-2 text-muted-foreground">{s.artist}</span>
              </span>
              <span className="text-xs text-muted-foreground">{formatDuration(s.duration)}</span>
            </button>
          ))}
        </div>
      </Dialog>
    </div>
  );
}

function SongRow({
  song,
  index,
  total,
  onPlay,
  onRemove,
  onUp,
  onDown,
}: {
  song: Song;
  index: number;
  total: number;
  onPlay: () => void;
  onRemove: () => void;
  onUp: () => void;
  onDown: () => void;
}) {
  return (
    <div className="grid grid-cols-[40px_1.4fr_1fr_80px_120px] items-center gap-2 rounded-md px-2 py-2 hover:bg-accent">
      <button onClick={onPlay} className="text-muted-foreground hover:text-foreground">
        <Play size={14} />
      </button>
      <button className="truncate text-left text-sm" onClick={onPlay}>
        {song.title}
      </button>
      <span className="truncate text-sm text-muted-foreground">{song.artist}</span>
      <span className="text-xs text-muted-foreground">{formatDuration(song.duration)}</span>
      <div className="flex justify-end gap-1">
        <Button variant="ghost" className="h-7 px-2" disabled={index === 0} onClick={onUp}>
          <ArrowUp size={14} />
        </Button>
        <Button variant="ghost" className="h-7 px-2" disabled={index === total - 1} onClick={onDown}>
          <ArrowDown size={14} />
        </Button>
        <Button variant="ghost" className="h-7 px-2" onClick={onRemove}>
          <Trash2 size={14} />
        </Button>
      </div>
    </div>
  );
}
