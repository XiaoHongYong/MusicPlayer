import { describe, expect, it } from 'vitest';
import type { Song } from '@/api/types';
import { compareSongs, initialSortDir, nextSongSort } from './songSort';

function song(partial: Partial<Song> & Pick<Song, 'id' | 'title'>): Song {
  return {
    artist: '',
    album: '',
    year: 0,
    genre: '',
    url: '',
    duration: 0,
    fileSize: 0,
    timeAdded: 0,
    timePlayed: 0,
    lyricsFile: '',
    rating: 0,
    format: '',
    play_count: 0,
    has_lyrics: false,
    bitRate: 0,
    channels: 0,
    bitsPerSample: 0,
    sampleRate: 0,
    ...partial,
  };
}

describe('nextSongSort', () => {
  it('starts text columns ascending and numeric descending', () => {
    expect(initialSortDir('title')).toBe('asc');
    expect(initialSortDir('plays')).toBe('desc');
    expect(nextSongSort({ key: 'title', dir: 'asc' }, 'plays')).toEqual({ key: 'plays', dir: 'desc' });
    expect(nextSongSort({ key: 'plays', dir: 'desc' }, 'plays')).toEqual({ key: 'plays', dir: 'asc' });
  });
});

describe('compareSongs', () => {
  it('sorts plays then title', () => {
    const a = song({ id: 1, title: 'A', play_count: 2 });
    const b = song({ id: 2, title: 'B', play_count: 9 });
    expect(compareSongs(a, b, { key: 'plays', dir: 'desc' })).toBeGreaterThan(0);
    expect(compareSongs(a, b, { key: 'title', dir: 'asc' })).toBeLessThan(0);
  });
});
