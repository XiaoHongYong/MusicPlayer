import { createBrowserRouter } from 'react-router-dom';
import { AppShell } from './shell/AppShell';
import { HomePage } from '@/features/home/HomePage';
import { SongsPage } from '@/features/library/components/SongsPage';
import { AlbumsPage, ArtistsPage, GenresPage } from '@/features/library/components/BrowsePages';
import { AlbumDetailPage } from '@/features/library/components/AlbumDetailPage';
import { ArtistDetailPage } from '@/features/library/components/ArtistDetailPage';
import { GenreDetailPage } from '@/features/library/components/GenreDetailPage';
import { SettingsPage } from '@/features/settings/SettingsPage';
import { StatisticsPage } from '@/features/statistics/StatisticsPage';
import { PlaylistsPage } from '@/features/playlists/components/PlaylistsPage';
import { PlaylistDetailPage } from '@/features/playlists/components/PlaylistDetailPage';
import { HistoryPage } from '@/features/history/components/HistoryPage';
import { SearchPage } from '@/features/search/SearchPage';

export const router = createBrowserRouter([
  {
    path: '/',
    element: <AppShell />,
    children: [
      { index: true, element: <HomePage /> },
      { path: 'search', element: <SearchPage /> },
      { path: 'library/songs', element: <SongsPage /> },
      { path: 'library/albums/:artist/:album', element: <AlbumDetailPage /> },
      { path: 'library/albums', element: <AlbumsPage /> },
      { path: 'library/artists/:name', element: <ArtistDetailPage /> },
      { path: 'library/artists', element: <ArtistsPage /> },
      { path: 'library/genres/:name', element: <GenreDetailPage /> },
      { path: 'library/genres', element: <GenresPage /> },
      { path: 'playlists', element: <PlaylistsPage /> },
      { path: 'playlists/:id', element: <PlaylistDetailPage /> },
      { path: 'history', element: <HistoryPage /> },
      { path: 'statistics', element: <StatisticsPage /> },
      { path: 'settings', element: <SettingsPage /> },
    ],
  },
]);
