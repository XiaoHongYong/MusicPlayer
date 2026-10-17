export type Rgba = { r: number; g: number; b: number; a: number };

const HEX = /^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/;

export function isHexColor(value: string): boolean {
    return HEX.test(value);
}

export function parseColor(value: string, path = ""): Rgba {
    if (!HEX.test(value)) {
        throw new Error(`${path}: 非法颜色 "${value}"，需要 #RRGGBB 或 #RRGGBBAA`);
    }
    const h = value.slice(1);
    const n = parseInt(h.slice(0, 6), 16);
    const a = h.length === 8 ? parseInt(h.slice(6, 8), 16) : 255;
    return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255, a };
}

export function colorToCss(c: Rgba): string {
    return `rgba(${c.r},${c.g},${c.b},${(c.a / 255).toFixed(4)})`;
}

export function colorToHex(c: Rgba): string {
    const rgb = [c.r, c.g, c.b].map((n) => n.toString(16).padStart(2, "0")).join("");
    if (c.a >= 255) {
        return `#${rgb}`.toUpperCase();
    }
    return `#${rgb}${c.a.toString(16).padStart(2, "0")}`.toUpperCase();
}

export function mixRgb(a: Rgba, b: Rgba, t: number): Rgba {
    return {
        r: Math.round(a.r + (b.r - a.r) * t),
        g: Math.round(a.g + (b.g - a.g) * t),
        b: Math.round(a.b + (b.b - a.b) * t),
        a: Math.round(a.a + (b.a - a.a) * t),
    };
}
