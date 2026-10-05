import { colorToCss, parseColor, type Rgba } from "./color.js";
import { iconSvg, resolveIconName } from "./icons.js";

export interface FillLinear {
    type: "linear";
    angle: number;
    stops: Array<[number, string]>;
}

export type Fill = string | FillLinear | Record<string, unknown>;

export interface Visual {
    fill?: Fill;
    border?: { width?: number; color?: string; radius?: number };
    shadow?: { offset?: [number, number]; blur?: number; spread?: number; color?: string };
    textColor?: string;
    iconColor?: string;
    opacity?: number;
    scale?: number;
    offset?: [number, number];
    layers?: Record<string, unknown>[];
    radius?: number;
    blur?: number;
    noise?: number;
    highlight?: { opacity?: number };
    icon?: string;
    /** 图标相对格子的内边距比例(0~0.45)，默认 0.22；越小图标越大 */
    iconInset?: number;
}

let gradSeq = 0;
let filterSeq = 0;
let clipSeq = 0;

function nextId(prefix: string): string {
    if (prefix === "g") {
        return `g${++gradSeq}`;
    }
    if (prefix === "f") {
        return `f${++filterSeq}`;
    }
    return `c${++clipSeq}`;
}

export function resetSvgIds(): void {
    gradSeq = 0;
    filterSeq = 0;
    clipSeq = 0;
}

function cssColor(value: string, fallback = "#FFFFFF"): string {
    try {
        return colorToCss(parseColor(value));
    } catch {
        if (typeof value === "string" && value.startsWith("#")) {
            return value;
        }
        return fallback;
    }
}

function linearGradientDef(fill: FillLinear): { id: string; def: string } {
    const id = nextId("g");
    const angle = ((fill.angle ?? 180) * Math.PI) / 180;
    // CSS 0deg = to top; SVG x1,y1 → x2,y2
    const x1 = 0.5 - Math.sin(angle) / 2;
    const y1 = 0.5 + Math.cos(angle) / 2;
    const x2 = 0.5 + Math.sin(angle) / 2;
    const y2 = 0.5 - Math.cos(angle) / 2;
    const stops = fill.stops.map(([o, c]) => `<stop offset="${o}" stop-color="${cssColor(c)}"/>`).join("");
    return {
        id,
        def: `<linearGradient id="${id}" x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}">${stops}</linearGradient>`,
    };
}

function paint(fill: Fill | undefined, defs: string[]): string {
    if (!fill) {
        return "none";
    }
    if (typeof fill === "string") {
        return cssColor(fill);
    }
    if ((fill as FillLinear).type === "linear") {
        const g = linearGradientDef(fill as FillLinear);
        defs.push(g.def);
        return `url(#${g.id})`;
    }
    return "none";
}

function insetBox(w: number, h: number, inset: number | number[] | undefined): { x: number; y: number; w: number; h: number } {
    if (inset == null) {
        return { x: 0, y: 0, w, h };
    }
    if (typeof inset === "number") {
        return { x: inset, y: inset, w: w - inset * 2, h: h - inset * 2 };
    }
    const [t, r, b, l] = inset.length === 4 ? inset : [inset[0], inset[0], inset[0], inset[0]];
    return { x: l, y: t, w: w - l - r, h: h - t - b };
}

function renderLayers(layers: Record<string, unknown>[], w: number, h: number, defs: string[]): string {
    return layers.map((layer) => renderLayer(layer, w, h, defs)).join("");
}

