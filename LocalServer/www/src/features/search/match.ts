export type SearchTab = 'songs' | 'albums' | 'artists' | 'playlists';

export const SEARCH_TABS: SearchTab[] = ['songs', 'albums', 'artists', 'playlists'];

export function parseSearchTab(raw: string | null | undefined): SearchTab {
  if (raw === 'albums' || raw === 'artists' || raw === 'playlists') return raw;
  return 'songs';
}

export function searchHref(q: string, tab: SearchTab = 'songs'): string {
  const sp = new URLSearchParams();
  const trimmed = q.trim();
  if (trimmed) sp.set('q', trimmed);
  if (tab !== 'songs') sp.set('tab', tab);
  const qs = sp.toString();
  return qs ? `/search?${qs}` : '/search';
}

export function normalizeQuery(q: string) {
  return q.trim().toLowerCase();
}

export function textMatches(q: string, ...fields: Array<string | number | undefined | null>) {
  if (!q) return true;
  return fields.some((f) => String(f ?? '').toLowerCase().includes(q));
}

export function songMatchesQuery(
  song: { title: string; artist: string; album: string; genre?: string },
  rawQuery: string,
) {
  const q = normalizeQuery(rawQuery);
  if (!q) return true;
  return textMatches(q, song.title, song.artist, song.album, song.genre);
}
