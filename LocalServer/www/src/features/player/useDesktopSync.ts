import { useEffect, useRef } from 'react';
import { api } from '@/api/client';
import { queryClient } from '@/api/query-client';
import type { PlayerState, QueueItem, ScanStatus } from '@/api/types';
import { sseManager, type SseEnvelope } from '@/features/realtime/sse';
import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from './store';

function asScanStatus(data: unknown): ScanStatus | null {
  if (!data || typeof data !== 'object') return null;
  const d = data as Record<string, unknown>;
  if (typeof d.state !== 'string') return null;
  const version = Number(d.snapshot_version ?? d.version ?? 0);
  return {
    state: d.state as ScanStatus['state'],
    total: Number(d.total ?? 0),
    scanned: Number(d.scanned ?? 0),
    snapshot_version: version,
  };
}

function applyEnvelope(envelope: SseEnvelope) {
  const target = useUiStore.getState().playbackTarget;

  if (envelope.event === 'player.state_changed' || envelope.event === 'player.song_changed') {
    if (target !== 'desktop') return;
    const state = envelope.data as PlayerState;
    if (state && typeof state === 'object' && 'state' in state) {
      usePlayerStore.getState().applyServerState(state);
    }
    return;
  }

  if (envelope.event === 'player.queue_changed') {
    if (target !== 'desktop') return;
    const payload = envelope.data as { items?: QueueItem[] };
    const songs = payload?.items?.map((i) => i.song) ?? [];
    const cur = usePlayerStore.getState();
    const fake: PlayerState = {
      state: cur.playing ? 'playing' : 'paused',
      player_id: 'player_1',
      song_id: cur.current()?.id ?? null,
      position: cur.position,
      duration: cur.duration,
      volume: cur.volume,
      shuffle: cur.shuffle,
      repeat: cur.repeat,
      state_version: envelope.state_version,
    };
    usePlayerStore.getState().applyServerState(fake, songs.length ? songs : undefined);
    return;
  }

  if (envelope.event.startsWith('library.scan_')) {
    const scan = asScanStatus(envelope.data);
    if (scan) queryClient.setQueryData(['scan-status'], scan);
    if (envelope.event === 'library.scan_finished') {
      void queryClient.invalidateQueries({ queryKey: ['library-snapshot'] });
      void queryClient.invalidateQueries({ queryKey: ['bootstrap'] });
    }
    return;
  }

  if (envelope.event === 'library.updated') {
    void queryClient.invalidateQueries({ queryKey: ['library-snapshot'] });
    void queryClient.invalidateQueries({ queryKey: ['bootstrap'] });
    return;
  }

  if (envelope.event === 'rating.changed') {
    void queryClient.invalidateQueries({ queryKey: ['library-snapshot'] });
    return;
  }

  if (envelope.event === 'playlist.updated') {
    void queryClient.invalidateQueries({ queryKey: ['playlists'] });
    const id = (envelope.data as { playlist_id?: number | null })?.playlist_id;
    if (id != null) {
      void queryClient.invalidateQueries({ queryKey: ['playlist', id] });
    }
    return;
  }

  if (envelope.event === 'history.updated') {
    void queryClient.invalidateQueries({ queryKey: ['history-recent'] });
    void queryClient.invalidateQueries({ queryKey: ['library-snapshot'] });
  }
}

/** 桌面播放：SSE 推状态；进度条在本地按时间推进，不轮询. */
export function useDesktopSync() {
  const target = useUiStore((s) => s.playbackTarget);
  const playing = usePlayerStore((s) => s.playing);
  const lastTick = useRef(performance.now());

  useEffect(() => {
    sseManager.start();
    const off = sseManager.onEvent(applyEnvelope);
    return off;
  }, []);

  useEffect(() => {
    if (target !== 'desktop') return;

    let cancelled = false;
    const snap = async () => {
      try {
        const [state, queue] = await Promise.all([api.playerState(), api.playerQueue()]);
        if (cancelled) return;
        const songs = queue.items.map((i) => i.song);
        usePlayerStore.getState().applyServerState(state, songs.length ? songs : undefined);
      } catch {
        /* ignore */
      }
    };

    void snap();
    return () => {
      cancelled = true;
    };
  }, [target]);

  useEffect(() => {
    if (target !== 'desktop' || !playing) return;
    lastTick.current = performance.now();
    let raf = 0;
    const loop = (now: number) => {
      const dt = (now - lastTick.current) / 1000;
      lastTick.current = now;
      const s = usePlayerStore.getState();
      if (s.playing) {
        const next = s.duration > 0 ? Math.min(s.position + dt, s.duration) : s.position + dt;
        usePlayerStore.setState({ position: next });
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [target, playing]);
}
