/** 内置矢量图标，坐标在 0..24 的 viewBox 内，由 Compiler 缩放到格子。 */

const PATHS: Record<string, { d: string; stroke?: boolean }> = {
    play: { d: "M8 5 L19 12 L8 19 Z" },
    "triangle-right": { d: "M8 5 L19 12 L8 19 Z" },
    pause: { d: "M7 5 h4 v14 h-4 z M13 5 h4 v14 h-4 z" },
    "pause-bars": { d: "M7 5 h4 v14 h-4 z M13 5 h4 v14 h-4 z" },
    prev: { d: "M6 6 h2.2 v12 H6 z M9.5 12 L18 6 v12 z" },
    next: { d: "M15.8 6 H18 v12 h-2.2 z M6 6 L14.5 12 L6 18 z" },
    check: { d: "M5 12 L10 17 L19 7", stroke: true },
    search: { d: "M10.5 10.5 m-5.5 0 a5.5 5.5 0 1 1 11 0 a5.5 5.5 0 1 1 -11 0 M14.5 14.5 L19 19", stroke: true },
    list: { d: "M5 7 h14 M5 12 h14 M5 17 h14", stroke: true },
    lyric: { d: "M8 17 a3 3 0 1 0 0.1 0 M11 17 V6 l7 2 v3", stroke: true },
    "chevron-right": { d: "M9 6 L15 12 L9 18", stroke: true },
    "chevron-up": { d: "M6 15 L12 9 L18 15", stroke: true },
    "chevron-down": { d: "M6 9 L12 15 L18 9", stroke: true },
};

export function resolveIconName(name: string): string | undefined {
    if (name in PATHS || name in FRAGMENTS) {
        return name;
    }
    return undefined;
}

const FRAGMENTS: Record<string, (color: string) => string> = {
    // 媒体中心：四宫格入口图标
    "media-center": (c) =>
        `<rect x="4.5" y="4.5" width="6.2" height="6.2" rx="1.2" fill="none" stroke="${c}" stroke-width="1.7"/>` +
        `<rect x="13.3" y="4.5" width="6.2" height="6.2" rx="1.2" fill="none" stroke="${c}" stroke-width="1.7"/>` +
        `<rect x="4.5" y="13.3" width="6.2" height="6.2" rx="1.2" fill="none" stroke="${c}" stroke-width="1.7"/>` +
        `<rect x="13.3" y="13.3" width="6.2" height="6.2" rx="1.2" fill="none" stroke="${c}" stroke-width="1.7"/>`,
    volume: (c) =>
        `<path d="M3 9 h3.5 l5 -4 v14 l-5 -4 H3 z" fill="${c}"/>` +
        `<path d="M14.2 9.2 a3.2 3.2 0 0 1 0 5.6" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/>` +
        `<path d="M16.6 7.2 a6 6 0 0 1 0 9.6" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/>`,
    minimize: (c) => `<line x1="5" y1="17" x2="19" y2="17" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/>`,
    maximize: (c) => `<rect x="6" y="6" width="12" height="12" fill="none" stroke="${c}" stroke-width="2"/>`,
    restore: (c) =>
        `<rect x="9" y="4" width="10" height="10" fill="none" stroke="${c}" stroke-width="1.8"/>` +
        `<rect x="5" y="8" width="10" height="10" fill="none" stroke="${c}" stroke-width="1.8"/>`,
    close: (c) =>
        `<line x1="6" y1="6" x2="18" y2="18" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/>` +
        `<line x1="18" y1="6" x2="6" y2="18" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/>`,
    "file-new": (c) =>
        `<path d="M7 4 h7 l4 4 v12 h-11 z" fill="none" stroke="${c}" stroke-width="1.7" stroke-linejoin="round"/>` +
        `<path d="M14 4 v4 h4" fill="none" stroke="${c}" stroke-width="1.7" stroke-linejoin="round"/>` +
        `<path d="M12 11 v6 M9 14 h6" stroke="${c}" stroke-width="1.7" stroke-linecap="round"/>`,
    "file-open": (c) =>
        `<path d="M4 8 h6 l2 2 h8 v8 h-16 z" fill="none" stroke="${c}" stroke-width="1.7" stroke-linejoin="round"/>`,
    "file-save": (c) =>
        `<path d="M5 5 h12 l3 3 v11 h-15 z" fill="none" stroke="${c}" stroke-width="1.7" stroke-linejoin="round"/>` +
        `<rect x="8" y="13" width="8" height="6" fill="none" stroke="${c}" stroke-width="1.5"/>` +
        `<rect x="8" y="7" width="6" height="4" fill="${c}"/>`,
    "file-save-as": (c) =>
        `<path d="M5 5 h10 l3 3 v11 h-13 z" fill="none" stroke="${c}" stroke-width="1.6" stroke-linejoin="round"/>` +
        `<path d="M14 14 l4 4 m0 0 l-2.2 -0.4 m2.2 0.4 l-0.4 -2.2" fill="none" stroke="${c}" stroke-width="1.6" stroke-linecap="round"/>`,
    stop: (c) => `<rect x="7" y="7" width="10" height="10" rx="1.2" fill="${c}"/>`,
    "jump-line": (c) =>
        `<path d="M5 7 h14 M5 12 h8 M5 17 h14" stroke="${c}" stroke-width="1.7" stroke-linecap="round"/>` +
        `<path d="M15 9.5 L20 12 L15 14.5 z" fill="${c}"/>`,
    "insert-tag-sm": (c) =>
        `<rect x="4" y="7" width="16" height="10" rx="2" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<path d="M9 12 h6 M12 9 v6" stroke="${c}" stroke-width="1.5" stroke-linecap="round"/>`,
    "lyric-prev": (c) =>
        `<path d="M6 14 L12 8 L18 14" fill="none" stroke="${c}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>`,
    "lyric-next": (c) =>
        `<path d="M6 10 L12 16 L18 10" fill="none" stroke="${c}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>`,
    "toolbar-toggle": (c) =>
        `<rect x="5" y="6" width="14" height="12" rx="2" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<path d="M8 11 L12 15 L16 11" fill="none" stroke="${c}" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>`,
    help: (c) =>
        `<circle cx="12" cy="12" r="8" fill="none" stroke="${c}" stroke-width="1.7"/>` +
        `<path d="M9.6 10 a2.5 2.5 0 1 1 2.6 2.6 v1.4" fill="none" stroke="${c}" stroke-width="1.6" stroke-linecap="round"/>` +
        `<circle cx="12" cy="17.2" r="0.95" fill="${c}"/>`,
    cut: (c) =>
        `<circle cx="8" cy="16.5" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<circle cx="16" cy="16.5" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<path d="M9.8 14.8 L18 5 M14.2 14.8 L6 5" fill="none" stroke="${c}" stroke-width="1.6" stroke-linecap="round"/>`,
    copy: (c) =>
        `<rect x="8" y="6" width="10" height="12" rx="1.4" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<rect x="5" y="9" width="10" height="12" rx="1.4" fill="none" stroke="${c}" stroke-width="1.6"/>`,
    paste: (c) =>
        `<rect x="6" y="8" width="12" height="12" rx="1.5" fill="none" stroke="${c}" stroke-width="1.6"/>` +
        `<rect x="9" y="5" width="6" height="4" rx="1" fill="none" stroke="${c}" stroke-width="1.5"/>`,
    undo: (c) =>
        `<path d="M8 10 H7 a5 5 0 1 1 0 0.1" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/>` +
        `<path d="M8 7 L5 10 L8 13" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>`,
    redo: (c) =>
        `<path d="M16 10 H17 a5 5 0 1 0 0 0.1" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/>` +
        `<path d="M16 7 L19 10 L16 13" fill="none" stroke="${c}" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>`,
    replace: (c) =>
        `<path d="M7 8 h7 M7 12 h5" stroke="${c}" stroke-width="1.7" stroke-linecap="round"/>` +
        `<path d="M16 7 l3 3 -3 3" fill="none" stroke="${c}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>` +
        `<path d="M19 16 l-3 3 3 3" fill="none" stroke="${c}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>`,
};

