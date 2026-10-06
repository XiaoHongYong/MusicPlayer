import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { formatDuration } from '@/lib/utils';
import { ListMusic } from 'lucide-react';
import { useT } from '@/i18n';

export function PlaylistsPage() {
  const t = useT();
  const qc = useQueryClient();
  const [name, setName] = useState('');
  const { data = [], isLoading, error } = useQuery({ queryKey: ['playlists'], queryFn: api.playlists });
  const create = useMutation({
    mutationFn: () => api.createPlaylist(name.trim()),
    onSuccess: () => {
      setName('');
      void qc.invalidateQueries({ queryKey: ['playlists'] });
    },
  });

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading playlists…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load playlists')}</p>;

  return (
    <div className="space-y-6 p-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-semibold">{t('Playlists')}</h1>
      </div>
      <form
        className="flex max-w-md gap-2"
        onSubmit={(e) => {
          e.preventDefault();
          if (name.trim()) create.mutate();
        }}
      >
        <Input
          className="min-w-0 flex-1"
          placeholder={t('New playlist name')}
          value={name}
          onChange={(e) => setName(e.target.value)}
        />
        <Button type="submit" disabled={!name.trim() || create.isPending}>
          {t('Create')}
        </Button>
      </form>
      {data.length === 0 ? (
        <p className="text-sm text-muted-foreground">{t('No playlists yet. Create one, then add songs from Songs.')}</p>
      ) : (
        <div className="grid grid-cols-1 gap-3 md:grid-cols-2 lg:grid-cols-3">
          {data.map((p) => (
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
                  {t('{n} tracks · {duration}', { n: p.count, duration: formatDuration(p.duration) })}
                </div>
              </div>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
}
