import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { CoverImage } from '@/components/CoverImage';
import { usePlayerStore } from '@/features/player/store';

export function HomePage() {
  const { data: boot } = useQuery({ queryKey: ['bootstrap'], queryFn: api.bootstrap });
  const { data: snapshot } = useQuery({
    queryKey: ['library-snapshot'],
    queryFn: api.snapshot,
  });
  const playSongs = usePlayerStore((s) => s.playSongs);
  const recent = [...(snapshot?.songs ?? [])]
    .sort((a, b) => b.timeAdded - a.timeAdded)
    .slice(0, 8);
  const top = [...(snapshot?.songs ?? [])]
    .sort((a, b) => b.play_count - a.play_count)
    .slice(0, 8);

  return (
    <div className="space-y-10 p-6">
      <div>
        <p className="mt-1 text-muted-foreground">
          {boot?.library.song_count ?? '—'} 首歌曲 · {boot?.library.album_count ?? '—'} 张专辑
        </p>
      </div>
      <section>
        <h2 className="mb-4 text-lg font-medium">最近添加</h2>
        <div className="grid grid-cols-2 gap-4 md:grid-cols-4">
          {recent.map((s) => (
            <button key={s.id} className="text-left" onClick={() => playSongs(recent, recent.indexOf(s))}>
              <CoverImage songId={s.id} className="aspect-square w-full rounded-lg" />
              <div className="mt-2 truncate text-sm">{s.title}</div>
              <div className="truncate text-xs text-muted-foreground">{s.artist}</div>
            </button>
          ))}
        </div>
      </section>
      <section>
        <h2 className="mb-4 text-lg font-medium">常听</h2>
        <div className="space-y-1">
          {top.map((s, i) => (
            <button
              key={s.id}
              className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
              onClick={() => playSongs(top, i)}
            >
              <span className="w-6 text-xs text-muted-foreground">{i + 1}</span>
              <CoverImage songId={s.id} className="h-10 w-10 rounded" />
              <div className="min-w-0 flex-1">
                <div className="truncate text-sm">{s.title}</div>
                <div className="truncate text-xs text-muted-foreground">{s.artist}</div>
              </div>
              <span className="text-xs text-muted-foreground">{s.play_count} 次</span>
            </button>
          ))}
        </div>
      </section>
    </div>
  );
}
