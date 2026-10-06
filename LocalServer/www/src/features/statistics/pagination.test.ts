import { describe, expect, it } from 'vitest';
import { clampPage, pageCount, pageSlice, pageWindow, parseStatsPageSize } from './pagination';

describe('parseStatsPageSize', () => {
  it('accepts allowed sizes and falls back otherwise', () => {
    expect(parseStatsPageSize('15')).toBe(15);
    expect(parseStatsPageSize('100')).toBe(100);
    expect(parseStatsPageSize('12')).toBe(20);
    expect(parseStatsPageSize(null)).toBe(20);
  });
});

describe('pagination window', () => {
  it('clamps page and slices items', () => {
    expect(pageCount(678, 20)).toBe(34);
    expect(clampPage(99, 25, 10)).toBe(3);
    expect(pageSlice(['a', 'b', 'c', 'd', 'e'], 2, 2)).toEqual(['c', 'd']);
  });
});

describe('pageWindow', () => {
  it('lists every page when there are few', () => {
    expect(pageWindow(1, 5)).toEqual([1, 2, 3, 4, 5]);
  });

  it('keeps first, last, and neighbors with ellipsis', () => {
    expect(pageWindow(1, 12)).toEqual([1, 2, 'ellipsis', 12]);
    expect(pageWindow(6, 12)).toEqual([1, 'ellipsis', 5, 6, 7, 'ellipsis', 12]);
    expect(pageWindow(12, 12)).toEqual([1, 'ellipsis', 11, 12]);
  });
});
