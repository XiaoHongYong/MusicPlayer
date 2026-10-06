import { describe, expect, it } from 'vitest';
import { parseSearchTab, searchHref, songMatchesQuery } from './match';

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

describe('search tabs', () => {
  it('parses known tabs and falls back to songs', () => {
    expect(parseSearchTab('albums')).toBe('albums');
    expect(parseSearchTab('artists')).toBe('artists');
    expect(parseSearchTab('playlists')).toBe('playlists');
    expect(parseSearchTab('songs')).toBe('songs');
    expect(parseSearchTab('nope')).toBe('songs');
    expect(parseSearchTab(null)).toBe('songs');
  });

  it('builds search urls with optional tab', () => {
    expect(searchHref('  hello  ')).toBe('/search?q=hello');
    expect(searchHref('hello', 'albums')).toBe('/search?q=hello&tab=albums');
    expect(searchHref('fav', 'playlists')).toBe('/search?q=fav&tab=playlists');
    expect(searchHref('', 'artists')).toBe('/search?tab=artists');
    expect(searchHref('', 'songs')).toBe('/search');
  });
});
