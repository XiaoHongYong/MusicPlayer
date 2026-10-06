import { describe, expect, it } from 'vitest';
import { clickFilterFromEvent, cssHslToCanvas, liftHsl, parseChartClick } from './chartClick';

describe('cssHslToCanvas', () => {
  it('adds commas so canvas can parse modern hsl vars', () => {
    expect(cssHslToCanvas('263 70% 66%')).toBe('hsl(263, 70%, 66%)');
  });
});

describe('liftHsl', () => {
  it('raises lightness for hover', () => {
    expect(liftHsl('hsl(263, 70%, 66%)', 12)).toBe('hsl(263, 70%, 78%)');
  });
});

describe('clickFilterFromEvent', () => {
  it('falls back to dataIndex when data is a raw value', () => {
    expect(
      clickFilterFromEvent({ componentType: 'series', data: 12, dataIndex: 1, name: 'Rock' }, [
        { key: 'Pop', count: 3 },
        { key: 'Rock', count: 12 },
      ], 'genre'),
    ).toEqual({ dim: 'genre', key: 'Rock' });
  });
});

describe('parseChartClick', () => {
  it('reads dim/key from series data objects', () => {
    expect(
      parseChartClick({
        componentType: 'series',
        data: { value: 15, name: 'Dance', key: 'Dance', dim: 'genre' },
        name: 'Dance',
      }),
    ).toEqual({ dim: 'genre', key: 'Dance' });
  });

  it('uses key when axis name is a display label', () => {
    expect(
      parseChartClick({
        componentType: 'series',
        data: { value: 677, name: '0', key: '0', dim: 'rating' },
        name: 'Unrated',
      }),
    ).toEqual({ dim: 'rating', key: '0' });
  });

  it('ignores axis clicks', () => {
    expect(parseChartClick({ componentType: 'xAxis', name: '2010' })).toBeNull();
  });
});
