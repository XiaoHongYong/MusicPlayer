import { mediaDurationSeconds } from '@/lib/utils';
import type { LibrarySnapshot, PlaylistDetail, QueueItem, Song, StatisticsSnapshot } from './types';

function normalizeSong(song: Song): Song {
  return { ...song, duration: mediaDurationSeconds(song.duration) };
}

function normalizeSnapshot(data: LibrarySnapshot): LibrarySnapshot {
  return { ...data, songs: (data.songs ?? []).map(normalizeSong) };
}

function normalizePlaylist(data: PlaylistDetail): PlaylistDetail {
  return { ...data, songs: (data.songs ?? []).map(normalizeSong) };
}

function normalizeQueue(data: { items?: QueueItem[] }): { items: QueueItem[] } {
  return {
    items: (data.items ?? []).map((item) => ({ ...item, song: normalizeSong(item.song) })),
  };
}

function normalizeStatistics(data: StatisticsSnapshot): StatisticsSnapshot {
  if (!data.song_facts) return data;
  return {
    ...data,
    song_facts: data.song_facts.map((row) => ({ ...row, duration: mediaDurationSeconds(row.duration) })),
  };
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    ...init,
    headers: {
      Accept: 'application/json',
      ...(init?.body ? { 'Content-Type': 'application/json' } : {}),
      ...init?.headers,
    },
  });
  if (!res.ok) {
    let message = res.statusText;
    try {
      const body = (await res.json()) as { error?: { message?: string } };
      if (body.error?.message) message = body.error.message;
    } catch {
      /* ignore */
    }
    throw new Error(message);
  }
  if (res.status === 204) return undefined as T;
  return (await res.json()) as T;
}

export const api = {
  bootstrap: async () => {
    const data = await request<import('./types').Bootstrap>('/api/v1/bootstrap');
    return { ...data, queue: normalizeQueue(data.queue ?? { items: [] }) };
  },
  snapshot: async () => normalizeSnapshot(await request<LibrarySnapshot>('/api/v1/library/snapshot')),
  scanStatus: () => request<import('./types').ScanStatus>('/api/v1/library/scan/status'),
  startScan: () =>
    request<import('./types').ScanStatus>('/api/v1/library/scan', { method: 'POST' }),
  playerState: () => request<import('./types').PlayerState>('/api/v1/player/state'),
  playerQueue: async () => normalizeQueue(await request<{ items: QueueItem[] }>('/api/v1/player/queue')),
  setQueue: (body: {
    action: 'replace' | 'insert';
    song_ids: number[];
    index?: number;
    play?: boolean;
  }) =>
    request<import('./types').PlayerState>('/api/v1/player/queue', {
      method: 'POST',
      body: JSON.stringify(body),
    }),
  playerCommand: (cmd: string, body?: unknown) =>
    request<import('./types').PlayerState>(`/api/v1/player/${cmd}`, {
      method: 'POST',
      body: body ? JSON.stringify(body) : undefined,
    }),
  song: async (id: number) => normalizeSong(await request<Song>(`/api/v1/songs/${id}`)),
  lyrics: (id: number) => request<import('./types').SongLyrics>(`/api/v1/songs/${id}/lyrics`),
  setRating: async (id: number, rating: number) =>
    normalizeSong(
      await request<Song>(`/api/v1/songs/${id}/rating`, {
        method: 'PUT',
        body: JSON.stringify({ rating }),
      }),
    ),
  playlists: () => request<import('./types').PlaylistBrief[]>('/api/v1/playlists'),
  playlist: async (id: number) =>
    normalizePlaylist(await request<PlaylistDetail>(`/api/v1/playlists/${id}`)),
  createPlaylist: async (name: string) =>
    normalizePlaylist(
      await request<PlaylistDetail>('/api/v1/playlists', {
        method: 'POST',
        body: JSON.stringify({ name }),
      }),
    ),
  patchPlaylist: async (id: number, body: { name: string }) =>
    normalizePlaylist(
      await request<PlaylistDetail>(`/api/v1/playlists/${id}`, {
        method: 'PATCH',
        body: JSON.stringify(body),
      }),
    ),
  deletePlaylist: (id: number) =>
    request<void>(`/api/v1/playlists/${id}`, { method: 'DELETE' }),
  addPlaylistSongs: async (id: number, songIds: number[]) =>
    normalizePlaylist(
      await request<PlaylistDetail>(`/api/v1/playlists/${id}/songs`, {
        method: 'POST',
        body: JSON.stringify({ song_ids: songIds }),
      }),
    ),
  removePlaylistSong: (id: number, songId: number) =>
    request<void>(`/api/v1/playlists/${id}/songs/${songId}`, { method: 'DELETE' }),
  reorderPlaylistSongs: async (id: number, songIds: number[]) =>
    normalizePlaylist(
      await request<PlaylistDetail>(`/api/v1/playlists/${id}/songs/order`, {
        method: 'PUT',
        body: JSON.stringify({ song_ids: songIds }),
      }),
    ),
  postHistory: (songId: number, playedAt = new Date().toISOString()) =>
    request<unknown>('/api/v1/history', {
      method: 'POST',
      body: JSON.stringify({ song_id: songId, played_at: playedAt }),
    }),
  recentHistory: (days = 30) =>
    request<import('./types').HistoryRecent>(`/api/v1/history/recent?days=${days}`),
  statisticsSnapshot: async () =>
    normalizeStatistics(await request<StatisticsSnapshot>('/api/v1/statistics/snapshot')),
};

export function streamUrl(id: number) {
  return `/api/v1/songs/${id}/stream`;
}

export function coverUrl(id: number) {
  return `/api/v1/songs/${id}/cover`;
}
