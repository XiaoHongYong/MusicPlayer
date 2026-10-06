import { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { LayoutGrid, List } from 'lucide-react';
import { useLibrarySnapshot } from './SongsPage';
import { CoverImage } from '@/components/CoverImage';
import {
  MediaContextMenu,
  mediaMenuFromEvent,
  type MediaMenuTarget,
} from './MediaContextMenu';
import { FilterSelect } from './FilterSelect';
import { cn } from '@/lib/utils';
import { albumPath, artistPath, genrePath, groupAlbums, groupArtists, groupGenres } from '../groups';
import { useT } from '@/i18n';

export function AlbumsPage() {
  const t = useT();
  const { data } = useLibrarySnapshot();
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const [artist, setArtist] = useState('');
  const [genre, setGenre] = useState('');
  const [year, setYear] = useState('');
  const [sort, setSort] = useState('name');
  const [view, setView] = useState<'grid' | 'list'>('grid');
  const allSongs = data?.songs ?? [];
  const albums = useMemo(() => groupAlbums(allSongs), [allSongs]);

  const artists = useMemo(
    () => [...new Set(albums.map((a) => a.artist))].sort((a, b) => a.localeCompare(b)),
    [albums],
  );
  const genres = useMemo(
    () => [...new Set(albums.map((a) => a.genre))].sort((a, b) => a.localeCompare(b)),
    [albums],
  );
  const years = useMemo(
    () => [...new Set(albums.map((a) => a.year).filter((y) => y > 0))].sort((a, b) => b - a),
    [albums],
  );

  const filtered = useMemo(() => {
    let list = albums.filter((a) => {
      if (artist && a.artist !== artist) return false;
      if (genre && a.genre !== genre) return false;
      if (year && String(a.year) !== year) return false;
      return true;
    });
    list = [...list].sort((a, b) => {
      if (sort === 'year') return b.year - a.year || a.name.localeCompare(b.name);
      if (sort === 'count') return b.count - a.count || a.name.localeCompare(b.name);
      if (sort === 'plays') return b.plays - a.plays || a.name.localeCompare(b.name);
      return a.name.localeCompare(b.name);
    });
    return list;
  }, [albums, artist, genre, year, sort]);

  return (
    <div className="p-6">
      <div className="mb-4 flex flex-wrap items-center justify-between gap-3">
        <h1 className="text-2xl font-semibold">{t('Albums')}</h1>
        <div className="flex rounded-md border border-border p-0.5">
          <button
            type="button"
            className={cn('rounded p-1.5', view === 'grid' && 'bg-accent')}
            aria-label={t('Grid')}
            onClick={() => setView('grid')}
          >
            <LayoutGrid size={16} />
          </button>
          <button
            type="button"
            className={cn('rounded p-1.5', view === 'list' && 'bg-accent')}
            aria-label={t('List')}
            onClick={() => setView('list')}
          >
            <List size={16} />
          </button>
        </div>
      </div>
      <div className="mb-4 flex flex-wrap gap-2">
        <FilterSelect aria-label={t('Artist')} value={artist} onChange={setArtist}>
          <option value="">{t('All artists')}</option>
          {artists.map((a) => (
            <option key={a} value={a}>
              {a}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Genre')} value={genre} onChange={setGenre}>
          <option value="">{t('All genres')}</option>
          {genres.map((g) => (
            <option key={g} value={g}>
              {g}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Year')} value={year} onChange={setYear}>
          <option value="">{t('All years')}</option>
          {years.map((y) => (
            <option key={y} value={String(y)}>
              {y}
            </option>
          ))}
        </FilterSelect>
        <FilterSelect aria-label={t('Sort')} value={sort} onChange={setSort}>
          <option value="name">{t('Sort by name')}</option>
          <option value="year">{t('Sort by year')}</option>
          <option value="count">{t('Sort by track count')}</option>
          <option value="plays">{t('Sort by plays')}</option>
        </FilterSelect>
      </div>
      {view === 'grid' ? (
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          {filtered.map((a) => {
            const albumSongs = allSongs.filter((s) => s.album === a.name && s.artist === a.artist);
            return (
              <Link
                key={`${a.name}-${a.artist}`}
                to={albumPath(a.artist, a.name)}
                className="text-left"
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, albumSongs))}
              >
                <CoverImage songId={a.coverId} className="aspect-square w-full rounded-lg" />
                <div className="mt-2 truncate text-sm font-medium">{a.name}</div>
                <div className="truncate text-xs text-muted-foreground">
                  {a.artist} · {t('{n} tracks', { n: a.count })}
                </div>
              </Link>
            );
          })}
        </div>
      ) : (
        <div className="space-y-1">
          {filtered.map((a) => {
            const albumSongs = allSongs.filter((s) => s.album === a.name && s.artist === a.artist);
            return (
              <Link
                key={`${a.name}-${a.artist}`}
                to={albumPath(a.artist, a.name)}
                className="flex items-center gap-3 rounded-md px-2 py-2 hover:bg-accent"
                onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, albumSongs))}
              >
                <CoverImage songId={a.coverId} className="h-12 w-12 rounded" />
                <div className="min-w-0 flex-1">
                  <div className="truncate text-sm">{a.name}</div>
                  <div className="truncate text-xs text-muted-foreground">{a.artist}</div>
                </div>
                <span className="text-xs text-muted-foreground">{a.year || '—'}</span>
                <span className="w-12 text-right text-xs text-muted-foreground">{t('{n} tracks', { n: a.count })}</span>
              </Link>
            );
          })}
        </div>
      )}
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}

export function ArtistsPage() {
  const t = useT();
  const { data } = useLibrarySnapshot();
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const [sort, setSort] = useState('name');
  const allSongs = data?.songs ?? [];
  const artists = useMemo(() => {
    const list = groupArtists(allSongs);
    return [...list].sort((a, b) => {
      if (sort === 'count') return b.count - a.count || a.name.localeCompare(b.name);
      if (sort === 'albums') return b.albums - a.albums || a.name.localeCompare(b.name);
      if (sort === 'plays') return b.plays - a.plays || a.name.localeCompare(b.name);
      return a.name.localeCompare(b.name);
    });
  }, [allSongs, sort]);

  return (
    <div className="p-6">
      <div className="mb-4 flex flex-wrap items-center justify-between gap-3">
        <h1 className="text-2xl font-semibold">{t('Artists')}</h1>
        <FilterSelect aria-label={t('Sort')} value={sort} onChange={setSort}>
          <option value="name">{t('Sort by name')}</option>
          <option value="albums">{t('Sort by album count')}</option>
          <option value="count">{t('Sort by track count')}</option>
          <option value="plays">{t('Sort by plays')}</option>
        </FilterSelect>
      </div>
      <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
        {artists.map((a) => {
          const artistSongs = allSongs.filter((s) => (s.artist || 'Unknown') === a.name);
          return (
            <Link
              key={a.name}
              to={artistPath(a.name)}
              className="rounded-lg bg-card p-4 text-left hover:bg-accent"
              onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, artistSongs))}
            >
              <CoverImage songId={a.coverId} kind="artist" className="aspect-square w-full rounded-lg" />
              <div className="mt-3 truncate text-center text-sm font-medium">{a.name}</div>
              <div className="text-center text-xs text-muted-foreground">
                {t('{albums} albums · {n} tracks', { albums: a.albums, n: a.count })}
              </div>
            </Link>
          );
        })}
      </div>
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}

