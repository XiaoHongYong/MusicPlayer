import { describe, expect, it } from 'vitest';
import { songMatchesQuery } from './match';

describe('songMatchesQuery', () => {
  const song = { title: '晴天', artist: '周杰伦', album: '叶惠美', genre: 'Pop' };

  it('matches title artist album genre', () => {
    expect(songMatchesQuery(song, '晴')).toBe(true);
    expect(songMatchesQuery(song, 'jay')).toBe(false);
    expect(songMatchesQuery(song, '周杰')).toBe(true);
    expect(songMatchesQuery(song, '叶惠')).toBe(true);
    expect(songMatchesQuery(song, 'pop')).toBe(true);
  });

  it('empty query matches all', () => {
    expect(songMatchesQuery(song, '  ')).toBe(true);
  });
});
