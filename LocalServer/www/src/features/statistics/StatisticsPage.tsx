import { useCallback, useEffect, useMemo, useState } from 'react';
import ReactECharts from 'echarts-for-react';
import { useQuery } from '@tanstack/react-query';
import type { Song } from '@/api/types';
import { TablePagination } from '@/components/TablePagination';
import { formatDuration } from '@/lib/utils';
import { FilterSelect } from '@/features/library/components/FilterSelect';
import { RatingStars } from '@/features/library/components/RatingStars';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from '@/features/library/components/MediaContextMenu';
import { useLibrarySnapshot } from '@/features/library/components/SongsPage';
import { usePlayerStore } from '@/features/player/store';
import { useT } from '@/i18n';
import { toggleFilterValue } from './aggregate';
import {
  clickFilterFromEvent,
  cssHslToCanvas,
  liftHsl,
  type ChartClickEvent,
} from './chartClick';
import type { CountRow } from './crossfilter';
import { EMPTY_FILTERS, fetchStatistics, filtersActive, useFilteredStats, type StatsFilters } from './useStatistics';
import {
  clampPage,
  loadStatsPageSize,
  pageSlice,
  saveStatsPageSize,
  type StatsPageSize,
} from './pagination';

function useChartColors() {
  const [colors, setColors] = useState({
    text: 'hsl(240, 5%, 64%)',
    primary: 'hsl(263, 70%, 66%)',
    emphasis: 'hsl(263, 70%, 80%)',
    muted: 'hsl(240, 5%, 18%)',
  });
  useEffect(() => {
    const read = () => {
      const s = getComputedStyle(document.documentElement);
      const token = (name: string) => cssHslToCanvas(s.getPropertyValue(name));
      const primary = token('--primary');
      setColors({
        text: token('--muted-foreground'),
        primary,
        emphasis: liftHsl(primary, 16),
        muted: token('--border'),
      });
    };
    read();
    const obs = new MutationObserver(read);
    obs.observe(document.documentElement, { attributes: true, attributeFilter: ['class', 'data-accent'] });
    return () => obs.disconnect();
  }, []);
  return colors;
}

function withCount(name: string, n: number) {
  return `${name} (${n})`;
}

function barItems(rows: CountRow[], selected: string, dim: keyof StatsFilters) {
  return rows.map((r) => ({
    value: r.count,
    name: r.key,
    key: r.key,
    dim,
    itemStyle: selected && r.key !== selected ? { opacity: 0.35 } : undefined,
  }));
}

