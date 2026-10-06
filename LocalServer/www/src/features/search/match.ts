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
