export const STATS_PAGE_SIZES = [10, 15, 20, 50, 100] as const;
export type StatsPageSize = (typeof STATS_PAGE_SIZES)[number];

export const STATS_PAGE_SIZE_KEY = 'pmc.statsPageSize';

export function parseStatsPageSize(raw: string | null | undefined, fallback: StatsPageSize = 20): StatsPageSize {
  const n = Number(raw);
  return (STATS_PAGE_SIZES as readonly number[]).includes(n) ? (n as StatsPageSize) : fallback;
}

export function loadStatsPageSize(): StatsPageSize {
  try {
    return parseStatsPageSize(localStorage.getItem(STATS_PAGE_SIZE_KEY));
  } catch {
    return 20;
  }
}

export function saveStatsPageSize(size: StatsPageSize) {
  try {
    localStorage.setItem(STATS_PAGE_SIZE_KEY, String(size));
  } catch {
    /* ignore */
  }
}

export function pageCount(total: number, pageSize: number) {
  if (total <= 0 || pageSize <= 0) return 1;
  return Math.max(1, Math.ceil(total / pageSize));
}

export function clampPage(page: number, total: number, pageSize: number) {
  const max = pageCount(total, pageSize);
  if (!Number.isFinite(page) || page < 1) return 1;
  return Math.min(page, max);
}

export function pageSlice<T>(items: T[], page: number, pageSize: number): T[] {
  const p = clampPage(page, items.length, pageSize);
  const start = (p - 1) * pageSize;
  return items.slice(start, start + pageSize);
}

export type PageToken = number | 'ellipsis';

/** Compact numbered pagination: 1 … 4 5 6 … 20 */
export function pageWindow(current: number, pages: number, sibling = 1): PageToken[] {
  const total = Math.max(1, pages);
  const page = Math.min(total, Math.max(1, Number.isFinite(current) ? current : 1));
  if (total <= 7) return Array.from({ length: total }, (_, i) => i + 1);

  const left = Math.max(2, page - sibling);
  const right = Math.min(total - 1, page + sibling);
  const tokens: PageToken[] = [1];
  if (left > 2) tokens.push('ellipsis');
  for (let i = left; i <= right; i += 1) {
    if (i !== 1 && i !== total) tokens.push(i);
  }
  if (right < total - 1) tokens.push('ellipsis');
  tokens.push(total);
  return tokens;
}
