import { describe, expect, it } from 'vitest';
import { currentLyricsLineIndex } from '@/features/player/utils';

describe('library lyrics filter helper', () => {
  it('treats missing timestamps as unscrolled', () => {
    expect(currentLyricsLineIndex([{ time: null, text: 'x' }], 10)).toBe(-1);
  });
});
