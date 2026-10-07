/** 从当前 CSS 主题变量读取可视化用色 */

function cssHslToCanvas(raw: string) {
  const m = raw.trim().match(/^(-?[\d.]+)\s+([\d.]+)%\s+([\d.]+)%/);
  if (!m) return `hsl(${raw.trim()})`;
  return `hsl(${m[1]}, ${m[2]}%, ${m[3]}%)`;
}

function liftHsl(color: string, delta: number) {
  const m = color.match(/hsl\(\s*([\d.]+),\s*([\d.]+)%,\s*([\d.]+)%\s*\)/);
  if (!m) return color;
  const l = Math.max(8, Math.min(92, Number(m[3]) + delta));
  return `hsl(${m[1]}, ${m[2]}%, ${l}%)`;
}

function withAlpha(color: string, alpha: number) {
  const m = color.match(/hsl\(\s*([\d.]+),\s*([\d.]+)%,\s*([\d.]+)%\s*\)/);
  if (!m) return color;
  return `hsla(${m[1]}, ${m[2]}%, ${m[3]}%, ${alpha})`;
}

export function readVisualizerThemeColors() {
  const s = getComputedStyle(document.documentElement);
  const primary = cssHslToCanvas(s.getPropertyValue('--primary'));
  // 柱：主题主色；peak：同色相抬亮，略透明，贴近桌面 PeakColor 观感
  return {
    bar: primary,
    peak: withAlpha(liftHsl(primary, 28), 0.92),
  };
}
