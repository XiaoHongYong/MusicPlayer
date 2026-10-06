import { create } from 'zustand';

type ThemeMode = 'light' | 'dark' | 'system';
export type PlaybackTarget = 'browser' | 'desktop';

const PLAYBACK_KEY = 'pmc.playbackTarget';

function loadPlaybackTarget(): PlaybackTarget {
  try {
    const v = localStorage.getItem(PLAYBACK_KEY);
    if (v === 'desktop' || v === 'browser') return v;
  } catch {
    /* ignore */
  }
  return 'browser';
}

interface UiState {
  sidebarCollapsed: boolean;
  themeMode: ThemeMode;
  nowPlayingOpen: boolean;
  queueOpen: boolean;
  lyricsSongId: number | null;
  playbackTarget: PlaybackTarget;
  setSidebarCollapsed: (v: boolean) => void;
  setThemeMode: (v: ThemeMode) => void;
  setNowPlayingOpen: (v: boolean) => void;
  setQueueOpen: (v: boolean) => void;
  setPlaybackTarget: (v: PlaybackTarget) => void;
  openLyrics: (songId: number) => void;
  closeLyrics: () => void;
}

export const useUiStore = create<UiState>((set) => ({
  sidebarCollapsed: false,
  themeMode: 'dark',
  nowPlayingOpen: false,
  queueOpen: false,
  lyricsSongId: null,
  playbackTarget: loadPlaybackTarget(),
  setSidebarCollapsed: (v) => set({ sidebarCollapsed: v }),
  setThemeMode: (v) => set({ themeMode: v }),
  setNowPlayingOpen: (v) => set({ nowPlayingOpen: v }),
  setQueueOpen: (v) => set({ queueOpen: v }),
  setPlaybackTarget: (v) => {
    try {
      localStorage.setItem(PLAYBACK_KEY, v);
    } catch {
      /* ignore */
    }
    set({ playbackTarget: v });
  },
  openLyrics: (songId) => set({ lyricsSongId: songId }),
  closeLyrics: () => set({ lyricsSongId: null }),
}));
