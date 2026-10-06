import { describe, expect, it } from 'vitest';
import { composeStatistics, dailyActivity, lastNDates, toggleFilterValue } from './aggregate';
import type { LibrarySnapshot } from '@/api/types';

describe('lastNDates', () => {
  it('returns UTC calendar dates ending today', () => {
    const end = new Date('2026-10-06T15:00:00Z');
    expect(lastNDates(3, end)).toEqual(['2026-10-04', '2026-10-05', '2026-10-06']);
  });
});

describe('dailyActivity', () => {
  it('fills missing days and respects song filter', () => {
    const end = new Date('2026-10-06T12:00:00Z');
    const dates = lastNDates(3, end);
    const plays = [
      { date: dates[0], song_id: 1, count: 2 },
      { date: dates[2], song_id: 1, count: 1 },
      { date: dates[2], song_id: 2, count: 4 },
    ];
    expect(dailyActivity(plays, null, 3).map((d) => d.count)).toEqual([2, 0, 5]);
    expect(dailyActivity(plays, new Set([1]), 3).map((d) => d.count)).toEqual([2, 0, 1]);
  });
});

describe('toggleFilterValue', () => {
  it('clears when clicking the same value', () => {
    expect(toggleFilterValue('Rock', 'Rock')).toBe('');
    expect(toggleFilterValue('', 'Rock')).toBe('Rock');
  });
});

describe('composeStatistics', () => {
  it('maps library songs and history into facts', () => {
    const lib: LibrarySnapshot = {
      version: 3,
      generated_at: '2026-10-06T00:00:00Z',
      artists: [],
      albums: [],
      genres: [],
      songs: [
        {
          id: 9,
          artist: '',
          album: 'A',
          title: 'T',
          year: 0,
          genre: '',
          url: '',
          duration: 10,
          fileSize: 0,
          timeAdded: 0,
          timePlayed: 0,
          lyricsFile: '',
          rating: 4,
          format: 'mp3',
          play_count: 3,
          has_lyrics: false,
          bitRate: 0,
          channels: 0,
          bitsPerSample: 0,
          sampleRate: 0,
        },
      ],
    };
    const snap = composeStatistics(lib, {
      days: [{ date: '2026-10-06', items: [{ song_id: 9, count: 2, last_played_at: '' }] }],
    });
    expect(snap.song_facts[0].artist).toBe('Unknown');
    expect(snap.song_facts[0].genre).toBe('Unknown');
    expect(snap.recent_plays).toEqual([{ date: '2026-10-06', song_id: 9, count: 2 }]);
  });
});