export function StatisticsPage() {
  const t = useT();
  const colors = useChartColors();
  const playSongs = usePlayerStore((s) => s.playSongs);
  const { data: library } = useLibrarySnapshot();
  const songMap = useMemo(() => new Map((library?.songs ?? []).map((s) => [s.id, s])), [library]);
  const [filters, setFilters] = useState<StatsFilters>(EMPTY_FILTERS);
  const [activityDays, setActivityDays] = useState<7 | 30>(30);
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const [pageSize, setPageSize] = useState<StatsPageSize>(loadStatsPageSize);
  const [page, setPage] = useState(1);
  const { data, isLoading, error } = useQuery({
    queryKey: ['statistics-snapshot'],
    queryFn: fetchStatistics,
    staleTime: 30_000,
  });
  const facts = data?.song_facts ?? [];
  const plays = data?.recent_plays ?? [];
  const view = useFilteredStats(facts, plays, filters, activityDays);
  const artistChartRows = useMemo(() => [...view.artistChart].reverse(), [view.artistChart]);
  const axis = useMemo(
    () => ({ axisLabel: { color: colors.text }, axisLine: { lineStyle: { color: colors.muted } } }),
    [colors],
  );
  const barVisual = useMemo(
    () => ({
      cursor: 'pointer' as const,
      itemStyle: { color: colors.primary },
      emphasis: {
        focus: 'none' as const,
        itemStyle: { color: colors.emphasis, opacity: 1, shadowBlur: 0 },
      },
      blur: { itemStyle: { opacity: 1, color: colors.primary } },
    }),
    [colors],
  );

  const playable = useMemo(
    () => view.filtered.map((row) => songMap.get(row.id)).filter((row): row is Song => !!row),
    [view.filtered, songMap],
  );

  useEffect(() => {
    setPage(1);
  }, [filters, activityDays, playable.length]);

  const applyChartFilter = useCallback((e: ChartClickEvent, rows: CountRow[], dim: keyof StatsFilters) => {
    const hit = clickFilterFromEvent(e, rows, dim);
    if (!hit) return;
    setFilters((prev) => ({ ...prev, [hit.dim]: toggleFilterValue(prev[hit.dim], hit.key) }));
  }, []);

  const onArtistClick = useCallback((e: ChartClickEvent) => applyChartFilter(e, artistChartRows, 'artist'), [applyChartFilter, artistChartRows]);
  const onGenreClick = useCallback((e: ChartClickEvent) => applyChartFilter(e, view.genreChart, 'genre'), [applyChartFilter, view.genreChart]);
  const onRatingClick = useCallback((e: ChartClickEvent) => applyChartFilter(e, view.ratingChart, 'rating'), [applyChartFilter, view.ratingChart]);
  const onYearClick = useCallback((e: ChartClickEvent) => applyChartFilter(e, view.yearChart, 'year'), [applyChartFilter, view.yearChart]);

  if (isLoading) return <p className="p-8 text-muted-foreground">{t('Loading statistics…')}</p>;
  if (error) return <p className="p-8 text-red-500">{t('Failed to load statistics')}</p>;

  const currentPage = clampPage(page, playable.length, pageSize);
  const rows = pageSlice(playable, currentPage, pageSize);

  const setFilter = (key: keyof StatsFilters, value: string) => {
    setFilters((prev) => ({ ...prev, [key]: value }));
  };
  const yearLabel = (key: string) => (key === '0' ? t('Unknown') : key);
  const ratingLabel = (key: string) => (key === '0' ? t('Unrated') : t('{n} stars', { n: Number(key) }));
  const artistAll = view.artistOptions.reduce((s, r) => s + r.count, 0);
  const albumAll = view.albumOptions.reduce((s, r) => s + r.count, 0);
  const genreAll = view.genreOptions.reduce((s, r) => s + r.count, 0);
  const ratingAll = view.ratingOptions.reduce((s, r) => s + r.count, 0);
  const yearAll = view.yearOptions.reduce((s, r) => s + r.count, 0);

  return (
    <div className="space-y-6 p-6">
      <div>
        <h1 className="text-2xl font-semibold">{t('Statistics')}</h1>
        <p className="text-sm text-muted-foreground">{t('Click a chart to update the filters. The list below shows matching songs.')}</p>
      </div>

      <div className="flex flex-wrap gap-2">
        <FilterSelect aria-label={t('Listening Activity')} value={String(activityDays)} onChange={(v) => setActivityDays(v === '7' ? 7 : 30)}>
          <option value="7">{t('Last 7 days')}</option>
          <option value="30">{t('Last 30 days')}</option>
        </FilterSelect>
        <FilterSelect aria-label={t('Artist')} value={filters.artist} onChange={(v) => setFilter('artist', v)}>
          <option value="">{withCount(t('All artists'), artistAll)}</option>
          {view.artistOptions.map((a) => (
            <option key={a.key} value={a.key}>
              {withCount(a.key, a.count)}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Album')} value={filters.album} onChange={(v) => setFilter('album', v)}>
          <option value="">{withCount(t('All albums'), albumAll)}</option>
          {view.albumOptions.map((a) => (
            <option key={a.key} value={a.key}>
              {withCount(a.key, a.count)}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Genre')} value={filters.genre} onChange={(v) => setFilter('genre', v)}>
          <option value="">{withCount(t('All genres'), genreAll)}</option>
          {view.genreOptions.map((g) => (
            <option key={g.key} value={g.key}>
              {withCount(g.key, g.count)}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Rating')} value={filters.rating} onChange={(v) => setFilter('rating', v)}>
          <option value="">{withCount(t('All ratings'), ratingAll)}</option>
          {view.ratingOptions
            .filter((r) => r.count > 0 || r.key === filters.rating)
            .map((r) => (
            <option key={r.key} value={r.key}>
              {withCount(ratingLabel(r.key), r.count)}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Year')} value={filters.year} onChange={(v) => setFilter('year', v)}>
          <option value="">{withCount(t('All years'), yearAll)}</option>
          {view.yearOptions.map((y) => (
            <option key={y.key} value={y.key}>
              {withCount(yearLabel(y.key), y.count)}
            </option>
          ))}
        </FilterSelect>
        {filtersActive(filters) && (
          <button type="button" className="h-9 rounded-md px-3 text-sm text-muted-foreground hover:bg-accent hover:text-foreground" onClick={() => setFilters(EMPTY_FILTERS)}>
            {t('Clear filters')}
          </button>
        )}
      </div>

      <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
        {[
          [t('Tracks'), view.overview.songs],
          [t('Albums'), view.overview.albums],
          [t('Artists'), view.overview.artists],
          [t('Total plays'), view.overview.plays],
        ].map(([label, value]) => (
          <div key={label} className="rounded-xl bg-card p-4">
            <div className="text-xs text-muted-foreground">{label}</div>
            <div className="mt-1 text-2xl font-semibold tabular-nums">{value}</div>
          </div>
        ))}
      </div>

      <div className="rounded-xl bg-card p-4">
        <h2 className="mb-2 text-sm font-medium">{t('Listening Activity')}</h2>
        <ReactECharts
          style={{ height: 220 }}
          opts={{ renderer: 'canvas' }}
          notMerge
          option={{
            color: [colors.primary],
            tooltip: { trigger: 'axis' },
            grid: { left: 40, right: 16, top: 16, bottom: 32 },
            xAxis: { type: 'category', data: view.activity.map((d) => d.date.slice(5)), ...axis },
            yAxis: { type: 'value', min: 0, minInterval: 1, ...axis },
            series: [{ type: 'bar', data: view.activity.map((d) => d.count), name: t('Plays'), ...barVisual }],
          }}
        />
      </div>

      <div className="grid gap-4 lg:grid-cols-2">
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Artists')}</h2>
          <ReactECharts
            style={{ height: 280 }}
            opts={{ renderer: 'canvas' }}
            notMerge
            onEvents={{ click: onArtistClick }}
            option={{
              color: [colors.primary],
              tooltip: { trigger: 'item' },
              grid: { left: 108, right: 16, top: 8, bottom: 24 },
              xAxis: { type: 'value', minInterval: 1, ...axis },
              yAxis: {
                type: 'category',
                triggerEvent: true,
                data: artistChartRows.map((x) => x.key),
                ...axis,
              },
              series: [
                {
                  type: 'bar',
                  data: barItems(artistChartRows, filters.artist, 'artist'),
                  name: t('Tracks'),
                  ...barVisual,
                },
              ],
            }}
          />
        </div>
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Genre Distribution')}</h2>
          <ReactECharts
            style={{ height: 280 }}
            opts={{ renderer: 'canvas' }}
            notMerge
            onEvents={{ click: onGenreClick }}
            option={{
              tooltip: { trigger: 'item' },
              series: [
                {
                  type: 'pie',
                  radius: ['42%', '70%'],
                  cursor: 'pointer',
                  emphasis: { focus: 'none', itemStyle: { shadowBlur: 8, shadowColor: colors.primary } },
                  data: view.genreChart.map((r) => ({
                    value: r.count,
                    name: r.key,
                    key: r.key,
                    dim: 'genre' as const,
                    itemStyle: filters.genre && r.key !== filters.genre ? { opacity: 0.35 } : undefined,
                  })),
                },
              ],
            }}
          />
        </div>
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Rating Distribution')}</h2>
          <ReactECharts
            style={{ height: 240 }}
            opts={{ renderer: 'canvas' }}
            notMerge
            onEvents={{ click: onRatingClick }}
            option={{
              color: [colors.primary],
              tooltip: { trigger: 'item' },
              grid: { left: 40, right: 16, top: 16, bottom: 32 },
              xAxis: {
                type: 'category',
                triggerEvent: true,
                data: view.ratingChart.map((r) => ratingLabel(r.key)),
                ...axis,
              },
              yAxis: { type: 'value', minInterval: 1, ...axis },
              series: [{ type: 'bar', data: barItems(view.ratingChart, filters.rating, 'rating'), name: t('Tracks'), ...barVisual }],
            }}
          />
        </div>
        <div className="rounded-xl bg-card p-4">
          <h2 className="mb-2 text-sm font-medium">{t('Year')}</h2>
          <ReactECharts
            style={{ height: 240 }}
            opts={{ renderer: 'canvas' }}
            notMerge
            onEvents={{ click: onYearClick }}
            option={{
              color: [colors.primary],
              tooltip: { trigger: 'item' },
              grid: { left: 40, right: 16, top: 16, bottom: 32 },
              xAxis: {
                type: 'category',
                triggerEvent: true,
                data: view.yearChart.map((r) => yearLabel(r.key)),
                ...axis,
              },
              yAxis: { type: 'value', minInterval: 1, ...axis },
              series: [{ type: 'bar', data: barItems(view.yearChart, filters.year, 'year'), name: t('Tracks'), ...barVisual }],
            }}
          />
        </div>
      </div>

      <div className="rounded-xl bg-card p-4">
        <h2 className="mb-3 text-sm font-medium">{t('{n} tracks', { n: playable.length })}</h2>
        {playable.length === 0 ? (
          <p className="text-sm text-muted-foreground">{t('No matches found.')}</p>
        ) : (
          <>
            <div className="overflow-x-auto">
              <table className="w-full min-w-[640px] text-left text-sm">
                <thead className="text-xs uppercase text-muted-foreground">
                  <tr className="border-b border-border">
                    <th className="w-10 px-2 py-2 font-medium">#</th>
                    <th className="px-2 py-2 font-medium">{t('Title')}</th>
                    <th className="px-2 py-2 font-medium">{t('Artist')}</th>
                    <th className="hidden px-2 py-2 font-medium md:table-cell">{t('Album')}</th>
                    <th className="w-16 px-2 py-2 font-medium">{t('Duration')}</th>
                    <th className="w-24 px-2 py-2 font-medium">{t('Rating')}</th>
                    <th className="w-16 px-2 py-2 text-right font-medium">{t('Plays')}</th>
                  </tr>
                </thead>
                <tbody>
                  {rows.map((song, i) => {
                    const index = (currentPage - 1) * pageSize + i;
                    return (
                      <tr
                        key={song.id}
                        className="cursor-pointer border-b border-border/60 hover:bg-accent"
                        onClick={() => playSongs(playable, index)}
                        onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, [song]))}
                      >
                        <td className="px-2 py-2 text-xs text-muted-foreground">{index + 1}</td>
                        <td className="max-w-[14rem] truncate px-2 py-2">{song.title}</td>
                        <td className="max-w-[10rem] truncate px-2 py-2 text-muted-foreground">{song.artist}</td>
                        <td className="hidden max-w-[10rem] truncate px-2 py-2 text-muted-foreground md:table-cell">
                          {song.album}
                        </td>
                        <td className="px-2 py-2 text-xs text-muted-foreground">{formatDuration(song.duration)}</td>
                        <td className="px-2 py-2">
                          <RatingStars value={song.rating} />
                        </td>
                        <td className="px-2 py-2 text-right text-xs text-muted-foreground">{song.play_count}</td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
            <TablePagination
              total={playable.length}
              page={currentPage}
              pageSize={pageSize}
              onPageChange={setPage}
              onPageSizeChange={(size) => {
                setPageSize(size);
                saveStatsPageSize(size);
                setPage(1);
              }}
            />
          </>
        )}
      </div>
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
