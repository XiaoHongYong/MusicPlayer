import { create } from 'zustand';
import type { PlayerState, RepeatMode, Song } from '@/api/types';
import { api } from '@/api/client';
import { queryClient } from '@/api/query-client';
import { useUiStore, type PlaybackTarget } from '@/stores/ui-store';
import { audioEngine } from './audio-engine';
import { shouldRecordPlayHistory } from './utils';

function invalidatePlayStats() {
  void queryClient.invalidateQueries({ queryKey: ['history-recent'] });
  void queryClient.invalidateQueries({ queryKey: ['library-snapshot'] });
}

function target(): PlaybackTarget {
  return useUiStore.getState().playbackTarget;
}

export type QueueActionMode = 'replace' | 'addThis' | 'addAll';

export interface QueueActionResult {
  kind: 'replace' | 'add';
  count: number;
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
  applyQueueAction: (opts: {
    mode: QueueActionMode;
    thisSongs: Song[];
    allSongs: Song[];
    startIndex: number;
    playNow: boolean;
    addToFront: boolean;
  }) => QueueActionResult;
  playPause: () => void;
  next: () => void;
  prev: () => void;
  seek: (sec: number) => void;
  setVolume: (v: number) => void;
  setShuffle: (v: boolean) => void;
  setRepeat: (v: RepeatMode) => void;
  cycleRepeat: () => void;
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

function commitLocalQueue(nextQueue: Song[], nextIndex: number, playNow: boolean, reload: boolean) {
  const song = nextQueue[nextIndex] ?? null;
  const wasPlaying = usePlayerStore.getState().playing;
  usePlayerStore.setState({
    queue: nextQueue,
    index: song ? nextIndex : -1,
    playing: playNow || (!reload && wasPlaying),
    historyReported: reload ? false : usePlayerStore.getState().historyReported,
    position: reload ? 0 : usePlayerStore.getState().position,
  });
  if (target() === 'desktop') {
    audioEngine.pause();
    bindSession(song);
    return;
  }
  if (reload && song) {
    audioEngine.load(song);
    audioEngine.setVolume(usePlayerStore.getState().volume);
    if (playNow) audioEngine.play();
    else audioEngine.pause();
  }
  bindSession(song);
}

function syncDesktopQueue(
  action: 'replace' | 'insert',
  songs: Song[],
  index: number,
  play: boolean,
) {
  void api
    .setQueue({ action, song_ids: songs.map((s) => s.id), index, play })
    .then((state) => {
      if (state && typeof state === 'object' && 'state' in state) {
        usePlayerStore.getState().applyServerState(state, undefined);
      }
    })
    .catch(() => undefined);
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
    get().applyQueueAction({
      mode: 'addAll',
      thisSongs: songs,
      allSongs: songs,
      startIndex: 0,
      playNow: false,
      addToFront: false,
    });
  },

  applyQueueAction: ({ mode, thisSongs, allSongs, startIndex, playNow, addToFront }) => {
    const { queue, index } = get();
    if (mode === 'replace') {
      const next = allSongs;
      if (!next.length) return { kind: 'replace', count: 0 };
      const nextIndex = Math.min(Math.max(0, startIndex), next.length - 1);
      commitLocalQueue(next, nextIndex, playNow, true);
      if (target() === 'desktop') syncDesktopQueue('replace', next, nextIndex, playNow);
      return { kind: 'replace', count: next.length };
    }

    const incoming = mode === 'addThis' ? thisSongs : allSongs;
    if (!incoming.length) return { kind: 'add', count: 0 };
    const inQueue = new Set(queue.map((s) => s.id));
    const toAdd = incoming.filter((s) => !inQueue.has(s.id));
    const insertAt = addToFront || queue.length === 0 ? 0 : queue.length;
    const nextQueue =
      toAdd.length > 0
        ? [...queue.slice(0, insertAt), ...toAdd, ...queue.slice(insertAt)]
        : queue;

    let nextIndex = index;
    if (playNow) {
      const firstId = incoming[0].id;
      const found = nextQueue.findIndex((s) => s.id === firstId);
      nextIndex = found >= 0 ? found : insertAt;
      get().playSongs(nextQueue, Math.max(0, nextIndex));
      return { kind: 'add', count: toAdd.length };
    }
    if (toAdd.length && insertAt <= index) {
      nextIndex = index + toAdd.length;
    }

    const shouldReload = queue.length === 0 && toAdd.length > 0;
    if (toAdd.length) {
      commitLocalQueue(nextQueue, nextQueue.length ? Math.max(0, nextIndex) : -1, false, shouldReload);
      if (target() === 'desktop') {
        syncDesktopQueue('insert', toAdd, addToFront ? 0 : -1, false);
      }
    }
    return { kind: 'add', count: toAdd.length };
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

  cycleRepeat: () => {
    const cur = get().repeat;
    const next: RepeatMode = cur === 'off' ? 'all' : cur === 'all' ? 'one' : 'off';
    get().setRepeat(next);
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
      // 浏览器播放写入后端 play_history，并累加歌曲 play_count；桌面播放由应用端同一套接口记账，此处不重复上报。
      void api
        .postHistory(song.id)
        .then(() => invalidatePlayStats())
        .catch(() => undefined);
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
