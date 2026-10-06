import { create } from 'zustand';
import type { PlayerState, RepeatMode, Song } from '@/api/types';
import { api } from '@/api/client';
import { useUiStore, type PlaybackTarget } from '@/stores/ui-store';
import { audioEngine } from './audio-engine';
import { shouldRecordPlayHistory } from './utils';

function target(): PlaybackTarget {
  return useUiStore.getState().playbackTarget;
}

interface PlayerStore {
  queue: Song[];
  index: number;
  shuffle: boolean;
  repeat: RepeatMode;
  volume: number;
  position: number;
  duration: number;
  playing: boolean;
  historyReported: boolean;
  playSongs: (songs: Song[], startIndex?: number) => void;
  addToQueue: (songs: Song[]) => void;
  playPause: () => void;
  next: () => void;
  prev: () => void;
  seek: (sec: number) => void;
  setVolume: (v: number) => void;
  setShuffle: (v: boolean) => void;
  setRepeat: (v: RepeatMode) => void;
  applyServerState: (state: PlayerState, queueSongs?: Song[]) => void;
  onTargetChanged: (next: PlaybackTarget) => void;
  tick: () => void;
  current: () => Song | null;
}

function bindSession(song: Song | null) {
  audioEngine.bindMediaSession(song, {
    play: () => usePlayerStore.getState().playPause(),
    pause: () => usePlayerStore.getState().playPause(),
    next: () => usePlayerStore.getState().next(),
    prev: () => usePlayerStore.getState().prev(),
  });
}

export const usePlayerStore = create<PlayerStore>((set, get) => ({
  queue: [],
  index: -1,
  shuffle: false,
  repeat: 'all',
  volume: 0.8,
  position: 0,
  duration: 0,
  playing: false,
  historyReported: false,

  current: () => {
    const { queue, index } = get();
    return index >= 0 ? queue[index] ?? null : null;
  },

  playSongs: (songs, startIndex = 0) => {
    if (!songs.length) return;
    const index = Math.min(Math.max(0, startIndex), songs.length - 1);
    set({ queue: songs, index, playing: true, historyReported: false, position: 0 });
    const song = songs[index];

    if (target() === 'desktop') {
      audioEngine.pause();
      void api
        .playerCommand('play', { song_ids: songs.map((s) => s.id), index })
        .then((state) => {
          if (state && typeof state === 'object' && 'state' in (state as object)) {
            get().applyServerState(state as PlayerState, songs);
          }
        })
        .catch(() => undefined);
      bindSession(song);
      return;
    }

    audioEngine.load(song);
    audioEngine.setVolume(get().volume);
    audioEngine.play();
    bindSession(song);
  },

  addToQueue: (songs) => {
    set({ queue: [...get().queue, ...songs] });
  },

  playPause: () => {
    const { queue, index, playing } = get();
    if (index < 0 || !queue[index]) return;

    if (target() === 'desktop') {
      void api.playerCommand(playing ? 'pause' : 'play').catch(() => undefined);
      set({ playing: !playing });
      return;
    }

    if (playing) {
      audioEngine.pause();
      set({ playing: false });
    } else {
      audioEngine.play();
      set({ playing: true });
    }
  },

  next: () => {
    if (target() === 'desktop') {
      void api.playerCommand('next').catch(() => undefined);
      return;
    }

    const { queue, index, repeat, shuffle } = get();
    if (!queue.length) return;
    let nextIndex = index + 1;
    if (shuffle) nextIndex = Math.floor(Math.random() * queue.length);
    if (nextIndex >= queue.length) {
      if (repeat === 'all') nextIndex = 0;
      else {
        audioEngine.pause();
        set({ playing: false });
        return;
      }
    }
    if (repeat === 'one') nextIndex = index;
    set({ index: nextIndex, playing: true, historyReported: false });
    const song = queue[nextIndex];
    audioEngine.load(song);
    audioEngine.play();
    bindSession(song);
  },

  prev: () => {
    if (target() === 'desktop') {
      if (get().position > 3) {
        void api.playerCommand('seek', { position: 0 }).catch(() => undefined);
        set({ position: 0 });
        return;
      }
      void api.playerCommand('previous').catch(() => undefined);
      return;
    }

    const { queue, index, position } = get();
    if (!queue.length) return;
    if (position > 3) {
      audioEngine.seek(0);
      return;
    }
    const prevIndex = index <= 0 ? queue.length - 1 : index - 1;
    set({ index: prevIndex, playing: true, historyReported: false });
    audioEngine.load(queue[prevIndex]);
    audioEngine.play();
    bindSession(queue[prevIndex]);
  },

  seek: (sec) => {
    if (target() === 'desktop') {
      void api.playerCommand('seek', { position: sec }).catch(() => undefined);
      set({ position: sec });
      return;
    }
    audioEngine.seek(sec);
    set({ position: sec });
  },

  setVolume: (v) => {
    if (target() === 'desktop') {
      void api.playerCommand('volume', { volume: v }).catch(() => undefined);
      set({ volume: v });
      return;
    }
    audioEngine.setVolume(v);
    set({ volume: v });
  },

  setShuffle: (v) => {
    if (target() === 'desktop') {
      void api.playerCommand('shuffle', { shuffle: v }).catch(() => undefined);
    }
    set({ shuffle: v });
  },

  setRepeat: (v) => {
    if (target() === 'desktop') {
      void api.playerCommand('repeat', { repeat: v }).catch(() => undefined);
    }
    set({ repeat: v });
  },

  applyServerState: (state, queueSongs) => {
    const queue = queueSongs ?? get().queue;
    let index = get().index;
    if (state.song_id != null) {
      const found = queue.findIndex((s) => s.id === state.song_id);
      if (found >= 0) index = found;
    }
    set({
      queue,
      index,
      playing: state.state === 'playing',
      position: state.position ?? 0,
      duration: state.duration ?? 0,
      volume: typeof state.volume === 'number' ? state.volume : get().volume,
      shuffle: state.shuffle,
      repeat: state.repeat,
    });
    bindSession(index >= 0 ? queue[index] ?? null : null);
  },

  onTargetChanged: (next) => {
    if (next === 'desktop') {
      audioEngine.pause();
      set({ playing: false });
      void Promise.all([api.playerState(), api.playerQueue()])
        .then(([state, queue]) => {
          const songs = queue.items.map((i) => i.song);
          get().applyServerState(state, songs.length ? songs : undefined);
        })
        .catch(() => undefined);
    } else {
      void api.playerCommand('pause').catch(() => undefined);
      set({ playing: false, position: 0 });
    }
  },

  tick: () => {
    if (target() === 'desktop') return;

    const ended = audioEngine.ended;
    const position = audioEngine.position;
    const duration = audioEngine.duration;
    set({
      position,
      duration,
      playing: !audioEngine.paused && !ended,
    });
    const song = get().current();
    if (
      song &&
      shouldRecordPlayHistory({
        reported: get().historyReported,
        position,
        duration: duration || song.duration,
      })
    ) {
      set({ historyReported: true });
      void api.postHistory(song.id).catch(() => undefined);
    }
    if (ended) {
      audioEngine.pause();
      get().next();
    }
  },
}));

audioEngine.subscribe(() => {
  usePlayerStore.getState().tick();
});