export function insertTagStripSvg(w: number, h: number, color: string): string {
    const c = color;
    return `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">
  <rect x="3" y="${(h - 16) / 2}" width="${w - 6}" height="16" rx="4" fill="none" stroke="${c}" stroke-width="1.5"/>
  <circle cx="${w * 0.32}" cy="${h / 2}" r="5" fill="none" stroke="${c}" stroke-width="1.4"/>
  <path d="M${w * 0.32} ${h / 2 - 3} v3 h3" fill="none" stroke="${c}" stroke-width="1.3" stroke-linecap="round"/>
  <path d="M${w * 0.52} ${h / 2 - 3.5} h${w * 0.32} M${w * 0.52} ${h / 2} h${w * 0.26} M${w * 0.52} ${h / 2 + 3.5} h${w * 0.32}"
        stroke="${c}" stroke-width="1.3" stroke-linecap="round"/>
</svg>`;
}

export const BUILTIN_ICONS: Record<string, string> = Object.fromEntries(
    Object.keys(PATHS).concat(Object.keys(FRAGMENTS)).map((k) => [k, k]),
);

export function iconSvg(name: string, color: string, size: number, inset = 0.22): string {
    const pad = size * inset;
    const inner = size - pad * 2;
    let body = "";
    if (FRAGMENTS[name]) {
        body = FRAGMENTS[name](color);
    } else {
        const spec = PATHS[name];
        if (!spec) {
            return "";
        }
        const attr = spec.stroke
            ? `fill="none" stroke="${color}" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"`
            : `fill="${color}"`;
        body = `<path d="${spec.d}" ${attr}/>`;
    }
    return `<svg x="${pad}" y="${pad}" width="${inner}" height="${inner}" viewBox="0 0 24 24" overflow="visible">${body}</svg>`;
}
