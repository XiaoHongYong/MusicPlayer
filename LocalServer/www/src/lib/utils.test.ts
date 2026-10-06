import { describe, expect, it } from 'vitest';
import { formatDuration, mediaDurationSeconds } from './utils';

describe('mediaDurationSeconds', () => {
  it('converts library milliseconds and keeps API seconds', () => {
    expect(mediaDurationSeconds(252000)).toBe(252);
    expect(mediaDurationSeconds(245)).toBe(245);
    expect(mediaDurationSeconds(0)).toBe(0);
  });
});

describe('formatDuration', () => {
  it('formats minutes without treating them as hours', () => {
    expect(formatDuration(252)).toBe('4:12');
    expect(formatDuration(65)).toBe('1:05');
  });
});
