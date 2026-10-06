import { useMemo } from 'react';
import { api } from '@/api/client';
import type { RecentPlay, SongFact, StatisticsSnapshot } from '@/api/types';
import { composeStatistics, filtersActive, normalizeSnapshot } from './aggregate';
import { computeStatsView, EMPTY_FILTERS, type StatsFilters } from './crossfilter';

export { EMPTY_FILTERS, filtersActive };
export type { StatsFilters };

export async function fetchStatistics(): Promise<StatisticsSnapshot> {
  try {
    return normalizeSnapshot(await api.statisticsSnapshot());
  } catch {
    const [lib, hist] = await Promise.all([api.snapshot(), api.recentHistory(30)]);
    return composeStatistics(lib, hist);
  }
}

export function useFilteredStats(
  facts: SongFact[],
  plays: RecentPlay[],
  filters: StatsFilters,
  activityDays: 7 | 30,
) {
  return useMemo(
    () => computeStatsView(facts, plays, filters, activityDays),
    [facts, plays, filters, activityDays],
  );
}
