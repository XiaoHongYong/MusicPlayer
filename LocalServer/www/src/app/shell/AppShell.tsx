import { Outlet } from 'react-router-dom';
import { Sidebar } from './Sidebar';
import { TopBar } from './TopBar';
import { MiniPlayer } from '@/features/player/components/MiniPlayer';
import { NowPlaying } from '@/features/player/components/NowPlaying';
import { QueueDrawer } from '@/features/player/components/QueueDrawer';
import { LyricsDialog } from '@/features/library/components/LyricsDialog';
import { ToastHost } from '@/components/ToastHost';
import { useDesktopSync } from '@/features/player/useDesktopSync';
import { applyThemeAccent, applyThemeMode, useUiStore } from '@/stores/ui-store';
import { usePlayerStore } from '@/features/player/store';
import { useEffect } from 'react';
import { BottomNav } from './BottomNav';

export function AppShell() {
  const mode = useUiStore((s) => s.themeMode);
  const accent = useUiStore((s) => s.themeAccent);
  useDesktopSync();

  useEffect(() => {
    applyThemeMode(mode);
    applyThemeAccent(accent);
    if (mode !== 'system') return;
    const mq = window.matchMedia('(prefers-color-scheme: dark)');
    const onChange = () => applyThemeMode('system');
    mq.addEventListener('change', onChange);
    return () => mq.removeEventListener('change', onChange);
  }, [mode, accent]);

  useEffect(() => {
    if (useUiStore.getState().playbackTarget === 'desktop') {
      usePlayerStore.getState().onTargetChanged('desktop');
    }
  }, []);

  return (
    <div className="flex h-full flex-col">
      <div className="flex min-h-0 flex-1">
        <Sidebar />
        <div className="flex min-w-0 flex-1 flex-col">
          <TopBar />
          <main className="min-h-0 flex-1 overflow-auto">
            <Outlet />
          </main>
        </div>
        <QueueDrawer />
      </div>
      <MiniPlayer />
      <BottomNav />
      <NowPlaying />
      <LyricsDialog />
      <ToastHost />
    </div>
  );
}
