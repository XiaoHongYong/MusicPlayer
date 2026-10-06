import { create } from 'zustand';

type ThemeMode = 'light' | 'dark' | 'system';
export type PlaybackTarget = 'browser' | 'desktop';

const KEYS = {
  playback: 'pmc.playbackTarget',
  theme: 'pmc.themeMode',
  sidebar: 'pmc.sidebarCollapsed',
  playImmediately: 'pmc.playImmediately',
  addToQueueFront: 'pmc.addToQueueFront',
} as const;

function readStorage(key: string): string | null {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function writeStorage(key: string, value: string) {
  try {
    localStorage.setItem(key, value);
  } catch {
    /* ignore quota / private mode */
  }
}

function loadPlaybackTarget(): PlaybackTarget {
  const v = readStorage(KEYS.playback);
  return v === 'desktop' || v === 'browser' ? v : 'browser';
}

function loadThemeMode(): ThemeMode {
  const v = readStorage(KEYS.theme);
  return v === 'light' || v === 'dark' || v === 'system' ? v : 'dark';
}

function loadSidebarCollapsed(): boolean {
  return readStorage(KEYS.sidebar) === '1';
}

function loadBool(key: string, fallback: boolean): boolean {
  const v = readStorage(key);
  if (v == null) return fallback;
  return v === '1' || v === 'true';
}

export function applyThemeMode(mode: ThemeMode) {
  if (typeof document === 'undefined') return;
  const dark =
    mode === 'dark' ||
    (mode === 'system' && window.matchMedia('(prefers-color-scheme: dark)').matches);
  document.documentElement.classList.toggle('dark', dark);
}

interface UiState {
  sidebarCollapsed: boolean;
  themeMode: ThemeMode;
  nowPlayingOpen: boolean;
  queueOpen: boolean;
  lyricsSongId: number | null;
  playbackTarget: PlaybackTarget;
  playImmediately: boolean;
  addToQueueFront: boolean;
  setSidebarCollapsed: (v: boolean) => void;
  setThemeMode: (v: ThemeMode) => void;
  setNowPlayingOpen: (v: boolean) => void;
  setQueueOpen: (v: boolean) => void;
  setPlaybackTarget: (v: PlaybackTarget) => void;
  setPlayImmediately: (v: boolean) => void;
  setAddToQueueFront: (v: boolean) => void;
  openLyrics: (songId: number) => void;
  closeLyrics: () => void;
}

const initialTheme = loadThemeMode();
applyThemeMode(initialTheme);

export const useUiStore = create<UiState>((set) => ({
  sidebarCollapsed: loadSidebarCollapsed(),
  themeMode: initialTheme,
  nowPlayingOpen: false,
  queueOpen: false,
  lyricsSongId: null,
  playbackTarget: loadPlaybackTarget(),
  playImmediately: loadBool(KEYS.playImmediately, true),
  addToQueueFront: loadBool(KEYS.addToQueueFront, true),
  setSidebarCollapsed: (v) => {
    writeStorage(KEYS.sidebar, v ? '1' : '0');
    set({ sidebarCollapsed: v });
  },
  setThemeMode: (v) => {
    writeStorage(KEYS.theme, v);
    applyThemeMode(v);
    set({ themeMode: v });
  },
  setNowPlayingOpen: (v) => set({ nowPlayingOpen: v }),
  setQueueOpen: (v) => set({ queueOpen: v }),
  setPlaybackTarget: (v) => {
    writeStorage(KEYS.playback, v);
    set({ playbackTarget: v });
  },
  setPlayImmediately: (v) => {
    writeStorage(KEYS.playImmediately, v ? '1' : '0');
    set({ playImmediately: v });
  },
  setAddToQueueFront: (v) => {
    writeStorage(KEYS.addToQueueFront, v ? '1' : '0');
    set({ addToQueueFront: v });
  },
  openLyrics: (songId) => set({ lyricsSongId: songId }),
  closeLyrics: () => set({ lyricsSongId: null }),
}));