function renderLayer(layer: Record<string, unknown>, w: number, h: number, defs: string[]): string {
    const type = String(layer.type);
    const box = insetBox(w, h, layer.inset as number | undefined);
    const r = typeof layer.radius === "number" ? layer.radius : 0;

    switch (type) {
        case "rect":
            return `<rect x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}" fill="${paint(layer.fill as Fill, defs)}"/>`;
        case "roundedRect":
            return `<rect x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}" rx="${r}" ry="${r}" fill="${paint(layer.fill as Fill, defs)}"/>`;
        case "circle": {
            const c = (layer.center as number[] | undefined) ?? [w / 2, h / 2];
            const rad = typeof layer.r === "number" ? layer.r : Math.min(w, h) / 2;
            return `<circle cx="${c[0]}" cy="${c[1]}" r="${rad}" fill="${paint(layer.fill as Fill, defs)}"/>`;
        }
        case "path": {
            const stroke = layer.stroke && typeof layer.stroke === "object"
                ? `stroke="${cssColor(String((layer.stroke as Record<string, unknown>).color ?? "#000"))}" stroke-width="${(layer.stroke as Record<string, unknown>).width ?? 1}" fill="none"`
                : `fill="${paint(layer.fill as Fill, defs)}"`;
            return `<path d="${layer.d ?? ""}" ${stroke}/>`;
        }
        case "line": {
            const from = (layer.from as number[]) ?? [0, 0];
            const to = (layer.to as number[]) ?? [w, 0];
            return `<line x1="${from[0]}" y1="${from[1]}" x2="${to[0]}" y2="${to[1]}" stroke="${cssColor(String(layer.color ?? "#000"))}" stroke-width="${layer.width ?? 1}"/>`;
        }
        case "gradient":
            return `<rect x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}" rx="${r}" fill="${paint(layer as Fill, defs)}"/>`;
        case "border": {
            const bw = Number(layer.width ?? 1);
            const rr = typeof layer.radius === "number" ? layer.radius : r;
            return `<rect x="${box.x + bw / 2}" y="${box.y + bw / 2}" width="${Math.max(0, box.w - bw)}" height="${Math.max(0, box.h - bw)}" rx="${Math.max(0, rr - bw / 2)}" fill="none" stroke="${cssColor(String(layer.color ?? "#fff"))}" stroke-width="${bw}"/>`;
        }
        case "highlight": {
            const op = Number(layer.opacity ?? 0.15);
            return `<rect x="${box.x + 1}" y="${box.y + 1}" width="${Math.max(0, box.w - 2)}" height="${Math.max(1, box.h * 0.35)}" rx="${r}" fill="rgba(255,255,255,${op})"/>`;
        }
        case "shadow": {
            const sh = (layer.shadow as Record<string, unknown>) ?? {};
            const id = nextId("f");
            const off = (sh.offset as number[]) ?? [0, 2];
            defs.push(`<filter id="${id}" x="-50%" y="-50%" width="200%" height="200%"><feDropShadow dx="${off[0]}" dy="${off[1]}" stdDeviation="${(Number(sh.blur ?? 8) / 2).toFixed(2)}" flood-color="${cssColor(String(sh.color ?? "#00000040"))}"/></filter>`);
            return `<rect x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}" rx="${r}" fill="transparent" filter="url(#${id})"/>`;
        }
        case "blur": {
            const id = nextId("f");
            defs.push(`<filter id="${id}"><feGaussianBlur stdDeviation="${Number(layer.radius ?? 4) / 2}"/></filter>`);
            return `<g filter="url(#${id})">${renderLayers((layer.layers as Record<string, unknown>[]) ?? [], w, h, defs)}</g>`;
        }
        case "clip": {
            const id = nextId("c");
            const d = typeof layer.path === "string" ? layer.path : undefined;
            const clipInner = d
                ? `<path d="${d}" fill="white"/>`
                : `<rect x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}" rx="${r}" fill="white"/>`;
            // Sharp/libvips 对 clipPath 支持不稳，用 mask 裁圆角
            defs.push(`<mask id="${id}">${clipInner}</mask>`);
            return `<g mask="url(#${id})">${renderLayers((layer.layers as Record<string, unknown>[]) ?? [], w, h, defs)}</g>`;
        }
        case "mask": {
            const id = nextId("c");
            const m = (layer.mask as Record<string, unknown>) ?? {};
            const inset = Number(m.inset ?? 0);
            const mr = typeof m.radius === "number" ? m.radius : r;
            const inner = m.shape === "circle"
                ? `<circle cx="${w / 2}" cy="${h / 2}" r="${Math.min(w, h) / 2 - inset}" fill="white"/>`
                : `<rect x="${inset}" y="${inset}" width="${w - inset * 2}" height="${h - inset * 2}" rx="${mr}" fill="white"/>`;
            defs.push(`<mask id="${id}"><rect width="${w}" height="${h}" fill="black"/>${inner}</mask>`);
            return `<g mask="url(#${id})">${renderLayers((layer.layers as Record<string, unknown>[]) ?? [], w, h, defs)}</g>`;
        }
        case "text": {
            const align = String(layer.align ?? "center");
            const anchor = align.includes("left") ? "start" : align.includes("right") ? "end" : "middle";
            const x = anchor === "start" ? box.x + 4 : anchor === "end" ? box.x + box.w - 4 : box.x + box.w / 2;
            const ty = (layer.typography as Record<string, unknown>) ?? {};
            const size = Number(ty.size ?? 13);
            return `<text x="${x}" y="${box.y + box.h / 2}" text-anchor="${anchor}" dominant-baseline="middle" font-size="${size}" fill="${cssColor(String(layer.color ?? "#000"))}">${escapeXml(String(layer.content ?? ""))}</text>`;
        }
        case "image":
            return "";
        default:
            return "";
    }
}