export function GenresPage() {
  const t = useT();
  const { data } = useLibrarySnapshot();
  const [menu, setMenu] = useState<MediaMenuTarget | null>(null);
  const [sort, setSort] = useState('count');
  const allSongs = data?.songs ?? [];
  const genres = useMemo(() => {
    const list = groupGenres(allSongs);
    return [...list].sort((a, b) => {
      if (sort === 'name') return a.name.localeCompare(b.name);
      if (sort === 'plays') return b.plays - a.plays || a.name.localeCompare(b.name);
      return b.count - a.count || a.name.localeCompare(b.name);
    });
  }, [allSongs, sort]);

  return (
    <div className="p-6">
      <div className="mb-4 flex flex-wrap items-center justify-between gap-3">
        <h1 className="text-2xl font-semibold">{t('Genres')}</h1>
        <FilterSelect aria-label={t('Sort')} value={sort} onChange={setSort}>
          <option value="count">{t('Sort by track count')}</option>
          <option value="plays">{t('Sort by plays')}</option>
          <option value="name">{t('Sort by name')}</option>
        </FilterSelect>
      </div>
      <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
        {genres.map((g) => {
          const genreSongs = allSongs.filter((s) => (s.genre || 'Unknown') === g.name);
          return (
            <Link
              key={g.name}
              to={genrePath(g.name)}
              className="rounded-xl bg-card p-6 text-left hover:bg-accent"
              onContextMenu={(e) => setMenu(mediaMenuFromEvent(e, genreSongs))}
            >
              <div className="text-lg font-semibold">{g.name}</div>
              <div className="mt-1 text-sm text-muted-foreground">
                {t('{n} tracks · {plays} plays', { n: g.count, plays: g.plays })}
              </div>
              {g.topArtist && (
                <div className="mt-1 truncate text-xs text-muted-foreground">{t('Top: {name}', { name: g.topArtist })}</div>
              )}
            </Link>
          );
        })}
      </div>
      {menu && <MediaContextMenu target={menu} onClose={() => setMenu(null)} />}
    </div>
  );
}
