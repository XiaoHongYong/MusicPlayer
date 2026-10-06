import { Monitor, Radio, Search } from 'lucide-react';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';
import { useWebSocketStatus } from '@/features/realtime/useWebSocketStatus';
import { usePlayerStore } from '@/features/player/store';
import { useUiStore, type PlaybackTarget } from '@/stores/ui-store';

function WsStatusIcon() {
  const status = useWebSocketStatus();
  const label =
    status === 'connected' ? 'WebSocket 已连接' : status === 'connecting' ? 'WebSocket 连接中…' : 'WebSocket 已断开';
  return (
    <div
      className="flex items-center gap-1.5 rounded-md px-2 py-1 text-xs text-muted-foreground"
      title={label}
      aria-label={label}
    >
      <span
        className={cn(
          'inline-block h-2 w-2 rounded-full',
          status === 'connected' && 'bg-emerald-500',
          status === 'connecting' && 'animate-pulse bg-amber-400',
          status === 'disconnected' && 'bg-rose-500',
        )}
      />
      <Radio size={14} className={status === 'connected' ? 'text-emerald-500' : 'text-muted-foreground'} />
      <span className="hidden sm:inline">{status === 'connected' ? '已连接' : status === 'connecting' ? '连接中' : '已断开'}</span>
    </div>
  );
}

function PlaybackTargetSwitch() {
  const target = useUiStore((s) => s.playbackTarget);
  const setTarget = useUiStore((s) => s.setPlaybackTarget);
  const onTargetChanged = usePlayerStore((s) => s.onTargetChanged);

  const select = (next: PlaybackTarget) => {
    if (next === target) return;
    setTarget(next);
    onTargetChanged(next);
  };

  return (
    <div
      className="flex items-center gap-1 rounded-lg border border-border bg-muted/40 p-0.5 text-xs"
      title="选择在哪里播放：浏览器本页，或桌面 MusicPlayer"
    >
      <button
        type="button"
        onClick={() => select('browser')}
        className={cn(
          'rounded-md px-2.5 py-1 transition',
          target === 'browser'
            ? 'bg-primary text-primary-foreground shadow-sm'
            : 'text-muted-foreground hover:text-foreground',
        )}
      >
        浏览器
      </button>
      <button
        type="button"
        onClick={() => select('desktop')}
        className={cn(
          'inline-flex items-center gap-1 rounded-md px-2.5 py-1 transition',
          target === 'desktop'
            ? 'bg-primary text-primary-foreground shadow-sm'
            : 'text-muted-foreground hover:text-foreground',
        )}
      >
        <Monitor size={12} />
        播放器
      </button>
    </div>
  );
}

export function TopBar() {
  const toggle = useUiStore((s) => s.setSidebarCollapsed);
  const collapsed = useUiStore((s) => s.sidebarCollapsed);
  return (
    <header className="flex h-14 items-center gap-3 border-b border-border px-4">
      <button className="text-sm text-muted-foreground" onClick={() => toggle(!collapsed)}>
        {collapsed ? '展开' : '收起'}
      </button>
      <div className="relative max-w-md flex-1">
        <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground" />
        <Input placeholder="搜索（即将接入）" className="pl-9" readOnly />
      </div>
      <div className="ml-auto flex items-center gap-2">
        <PlaybackTargetSwitch />
        <WsStatusIcon />
      </div>
    </header>
  );
}
