import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

/** 媒体库存毫秒。未重编的桌面端会原样下发，一首歌几乎不会到 60000 秒。 */
export function mediaDurationSeconds(value: number) {
  if (!Number.isFinite(value) || value <= 0) return 0;
  return value >= 60_000 ? value / 1000 : value;
}

export function formatDuration(seconds: number) {
  if (!Number.isFinite(seconds) || seconds < 0) return '0:00';
  const s = Math.floor(seconds % 60);
  const m = Math.floor(seconds / 60) % 60;
  const h = Math.floor(seconds / 3600);
  if (h > 0) return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  return `${m}:${String(s).padStart(2, '0')}`;
}
