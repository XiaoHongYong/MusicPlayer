import type { CountRow, StatsFilters } from './crossfilter';

export function cssHslToCanvas(raw: string) {
  const m = raw.trim().match(/^(-?[\d.]+)\s+([\d.]+)%\s+([\d.]+)%/);
  if (!m) return `hsl(${raw.trim()})`;
  return `hsl(${m[1]}, ${m[2]}%, ${m[3]}%)`;
}

export function liftHsl(color: string, delta: number) {
  const m = color.match(/hsl\(\s*([\d.]+),\s*([\d.]+)%,\s*([\d.]+)%\s*\)/);
  if (!m) return color;
  const l = Math.max(8, Math.min(88, Number(m[3]) + delta));
  return `hsl(${m[1]}, ${m[2]}%, ${l}%)`;
}

export type ChartClickEvent = {
  componentType?: string;
  data?: unknown;
  dataIndex?: number;
  name?: string;
};

export function parseChartClick(e: ChartClickEvent): { dim: keyof StatsFilters; key: string } | null {
  if (e.componentType && e.componentType !== 'series' && e.componentType !== 'markPoint') return null;
  const data = e.data;
  if (data && typeof data === 'object') {
    const item = data as { dim?: keyof StatsFilters; key?: string; name?: string };
    const key = item.key ?? item.name;
    if (item.dim && key != null && key !== '') {
      return { dim: item.dim, key: String(key) };
    }
  }
  return null;
}

export function clickFilterFromEvent(
  e: ChartClickEvent,
  rows: CountRow[],
  dim: keyof StatsFilters,
): { dim: keyof StatsFilters; key: string } | null {
  const parsed = parseChartClick(e);
  if (parsed) return parsed;
  if (e.componentType && e.componentType !== 'series') return null;
  if (typeof e.dataIndex !== 'number') return null;
  const row = rows[e.dataIndex];
  return row ? { dim, key: row.key } : null;
}
