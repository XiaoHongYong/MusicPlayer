import type { LyricsLine } from '@/api/types';

/** 有效播放：连续听到约 10 秒，或进度达到时长的 20%。 */
export function shouldRecordPlayHistory(opts: {
  reported: boolean;
  position: number;
  duration: number;
  minSeconds?: number;
  minRatio?: number;
}) {
  if (opts.reported) return false;
  const minSeconds = opts.minSeconds ?? 10;
  const minRatio = opts.minRatio ?? 0.2;
  if (opts.position >= minSeconds) return true;
  return opts.duration > 0 && opts.position / opts.duration >= minRatio;
}

/** 根据播放进度（秒）找到当前歌词行。无时间轴时返回 -1。 */
export function currentLyricsLineIndex(lines: LyricsLine[] | undefined, positionSec: number) {
  if (!lines?.length) return -1;
  let idx = -1;
  for (let i = 0; i < lines.length; i++) {
    const t = lines[i].time;
    if (t == null) continue;
    if (t <= positionSec) idx = i;
    else break;
  }
  return idx;
}
