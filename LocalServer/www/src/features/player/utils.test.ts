import { describe, expect, it } from 'vitest';
import { currentLyricsLineIndex, shouldRecordPlayHistory } from './utils';

describe('currentLyricsLineIndex', () => {
  const lines = [
    { time: 0, text: 'a' },
    { time: 12, text: 'b' },
    { time: 15.5, text: 'c' },
  ];

  it('returns -1 for empty lyrics', () => {
    expect(currentLyricsLineIndex([], 1)).toBe(-1);
  });

  it('highlights the latest line whose time has passed', () => {
    expect(currentLyricsLineIndex(lines, 0)).toBe(0);
    expect(currentLyricsLineIndex(lines, 12.1)).toBe(1);
    expect(currentLyricsLineIndex(lines, 20)).toBe(2);
  });
});

describe('shouldRecordPlayHistory', () => {
  it('records after 10 seconds', () => {
    expect(shouldRecordPlayHistory({ reported: false, position: 10, duration: 200 })).toBe(true);
  });

  it('records at 20% progress', () => {
    expect(shouldRecordPlayHistory({ reported: false, position: 40, duration: 200 })).toBe(true);
  });

  it('skips short previews and already reported plays', () => {
    expect(shouldRecordPlayHistory({ reported: false, position: 5, duration: 200 })).toBe(false);
    expect(shouldRecordPlayHistory({ reported: true, position: 30, duration: 200 })).toBe(false);
  });
});
