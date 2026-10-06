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
  bootstrap: () => request<import('./types').Bootstrap>('/api/v1/bootstrap'),
  snapshot: () => request<import('./types').LibrarySnapshot>('/api/v1/library/snapshot'),
  scanStatus: () => request<import('./types').ScanStatus>('/api/v1/library/scan/status'),
  startScan: () =>
    request<import('./types').ScanStatus>('/api/v1/library/scan', { method: 'POST' }),
  playerState: () => request<import('./types').PlayerState>('/api/v1/player/state'),
  playerQueue: () => request<{ items: import('./types').QueueItem[] }>('/api/v1/player/queue'),
  playerCommand: (cmd: string, body?: unknown) =>
    request<import('./types').PlayerState>(`/api/v1/player/${cmd}`, {
      method: 'POST',
      body: body ? JSON.stringify(body) : undefined,
    }),
  song: (id: number) => request<import('./types').Song>(`/api/v1/songs/${id}`),
  lyrics: (id: number) => request<import('./types').SongLyrics>(`/api/v1/songs/${id}/lyrics`),
  setRating: (id: number, rating: number) =>
    request<import('./types').Song>(`/api/v1/songs/${id}/rating`, {
      method: 'PUT',
      body: JSON.stringify({ rating }),
    }),
  playlists: () => request<import('./types').PlaylistBrief[]>('/api/v1/playlists'),
  playlist: (id: number) => request<import('./types').PlaylistDetail>(`/api/v1/playlists/${id}`),
  createPlaylist: (name: string) =>
    request<import('./types').PlaylistDetail>('/api/v1/playlists', {
      method: 'POST',
      body: JSON.stringify({ name }),
    }),
  patchPlaylist: (id: number, body: { name: string }) =>
    request<import('./types').PlaylistDetail>(`/api/v1/playlists/${id}`, {
      method: 'PATCH',
      body: JSON.stringify(body),
    }),
  deletePlaylist: (id: number) =>
    request<void>(`/api/v1/playlists/${id}`, { method: 'DELETE' }),
  addPlaylistSongs: (id: number, songIds: number[]) =>
    request<import('./types').PlaylistDetail>(`/api/v1/playlists/${id}/songs`, {
      method: 'POST',
      body: JSON.stringify({ song_ids: songIds }),
    }),
  removePlaylistSong: (id: number, songId: number) =>
    request<void>(`/api/v1/playlists/${id}/songs/${songId}`, { method: 'DELETE' }),
  reorderPlaylistSongs: (id: number, songIds: number[]) =>
    request<import('./types').PlaylistDetail>(`/api/v1/playlists/${id}/songs/order`, {
      method: 'PUT',
      body: JSON.stringify({ song_ids: songIds }),
    }),
  postHistory: (songId: number, playedAt = new Date().toISOString()) =>
    request<unknown>('/api/v1/history', {
      method: 'POST',
      body: JSON.stringify({ song_id: songId, played_at: playedAt }),
    }),
  recentHistory: (days = 30) =>
    request<import('./types').HistoryRecent>(`/api/v1/history/recent?days=${days}`),
};

export function streamUrl(id: number) {
  return `/api/v1/songs/${id}/stream`;
}

export function coverUrl(id: number) {
  return `/api/v1/songs/${id}/cover`;
}
