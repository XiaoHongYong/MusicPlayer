/** attack 快 / release 慢 */
export function smoothToward(current: number, target: number, attack = 0.75, release = 0.4): number {
  const coef = target > current ? attack : release;
  return current + (target - current) * coef;
}

export function smoothSpectrum(
  current: Float32Array,
  target: Float32Array,
  attack = 0.75,
  release = 0.4,
): Float32Array {
  const n = Math.min(current.length, target.length);
  const out = current.length === n ? current : new Float32Array(n);
  for (let i = 0; i < n; i++) {
    out[i] = smoothToward(out[i] ?? 0, target[i] ?? 0, attack, release);
  }
  return out;
}
