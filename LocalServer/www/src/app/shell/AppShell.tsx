import { Outlet } from 'react-router-dom';
import { Sidebar } from './Sidebar';
import { TopBar } from './TopBar';
import { MiniPlayer } from '@/features/player/components/MiniPlayer';
import { NowPlaying } from '@/features/player/components/NowPlaying';
import { QueueDrawer } from '@/features/player/components/QueueDrawer';
import { LyricsDialog } from '@/features/library/components/LyricsDialog';
import { useDesktopSync } from '@/features/player/useDesktopSync';
import { useUiStore } from '@/stores/ui-store';
import { useEffect } from 'react';

export function AppShell() {
  const mode = useUiStore((s) => s.themeMode);
  useDesktopSync();

  useEffect(() => {
    const dark =
      mode === 'dark' ||
      (mode === 'system' && window.matchMedia('(prefers-color-scheme: dark)').matches);
    document.documentElement.classList.toggle('dark', dark);
  }, [mode]);

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
      </div>
      <MiniPlayer />
      <NowPlaying />
      <QueueDrawer />
      <LyricsDialog />
    </div>
  );
}