function escapeXml(s: string): string {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function synthesizedLayers(visual: Visual, w: number, h: number): Record<string, unknown>[] {
    if (visual.layers && visual.layers.length) {
        return visual.layers;
    }
    const layers: Record<string, unknown>[] = [];
    const r = visual.radius ?? 0;
    if (visual.shadow) {
        layers.push({ type: "shadow", shadow: visual.shadow, radius: r });
    }
    if (visual.fill) {
        layers.push({ type: "roundedRect", fill: visual.fill, radius: r });
    }
    if (visual.border) {
        layers.push({ type: "border", width: visual.border.width ?? 1, color: visual.border.color ?? "#ffffff55", radius: visual.border.radius ?? r });
    }
    if (visual.highlight) {
        layers.push({ type: "highlight", opacity: visual.highlight.opacity ?? 0.15, radius: r });
    }
    if (visual.noise && visual.noise > 0) {
        layers.push({ type: "rect", fill: `rgba(255,255,255,${visual.noise * 0.25})` });
    }
    if (!layers.length) {
        layers.push({ type: "roundedRect", fill: "#ffffff00", radius: r });
    }
    void w;
    void h;
    return layers;
}

export function visualToSvg(visual: Visual, w: number, h: number, opts?: {
    icon?: string;
    maskOnly?: boolean;
    iconInset?: number;
    /** 仅平移图标(不带动背景/指示条)，用于 Tab 等把图标摆到指示条旁的内容区中心 */
    iconOffset?: [number, number];
}): string {
    resetSvgIds();
    const defs: string[] = [];
    const iconName = opts?.icon ?? visual.icon;
    const scale = visual.scale ?? 1;
    const [ox, oy] = visual.offset ?? [0, 0];
    const [iox, ioy] = opts?.iconOffset ?? [0, 0];
    const opacity = visual.opacity ?? 1;
    const cx = w / 2;
    const cy = h / 2;
    const iconInset = typeof opts?.iconInset === "number" ? opts.iconInset
        : (typeof visual.iconInset === "number" ? visual.iconInset : 0.22);

    const iconSide = Math.min(w, h);
    // iconSvg 画的是贴在 (0,0) 的方块；非正方形格子需平移到单元格中心
    const iconOriginX = (w - iconSide) / 2;
    const iconOriginY = (h - iconSide) / 2;

    let layersBody = "";
    let iconBody = "";
    if (opts?.maskOnly) {
        const r = visual.radius ?? 0;
        layersBody = `<rect x="0" y="0" width="${w}" height="${h}" rx="${r}" fill="white"/>`;
        if (iconName && resolveIconName(iconName)) {
            layersBody = `<rect width="${w}" height="${h}" fill="black"/>`;
            iconBody = iconSvg(iconName, "white", iconSide, iconInset);
        }
    } else {
        layersBody = renderLayers(synthesizedLayers(visual, w, h), w, h, defs);
        if (iconName && resolveIconName(iconName)) {
            const ic = cssColor(visual.iconColor ?? visual.textColor ?? "#FFFFFF");
            iconBody = iconSvg(iconName, ic, iconSide, iconInset);
        }
    }

    const transform = `translate(${ox},${oy}) translate(${cx},${cy}) scale(${scale}) translate(${-cx},${-cy})`;
    const ix = iconOriginX + iox;
    const iy = iconOriginY + ioy;
    const iconGroup = iconBody
        ? `<g transform="translate(${ix},${iy})">${iconBody}</g>`
        : "";
    return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" shape-rendering="geometricPrecision">
  <defs>${defs.join("")}</defs>
  <g opacity="${opacity}" transform="${transform}">${layersBody}${iconGroup}</g>
</svg>`;
}

export function frameRingSvg(size: number, round: number, thick: number, color: string, innerColor?: string): string {
    const c = cssColor(color);
    const inner = Math.max(0, round - thick);
    const t = thick;
    return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">
  <path fill="${c}" fill-rule="evenodd"
    d="M${round},0 H${size - round} A${round},${round} 0 0 1 ${size},${round}
       V${size - round} A${round},${round} 0 0 1 ${size - round},${size}
       H${round} A${round},${round} 0 0 1 0,${size - round}
       V${round} A${round},${round} 0 0 1 ${round},0 Z
       M${t + inner},${t} H${size - t - inner}
       A${inner},${inner} 0 0 1 ${size - t},${t + inner}
       V${size - t - inner} A${inner},${inner} 0 0 1 ${size - t - inner},${size - t}
       H${t + inner} A${inner},${inner} 0 0 1 ${t},${size - t - inner}
       V${t + inner} A${inner},${inner} 0 0 1 ${t + inner},${t} Z"/>
  ${innerColor ? `<rect x="${t}" y="${t}" width="${size - t * 2}" height="${size - t * 2}" rx="${inner}" fill="none" stroke="${cssColor(innerColor)}" stroke-width="0.5"/>` : ""}
</svg>`;
}

export function maskShapeSvg(w: number, h: number, shape: "roundedRect" | "circle", radius: number, inset: number): string {
    const innerW = w - inset * 2;
    const innerH = h - inset * 2;
    const body = shape === "circle"
        ? `<circle cx="${w / 2}" cy="${h / 2}" r="${Math.min(innerW, innerH) / 2}" fill="white"/>`
        : `<rect x="${inset}" y="${inset}" width="${innerW}" height="${innerH}" rx="${radius}" fill="white"/>`;
    return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">
  ${body}
</svg>`;
}

export function parseRgbaSafe(value: unknown): Rgba | undefined {
    if (typeof value !== "string") {
        return undefined;
    }
    try {
        return parseColor(value);
    } catch {
        return undefined;
    }
}
