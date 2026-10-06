import { useEffect, useState } from 'react';
import { useLocation, useNavigate, useSearchParams } from 'react-router-dom';
import { Monitor, Radio, Search } from 'lucide-react';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';
import { useRealtimeStatus } from '@/features/realtime/useRealtimeStatus';
import { usePlayerStore } from '@/features/player/store';
import { useUiStore, type PlaybackTarget } from '@/stores/ui-store';
import { parseSearchTab, searchHref } from '@/features/search/match';
import { useT } from '@/i18n';

function ConnectionStatusIcon() {
  const t = useT();
  const { status, reconnect } = useRealtimeStatus();
  const offline = status !== 'connected';
  const label =
    status === 'connected'
      ? t('Connected to player')
      : status === 'connecting'
        ? t('Connecting to player… tap to retry')
        : t('Disconnected, tap to reconnect');

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
        {status === 'connected' ? t('Connected') : status === 'connecting' ? t('Connecting') : t('Disconnected')}
      </span>
    </button>
  );
}

function PlaybackTargetSwitch() {
  const t = useT();
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
      title={t('Choose where to play: this browser tab, or the desktop MusicPlayer')}
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
        {t('Browser')}
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
        {t('Player')}
      </button>
    </div>
  );
}

export function TopBar() {
  const t = useT();
  const location = useLocation();
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const qFromUrl = location.pathname === '/search' ? params.get('q') ?? '' : '';
  const [q, setQ] = useState(qFromUrl);

  useEffect(() => {
    setQ(qFromUrl);
  }, [qFromUrl]);

  const tab = location.pathname === '/search' ? parseSearchTab(params.get('tab')) : 'songs';

  const goSearch = (value: string) => {
    setQ(value);
    if (location.pathname === '/search') {
      navigate(searchHref(value, tab), { replace: true });
      return;
    }
    if (value.trim()) {
      navigate(searchHref(value));
    }
  };

  return (
    <header className="flex h-14 items-center gap-3 border-b border-border px-4">
      <form
        className="relative max-w-md flex-1"
        onSubmit={(e) => {
          e.preventDefault();
          const trimmed = q.trim();
          navigate(trimmed ? searchHref(trimmed, tab) : searchHref('', tab));
        }}
      >
        <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground" />
        <Input
          placeholder={t('Search songs, artists, albums, playlists')}
          className="pl-9"
          type="search"
          autoComplete="off"
          value={q}
          onChange={(e) => goSearch(e.target.value)}
          aria-label={t('Search library')}
        />
      </form>
      <div className="ml-auto flex items-center gap-2">
        <PlaybackTargetSwitch />
        <ConnectionStatusIcon />
      </div>
    </header>
  );
}
