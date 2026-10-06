import type { Song } from '@/api/types';

export const SONG_SORT_KEYS = ['title', 'artist', 'album', 'duration', 'plays', 'rating', 'lyrics'] as const;
export type SongSortKey = (typeof SONG_SORT_KEYS)[number];
export type SortDir = 'asc' | 'desc';
export type SongSort = { key: SongSortKey; dir: SortDir };

const NUMERIC = new Set<SongSortKey>(['duration', 'plays', 'rating', 'lyrics']);

export function initialSortDir(key: SongSortKey): SortDir {
  return NUMERIC.has(key) ? 'desc' : 'asc';
}

export function nextSongSort(current: SongSort, clicked: SongSortKey): SongSort {
  if (current.key !== clicked) return { key: clicked, dir: initialSortDir(clicked) };
  return { key: clicked, dir: current.dir === 'asc' ? 'desc' : 'asc' };
}

function cmp(a: string | number, b: string | number) {
  if (typeof a === 'string' && typeof b === 'string') return a.localeCompare(b);
  return Number(a) - Number(b);
}

export function compareSongs(a: Song, b: Song, sort: SongSort) {
  let c = 0;
  switch (sort.key) {
    case 'title':
      c = cmp(a.title, b.title);
      break;
    case 'artist':
      c = cmp(a.artist, b.artist);
      break;
    case 'album':
      c = cmp(a.album, b.album);
      break;
    case 'duration':
      c = cmp(a.duration, b.duration);
      break;
    case 'plays':
      c = cmp(a.play_count, b.play_count);
      break;
    case 'rating':
      c = cmp(a.rating, b.rating);
      break;
    case 'lyrics':
      c = cmp(Number(a.has_lyrics), Number(b.has_lyrics));
      break;
  }
  if (sort.dir === 'desc') c = -c;
  return c || cmp(a.title, b.title) || a.id - b.id;
}
