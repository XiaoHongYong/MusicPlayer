import crossfilter from 'crossfilter2';
import type { RecentPlay, SongFact } from '@/api/types';
import { dailyActivity, filtersActive } from './aggregate';

export type StatsFilters = {
  artist: string;
  album: string;
  genre: string;
  rating: string;
  year: string;
};

export const EMPTY_FILTERS: StatsFilters = {
  artist: '',
  album: '',
  genre: '',
  rating: '',
  year: '',
};

export type CountRow = { key: string; count: number };

function applyFilters(
  dims: {
    artist: { filterExact: (v: string) => unknown; filterAll: () => unknown };
    album: { filterExact: (v: string) => unknown; filterAll: () => unknown };
    genre: { filterExact: (v: string) => unknown; filterAll: () => unknown };
    rating: { filterExact: (v: number) => unknown; filterAll: () => unknown };
    year: { filterExact: (v: number) => unknown; filterAll: () => unknown };
  },
  filters: StatsFilters,
) {
  if (filters.artist) dims.artist.filterExact(filters.artist);
  else dims.artist.filterAll();
  if (filters.album) dims.album.filterExact(filters.album);
  else dims.album.filterAll();
  if (filters.genre) dims.genre.filterExact(filters.genre);
  else dims.genre.filterAll();
  if (filters.rating !== '') dims.rating.filterExact(Number(filters.rating));
  else dims.rating.filterAll();
  if (filters.year !== '') dims.year.filterExact(Number(filters.year));
  else dims.year.filterAll();
}

function namedRows(
  group: { all: () => ReadonlyArray<{ key: unknown; value: unknown }> },
  selected: string,
): CountRow[] {
  return group
    .all()
    .map((d) => ({ key: String(d.key), count: Number(d.value) }))
    .filter((d) => d.count > 0 || d.key === selected)
    .sort((a, b) => a.key.localeCompare(b.key));
}

function ratingRows(group: { all: () => ReadonlyArray<{ key: unknown; value: unknown }> }): CountRow[] {
  const map = new Map<number, number>();
  for (const d of group.all()) map.set(Number(d.key), Number(d.value));
  return [0, 1, 2, 3, 4, 5].map((n) => ({ key: String(n), count: map.get(n) ?? 0 }));
}

export function chartSlice(rows: CountRow[], selected: string, limit: number): CountRow[] {
  const sorted = [...rows].sort((a, b) => b.count - a.count || a.key.localeCompare(b.key));
  const top = sorted.filter((r) => r.count > 0).slice(0, limit);
  if (selected && !top.some((r) => r.key === selected)) {
    const extra = rows.find((r) => r.key === selected);
    if (extra) top.push(extra);
  }
  return top;
}

export function computeStatsView(
  facts: SongFact[],
  plays: RecentPlay[],
  filters: StatsFilters,
  activityDays: 7 | 30,
) {
  const ndx = crossfilter(facts);
  const dimArtist = ndx.dimension((d: SongFact) => d.artist);
  const dimAlbum = ndx.dimension((d: SongFact) => d.album);
  const dimGenre = ndx.dimension((d: SongFact) => d.genre);
  const dimRating = ndx.dimension((d: SongFact) => Math.min(5, Math.max(0, Math.round(d.rating))));
  const dimYear = ndx.dimension((d: SongFact) => d.year || 0);

  const artistGroup = dimArtist.group().reduceCount();
  const albumGroup = dimAlbum.group().reduceCount();
  const genreGroup = dimGenre.group().reduceCount();
  const ratingGroup = dimRating.group().reduceCount();
  const yearGroup = dimYear.group().reduceCount();

  applyFilters({ artist: dimArtist, album: dimAlbum, genre: dimGenre, rating: dimRating, year: dimYear }, filters);

  const filtered = (ndx.allFiltered() as SongFact[]).slice().sort((a, b) => {
    return b.play_count - a.play_count || a.title.localeCompare(b.title);
  });
  const ids = new Set(filtered.map((s) => s.id));
  const albums = new Set(filtered.map((s) => `${s.artist}::${s.album}`));
  const artists = new Set(filtered.map((s) => s.artist));
  let totalPlays = 0;
  for (const s of filtered) totalPlays += s.play_count;

  const artistOptions = namedRows(artistGroup, filters.artist);
  const albumOptions = namedRows(albumGroup, filters.album);
  const genreOptions = namedRows(genreGroup, filters.genre);
  const ratingOptions = ratingRows(ratingGroup);
  const yearOptions = namedRows(yearGroup, filters.year);

  return {
    filtered,
    overview: {
      songs: filtered.length,
      albums: albums.size,
      artists: artists.size,
      plays: totalPlays,
    },
    artistOptions,
    albumOptions,
    genreOptions,
    ratingOptions,
    yearOptions,
    artistChart: chartSlice(artistOptions, filters.artist, 12),
    genreChart: chartSlice(genreOptions, filters.genre, 12),
    ratingChart: ratingOptions,
    yearChart: chartSlice(yearOptions, filters.year, 16),
    activity: dailyActivity(plays, filtersActive(filters) ? ids : null, activityDays),
  };
}
