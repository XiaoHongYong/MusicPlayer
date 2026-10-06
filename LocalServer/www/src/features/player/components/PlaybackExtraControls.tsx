import { Repeat, Repeat1, Shuffle, Volume2 } from 'lucide-react';
import { cn } from '@/lib/utils';
import { usePlayerStore } from '../store';
import { useT } from '@/i18n';

export function PlaybackExtraControls({ compact = false }: { compact?: boolean }) {
  const t = useT();
  const shuffle = usePlayerStore((s) => s.shuffle);
  const repeat = usePlayerStore((s) => s.repeat);
  const volume = usePlayerStore((s) => s.volume);
  const setShuffle = usePlayerStore((s) => s.setShuffle);
  const cycleRepeat = usePlayerStore((s) => s.cycleRepeat);
  const setVolume = usePlayerStore((s) => s.setVolume);
  const icon = compact ? 16 : 20;

  return (
    <div className={cn('flex items-center', compact ? 'gap-2' : 'gap-4')}>
      <button
        title={t('Shuffle')}
        className={cn(
          'text-muted-foreground hover:text-foreground',
          shuffle && 'text-primary hover:text-primary',
        )}
        onClick={() => setShuffle(!shuffle)}
      >
        <Shuffle size={icon} />
      </button>
      <button
        title={repeat === 'one' ? t('Repeat one') : repeat === 'all' ? t('Repeat all') : t('Repeat off')}
        className={cn(
          'text-muted-foreground hover:text-foreground',
          repeat !== 'off' && 'text-primary hover:text-primary',
        )}
        onClick={cycleRepeat}
      >
        {repeat === 'one' ? <Repeat1 size={icon} /> : <Repeat size={icon} />}
      </button>
      <div className="flex items-center gap-1.5 text-muted-foreground">
        <Volume2 size={icon} />
        <input
          type="range"
          min={0}
          max={1}
          step={0.01}
          value={volume}
          aria-label={t('Volume')}
          onChange={(e) => setVolume(Number(e.target.value))}
          className={cn('h-1 accent-[hsl(var(--primary))]', compact ? 'w-16' : 'w-28')}
        />
      </div>
    </div>
  );
}
