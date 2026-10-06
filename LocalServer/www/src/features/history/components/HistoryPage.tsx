import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { CoverImage } from '@/components/CoverImage';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import { usePlayerStore } from '@/features/player/store';

export function HistoryPage() {
  const { data, isLoading, error } = useQuery({
    queryKey: ['history-recent'],
    queryFn: () => api.recentHistory(30),
  });
  const { data: snapshot } = useLibrarySnapshot();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const songMap = new Map((snapshot?.songs ?? []).map((s) => [s.id, s]));

  if (isLoading) return <p className="p-8 text-muted-foreground">加载播放历史…</p>;
  if (error) return <p className="p-8 text-red-500">无法加载播放历史</p>;

  const days = data?.days ?? [];

  return (
    <div className="space-y-8 p-6">
      <div>
        <h1 className="text-2xl font-semibold">History</h1>
        <p className="mt-1 text-sm text-muted-foreground">近 30 天有效播放（满约 10 秒或 20% 进度后上报）</p>
      </div>
      {days.length === 0 ? (
        <p className="text-sm text-muted-foreground">还没有记录。在浏览器里听一会儿歌就会出现在这里。</p>
      ) : (
        days.map((day) => (
          <section key={day.date}>
            <h2 className="mb-3 text-sm font-medium text-muted-foreground">{day.date}</h2>
            <div className="space-y-1">
              {day.items.map((item) => {
                const song = songMap.get(item.song_id);
                return (
                  <button
                    key={`${day.date}-${item.song_id}`}
                    className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
                    onClick={() => song && playSongs([song], 0)}
                    disabled={!song}
                  >
                    <CoverImage songId={song?.id} className="h-10 w-10 rounded" />
                    <div className="min-w-0 flex-1">
                      <div className="truncate text-sm">{song?.title ?? `歌曲 #${item.song_id}`}</div>
                      <div className="truncate text-xs text-muted-foreground">{song?.artist ?? '—'}</div>
                    </div>
                    <span className="text-xs text-muted-foreground">{item.count} 次</span>
                  </button>
                );
              })}
            </div>
          </section>
        ))
      )}
    </div>
  );
}
