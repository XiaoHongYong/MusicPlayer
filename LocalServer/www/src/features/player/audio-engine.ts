import { streamUrl } from '@/api/client';
import type { Song } from '@/api/types';

type Listener = () => void;

class AudioEngine {
  private audio: HTMLAudioElement | null = null;
  private listeners = new Set<Listener>();

  private ensure() {
    if (!this.audio) {
      this.audio = new Audio();
      this.audio.preload = 'metadata';
      const bump = () => this.emit();
      this.audio.addEventListener('timeupdate', bump);
      this.audio.addEventListener('play', bump);
      this.audio.addEventListener('pause', bump);
      this.audio.addEventListener('ended', bump);
      this.audio.addEventListener('loadedmetadata', bump);
      this.audio.addEventListener('error', bump);
    }
    return this.audio;
  }

  subscribe(fn: Listener) {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit() {
    this.listeners.forEach((fn) => fn());
  }

  load(song: Song) {
    const a = this.ensure();
    a.src = streamUrl(song.id);
    a.load();
    this.emit();
  }

  play() {
    void this.ensure().play();
  }

  pause() {
    this.ensure().pause();
  }

  seek(positionSec: number) {
    this.ensure().currentTime = positionSec;
    this.emit();
  }

  setVolume(v: number) {
    this.ensure().volume = Math.min(1, Math.max(0, v));
    this.emit();
  }

  get position() {
    return this.audio?.currentTime ?? 0;
  }

  get duration() {
    return this.audio?.duration && Number.isFinite(this.audio.duration) ? this.audio.duration : 0;
  }

  get paused() {
    return this.audio?.paused ?? true;
  }

  get ended() {
    return this.audio?.ended ?? false;
  }

  get volume() {
    return this.audio?.volume ?? 1;
  }

  /** 供 Web Audio AnalyserNode 挂接；可能尚未 ensure。 */
  getMediaElement(): HTMLAudioElement | null {
    return this.audio;
  }

  /** 确保 audio 元素已创建（可视化在 browser 播放时需要）. */
  ensureMediaElement(): HTMLAudioElement {
    return this.ensure();
  }

  bindMediaSession(song: Song | null, handlers: { play: () => void; pause: () => void; next: () => void; prev: () => void }) {
    if (!('mediaSession' in navigator)) return;
    if (!song) {
      navigator.mediaSession.metadata = null;
      return;
    }
    navigator.mediaSession.metadata = new MediaMetadata({
      title: song.title,
      artist: song.artist,
      album: song.album,
    });
    navigator.mediaSession.setActionHandler('play', handlers.play);
    navigator.mediaSession.setActionHandler('pause', handlers.pause);
    navigator.mediaSession.setActionHandler('nexttrack', handlers.next);
    navigator.mediaSession.setActionHandler('previoustrack', handlers.prev);
  }
}

export const audioEngine = new AudioEngine();
