import { useEffect } from 'react';
import { api } from '@/api/client';
import { useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from './store';

/** 桌面播放模式下轮询 server player state / queue。 */
export function useDesktopSync() {
  const target = useUiStore((s) => s.playbackTarget);

  useEffect(() => {
    if (target !== 'desktop') return;

    let cancelled = false;
    const sync = async () => {
      try {
        const [state, queue] = await Promise.all([api.playerState(), api.playerQueue()]);
        if (cancelled) return;
        const songs = queue.items.map((i) => i.song);
        usePlayerStore.getState().applyServerState(state, songs.length ? songs : undefined);
      } catch {
        /* ignore transient errors */
      }
    };

    void sync();
    const id = window.setInterval(() => void sync(), 1000);
    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, [target]);
}
