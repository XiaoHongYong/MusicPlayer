import { describe, expect, it } from 'vitest';
import type { SongFact } from '@/api/types';
import { chartSlice, computeStatsView, EMPTY_FILTERS } from './crossfilter';

function fact(partial: Partial<SongFact> & Pick<SongFact, 'id'>): SongFact {
  return {
    title: `t${partial.id}`,
    artist: 'A',
    album: 'X',
    genre: 'Rock',
    year: 2010,
    duration: 10,
    rating: 5,
    play_count: 1,
    ...partial,
  };
}

const facts: SongFact[] = [
  fact({ id: 1, artist: 'A', album: 'X', genre: 'Rock', year: 2010, rating: 5 }),
  fact({ id: 2, artist: 'A', album: 'Y', genre: 'Pop', year: 2010, rating: 4 }),
  fact({ id: 3, artist: 'B', album: 'X', genre: 'Rock', year: 2011, rating: 5 }),
];

describe('computeStatsView', () => {
  it('uses crossfilter groups for subcategory counts', () => {
    const view = computeStatsView(facts, [], EMPTY_FILTERS, 30);
    expect(view.filtered).toHaveLength(3);
    expect(view.artistOptions).toEqual([
      { key: 'A', count: 2 },
      { key: 'B', count: 1 },
    ]);
    expect(view.genreOptions).toEqual([
      { key: 'Pop', count: 1 },
      { key: 'Rock', count: 2 },
    ]);
    expect(view.ratingOptions.find((r) => r.key === '5')?.count).toBe(2);
  });

  it('artist counts follow other filters, not the artist filter itself', () => {
    const rock = computeStatsView(facts, [], { ...EMPTY_FILTERS, genre: 'Rock' }, 30);
    expect(rock.filtered.map((s) => s.id).sort()).toEqual([1, 3]);
    expect(rock.artistOptions).toEqual([
      { key: 'A', count: 1 },
      { key: 'B', count: 1 },
    ]);
    expect(rock.genreOptions).toEqual([
      { key: 'Pop', count: 1 },
      { key: 'Rock', count: 2 },
    ]);
    expect(rock.albumOptions).toEqual([{ key: 'X', count: 2 }]);
  });
});

describe('chartSlice', () => {
  it('keeps the selected key even when outside the top n', () => {
    const rows = [
      { key: 'a', count: 9 },
      { key: 'b', count: 8 },
      { key: 'c', count: 1 },
    ];
    expect(chartSlice(rows, 'c', 2).map((r) => r.key)).toEqual(['a', 'b', 'c']);
  });
});
