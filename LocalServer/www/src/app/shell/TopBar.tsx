import { useEffect, useState } from 'react';
import { useLocation, useNavigate, useSearchParams } from 'react-router-dom';
import { Monitor, Radio, Search } from 'lucide-react';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';
import { useRealtimeStatus } from '@/features/realtime/useRealtimeStatus';
import { usePlayerStore } from '@/features/player/store';
import { useUiStore, type PlaybackTarget } from '@/stores/ui-store';

function ConnectionStatusIcon() {
  const { status, reconnect } = useRealtimeStatus();
  const offline = status !== 'connected';
  const label =
    status === 'connected'
      ? '已连接播放器'
      : status === 'connecting'
        ? '正在连接播放器…点击重试'
        : '已断开，点击重新连接';

  return (
    <button
      type="button"
      className={cn(
        'flex items-center gap-1.5 rounded-md px-2 py-1 text-xs text-muted-foreground',
        offline && 'hover:bg-accent hover:text-foreground',
        !offline && 'cursor-default',
      )}
      title={label}
      aria-label={label}
      onClick={() => {
        if (offline) reconnect();
      }}
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
      <span className="hidden sm:inline">
        {status === 'connected' ? '已连接' : status === 'connecting' ? '连接中' : '已断开'}
      </span>
    </button>
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
  const location = useLocation();
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const qFromUrl = location.pathname === '/search' ? params.get('q') ?? '' : '';
  const [q, setQ] = useState(qFromUrl);

  useEffect(() => {
    setQ(qFromUrl);
  }, [qFromUrl]);

  const goSearch = (value: string) => {
    setQ(value);
    if (location.pathname === '/search') {
      navigate(value ? `/search?q=${encodeURIComponent(value)}` : '/search', { replace: true });
      return;
    }
    if (value.trim()) {
      navigate(`/search?q=${encodeURIComponent(value)}`);
    }
  };

  return (
    <header className="flex h-14 items-center gap-3 border-b border-border px-4">
      <form
        className="relative max-w-md flex-1"
        onSubmit={(e) => {
          e.preventDefault();
          const trimmed = q.trim();
          navigate(trimmed ? `/search?q=${encodeURIComponent(trimmed)}` : '/search');
        }}
      >
        <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground" />
        <Input
          placeholder="搜索歌曲、艺人、专辑、歌单"
          className="pl-9"
          type="search"
          autoComplete="off"
          value={q}
          onChange={(e) => goSearch(e.target.value)}
          aria-label="搜索媒体库"
        />
      </form>
      <div className="ml-auto flex items-center gap-2">
        <PlaybackTargetSwitch />
        <ConnectionStatusIcon />
      </div>
    </header>
  );
}
