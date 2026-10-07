import { describe, expect, it } from 'vitest';
import { smoothToward } from './smooth';

describe('smoothToward', () => {
  it('attacks faster than release', () => {
    const up = smoothToward(0, 1, 0.5, 0.1);
    const down = smoothToward(1, 0, 0.5, 0.1);
    expect(up).toBeCloseTo(0.5);
    expect(down).toBeCloseTo(0.9);
  });
});
