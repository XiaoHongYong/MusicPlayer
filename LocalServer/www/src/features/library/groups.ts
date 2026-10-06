import type { Song } from '@/api/types';

export interface AlbumGroup {
  name: string;
  artist: string;
  year: number;
  genre: string;
  count: number;
  plays: number;
  coverId: number;
}

export interface ArtistGroup {
  name: string;
  count: number;
  albums: number;
  plays: number;
  coverId: number;
}

export interface GenreGroup {
  name: string;
  count: number;
  plays: number;
  topArtist: string;
  coverId: number;
}

export function groupAlbums(songs: Song[]): AlbumGroup[] {
  const map = new Map<string, AlbumGroup>();
  for (const s of songs) {
    const key = `${s.album}::${s.artist}`;
    const cur = map.get(key);
    if (cur) {
      cur.count += 1;
      cur.plays += s.play_count;
      if (s.year > cur.year) cur.year = s.year;
    } else {
      map.set(key, {
        name: s.album || 'Unknown',
        artist: s.artist || 'Unknown',
        year: s.year || 0,
        genre: s.genre || 'Unknown',
        count: 1,
        plays: s.play_count,
        coverId: s.id,
      });
    }
  }
  return [...map.values()];
}

export function groupArtists(songs: Song[]): ArtistGroup[] {
  const map = new Map<string, { name: string; count: number; albums: Set<string>; plays: number; coverId: number }>();
  for (const s of songs) {
    const name = s.artist || 'Unknown';
    const cur = map.get(name);
    if (cur) {
      cur.count += 1;
      cur.plays += s.play_count;
      cur.albums.add(s.album);
    } else {
      map.set(name, { name, count: 1, albums: new Set([s.album]), plays: s.play_count, coverId: s.id });
    }
  }
  return [...map.values()].map((a) => ({
    name: a.name,
    count: a.count,
    albums: a.albums.size,
    plays: a.plays,
    coverId: a.coverId,
  }));
}

export function groupGenres(songs: Song[]): GenreGroup[] {
  const map = new Map<
    string,
    { name: string; count: number; plays: number; artists: Map<string, number>; coverId: number }
  >();
  for (const s of songs) {
    const name = s.genre || 'Unknown';
    const cur = map.get(name);
    const artist = s.artist || 'Unknown';
    if (cur) {
      cur.count += 1;
      cur.plays += s.play_count;
      cur.artists.set(artist, (cur.artists.get(artist) ?? 0) + 1);
    } else {
      map.set(name, {
        name,
        count: 1,
        plays: s.play_count,
        artists: new Map([[artist, 1]]),
        coverId: s.id,
      });
    }
  }
  return [...map.values()].map((g) => {
    let topArtist = '';
    let top = 0;
    for (const [artist, n] of g.artists) {
      if (n > top) {
        top = n;
        topArtist = artist;
      }
    }
    return { name: g.name, count: g.count, plays: g.plays, topArtist, coverId: g.coverId };
  });
}

export function albumPath(artist: string, album: string) {
  return `/library/albums/${encodeURIComponent(artist)}/${encodeURIComponent(album)}`;
}

export function artistPath(name: string) {
  return `/library/artists/${encodeURIComponent(name)}`;
}

export function genrePath(name: string) {
  return `/library/genres/${encodeURIComponent(name)}`;
}

export function shuffleSongs(songs: Song[]): Song[] {
  const next = [...songs];
  for (let i = next.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [next[i], next[j]] = [next[j], next[i]];
  }
  return next;
}
