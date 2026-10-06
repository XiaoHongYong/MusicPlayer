import type { HistoryRecent, LibrarySnapshot, RecentPlay, StatisticsSnapshot } from '@/api/types';

export function lastNDates(n: number, end = new Date()): string[] {
  const dates: string[] = [];
  const day = Date.UTC(end.getUTCFullYear(), end.getUTCMonth(), end.getUTCDate());
  for (let i = n - 1; i >= 0; i -= 1) {
    dates.push(new Date(day - i * 86400000).toISOString().slice(0, 10));
  }
  return dates;
}

export function dailyActivity(plays: RecentPlay[], allowedSongIds: Set<number> | null, days: number): { date: string; count: number }[] {
  const dates = lastNDates(days);
  const map = new Map(dates.map((d) => [d, 0]));
  for (const p of plays) {
    if (!map.has(p.date)) continue;
    if (allowedSongIds && !allowedSongIds.has(p.song_id)) continue;
    map.set(p.date, (map.get(p.date) ?? 0) + p.count);
  }
  return dates.map((date) => ({ date, count: map.get(date) ?? 0 }));
}

export function toggleFilterValue(current: string, next: string): string {
  return current === next ? '' : next;
}

export function filtersActive(filters: { artist: string; album: string; genre: string; rating: string; year: string }) {
  return Boolean(filters.artist || filters.album || filters.genre || filters.rating || filters.year);
}

export function composeStatistics(lib: LibrarySnapshot, hist: HistoryRecent): StatisticsSnapshot {
  const song_facts = lib.songs.map((s) => ({
    id: s.id,
    title: s.title,
    artist: s.artist || 'Unknown',
    album: s.album || 'Unknown',
    genre: s.genre || 'Unknown',
    year: s.year || 0,
    duration: s.duration,
    rating: s.rating,
    play_count: s.play_count,
  }));
  const recent_plays: RecentPlay[] = [];
  for (const day of hist.days) {
    for (const item of day.items) {
      recent_plays.push({ date: day.date, song_id: item.song_id, count: item.count });
    }
  }
  return {
    version: lib.version,
    generated_at: lib.generated_at,
    song_facts,
    recent_plays,
  };
}

export function normalizeSnapshot(raw: StatisticsSnapshot): StatisticsSnapshot {
  const song_facts = (raw.song_facts ?? []).map((s) => ({
    ...s,
    artist: s.artist || 'Unknown',
    album: s.album || 'Unknown',
    genre: s.genre || 'Unknown',
  }));
  return {
    ...raw,
    song_facts,
    recent_plays: raw.recent_plays ?? [],
  };
}
