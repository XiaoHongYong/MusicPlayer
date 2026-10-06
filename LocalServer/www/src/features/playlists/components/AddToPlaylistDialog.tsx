import { useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { Dialog } from '@/components/ui/dialog';
import { Input } from '@/components/ui/input';
import type { Song } from '@/api/types';
import { useT } from '@/i18n';

export function AddToPlaylistDialog({
  songs,
  open,
  onClose,
}: {
  songs: Song[];
  open: boolean;
  onClose: () => void;
}) {
  const t = useT();
  const qc = useQueryClient();
  const [name, setName] = useState('');
  const { data: playlists = [] } = useQuery({
    queryKey: ['playlists'],
    queryFn: api.playlists,
    enabled: open,
  });
  const add = useMutation({
    mutationFn: async (playlistId: number) => {
      await api.addPlaylistSongs(
        playlistId,
        songs.map((s) => s.id),
      );
    },
    onSuccess: () => {
      void qc.invalidateQueries({ queryKey: ['playlists'] });
      onClose();
    },
  });
  const create = useMutation({
    mutationFn: async () => {
      const pl = await api.createPlaylist(name.trim());
      await api.addPlaylistSongs(
        pl.id,
        songs.map((s) => s.id),
      );
    },
    onSuccess: () => {
      setName('');
      void qc.invalidateQueries({ queryKey: ['playlists'] });
      onClose();
    },
  });

  return (
    <Dialog open={open} title={t('Add to playlist')} onClose={onClose}>
      <div className="space-y-3">
        <p className="text-sm text-muted-foreground">
          {t('Add {n} songs to an existing playlist, or create a new one.', { n: songs.length })}
        </p>
        <div className="flex gap-2">
          <Input
            className="min-w-0 flex-1"
            placeholder={t('New playlist name')}
            value={name}
            onChange={(e) => setName(e.target.value)}
          />
          <Button disabled={!name.trim() || create.isPending} onClick={() => create.mutate()}>
            {t('New')}
          </Button>
        </div>
        <div className="max-h-64 space-y-1 overflow-auto">
          {playlists.length === 0 && <p className="text-sm text-muted-foreground">{t('No playlists yet')}</p>}
          {playlists.map((p) => (
            <button
              key={p.id}
              className="flex w-full items-center justify-between rounded-md px-2 py-2 text-left hover:bg-accent"
              onClick={() => add.mutate(p.id)}
            >
              <span className="text-sm">{p.name}</span>
              <span className="text-xs text-muted-foreground">{t('{n} tracks', { n: p.count })}</span>
            </button>
          ))}
        </div>
      </div>
    </Dialog>
  );
}
