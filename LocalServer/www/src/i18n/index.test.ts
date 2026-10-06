import { describe, expect, it } from 'vitest';
import { interpolate, translate } from './index';

describe('interpolate', () => {
  it('replaces named placeholders', () => {
    expect(interpolate('Added {n} songs to "{name}"', { n: 3, name: 'Fav' })).toBe(
      'Added 3 songs to "Fav"',
    );
  });

  it('keeps unknown placeholders', () => {
    expect(interpolate('Hello {name}', {})).toBe('Hello {name}');
  });
});

describe('translate', () => {
  it('falls back to the English key', () => {
    expect(translate('en', 'Play')).toBe('Play');
    expect(translate('zh-CN', '__missing_key__')).toBe('__missing_key__');
  });
});
