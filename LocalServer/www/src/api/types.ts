export type PlayerStatus = 'idle' | 'loading' | 'playing' | 'paused' | 'error';
export type RepeatMode = 'off' | 'one' | 'all';

export interface Song {
  id: number;
  artist: string;
  album: string;
  title: string;
  year: number;
  genre: string;
  url: string;
  duration: number;
  fileSize: number;
  timeAdded: number;
  timePlayed: number;
  lyricsFile: string;
  rating: number;
  format: string;
  play_count: number;
  has_lyrics: boolean;
  bitRate: number;
  channels: number;
  bitsPerSample: number;
  sampleRate: number;
}

export interface PlayerState {
  state: PlayerStatus;
  player_id: string;
  song_id: number | null;
  position: number;
  duration: number;
  volume: number;
  shuffle: boolean;
  repeat: RepeatMode;
  state_version: number;
}

export interface QueueItem {
  item_id: number;
  song: Song;
}

export interface LibrarySnapshot {
  version: number;
  generated_at: string;
  artists: string[];
  albums: string[];
  genres: string[];
  songs: Song[];
}

export interface Bootstrap {
  server: { version: string };
  settings: Record<string, unknown>;
  player: PlayerState;
  queue: { items: QueueItem[] };
  library: {
    snapshot_version: number;
    song_count: number;
    album_count: number;
    artist_count: number;
  };
}

export interface LyricsLine {
  time: number | null;
  text: string;
}

export interface SongLyrics {
  song_id: number;
  has_lyrics: boolean;
  synced?: boolean;
  source?: string;
  source_type?: 'lrc' | 'txt' | 'embedded';
  content?: string;
  lines?: LyricsLine[];
  error?: string;
}

export interface ScanStatus {
  state: 'idle' | 'running' | 'finished';
  total: number;
  scanned: number;
  snapshot_version: number;
}

export interface PlaylistBrief {
  id: number;
  name: string;
  count: number;
  duration: number;
  rating: number;
  time_modified: number;
}

export interface PlaylistDetail extends PlaylistBrief {
  songs: Song[];
}

export interface HistoryDayItem {
  song_id: number;
  count: number;
  last_played_at: string;
}

export interface HistoryRecent {
  days: { date: string; items: HistoryDayItem[] }[];
}
