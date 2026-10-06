import { useMemo } from 'react';
import ReactECharts from 'echarts-for-react';
import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { useT } from '@/i18n';

export function StatisticsPage() {
  const t = useT();
  const { data, isLoading } = useQuery({ queryKey: ['library-snapshot'], queryFn: api.snapshot });

  const charts = useMemo(() => {
    const songs = data?.songs ?? [];
    const byGenre = new Map<string, number>();
    const byArtist = new Map<string, number>();
    const ratingBuckets = [0, 0, 0, 0, 0, 0];
    for (const s of songs) {
      byGenre.set(s.genre || 'Unknown', (byGenre.get(s.genre || 'Unknown') ?? 0) + 1);
      byArtist.set(s.artist || 'Unknown', (byArtist.get(s.artist || 'Unknown') ?? 0) + 1);
      const bucket = Math.min(5, Math.max(0, Math.round(s.rating)));
      ratingBuckets[bucket] += 1;
    }
    const topArtists = [...byArtist.entries()].sort((a, b) => b[1] - a[1]).slice(0, 10);
    const genres = [...byGenre.entries()].sort((a, b) => b[1] - a[1]).slice(0, 10);
    return { topArtists, genres, ratingBuckets, total: songs.length };
  }, [data]);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading statistics…')}</p>;

  const axis = { axisLabel: { color: 'hsl(var(--muted-foreground))' } };

  return (
    <div className="space-y-6 p-6">
      <div>
        <h1 className="text-2xl font-semibold">{t('Statistics')}</h1>
        <p className="text-sm text-muted-foreground">
          {t('Aggregated from the library snapshot. {n} songs.', { n: charts.total })}
        </p>
      </div>
      <div className="grid gap-4 lg:grid-cols-2">
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Top Artists')}</h2>
          <ReactECharts
            style={{ height: 280 }}
            option={{
              tooltip: {},
              xAxis: { type: 'value', ...axis },
              yAxis: { type: 'category', data: charts.topArtists.map((x) => x[0]).reverse(), ...axis },
              series: [{ type: 'bar', data: charts.topArtists.map((x) => x[1]).reverse() }],
            }}
          />
        </div>
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Genre Distribution')}</h2>
          <ReactECharts
            style={{ height: 280 }}
            option={{
              tooltip: {},
              series: [
                {
                  type: 'pie',
                  radius: ['40%', '70%'],
                  data: charts.genres.map(([name, value]) => ({ name, value })),
                },
              ],
            }}
          />
        </div>
        <div className="rounded-xl bg-card p-4 lg:col-span-2">
          <h2 className="mb-2 text-sm font-medium">{t('Rating Distribution')}</h2>
          <ReactECharts
            style={{ height: 240 }}
            option={{
              tooltip: {},
              xAxis: { type: 'category', data: ['0', '1', '2', '3', '4', '5'], ...axis },
              yAxis: { type: 'value', ...axis },
              series: [{ type: 'bar', data: charts.ratingBuckets }],
            }}
          />
        </div>
      </div>
    </div>
  );
}
