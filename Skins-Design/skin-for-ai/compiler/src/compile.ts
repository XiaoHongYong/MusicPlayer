import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import sharp from "sharp";
import { colorToHex, parseColor, type Rgba } from "./color.js";
import { insertTagStripSvg, resolveIconName } from "./icons.js";
import { compositeSheet, extendPos, hashJson, kebabName, rasterSvg, renderSvgToPair, savePngPair, SS, type SavedPng } from "./raster.js";
import { resolveSkin, validateLayerTypes, type ResolveIssue } from "./refs.js";
import { assertComponentType, formatZodError, SkinJsonSchema, type ComponentType } from "./schema.js";
import { frameRingSvg, maskShapeSvg, visualToSvg, type Visual } from "./svg.js";

export interface CompileOptions {
    inputPath: string;
    outDir?: string;
    force?: boolean;
    /** 只出 PNG，不覆盖已有 Styles.xml / main.xml / theme.xml（迁移手写皮肤时用） */
    pngOnly?: boolean;
}

export interface CompileReport {
    skin: string;
    outDir: string;
    files: Array<{
        file: string;
        component: string;
        width: number;
        height: number;
        horzExtendPos?: string;
        vertExtendPos?: string;
    }>;
    warnings: string[];
    errors: string[];
}

function defaultSkinsDir(): string {
    return path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../../skins");
}

function fileBase(comp: Record<string, unknown>, type: ComponentType, name: string): string {
    if (typeof comp.file === "string" && comp.file.trim()) {
        return kebabName(String(comp.file).replace(/\.png$/i, ""));
    }
    return fileBaseFor(type, name, comp);
}

const REQUIRED_TYPES: ComponentType[] = [
    "windowBackground",
    "captionToolbar",
    "toggleButton",
    "slider",
    "playlist",
];

function asVisual(obj: Record<string, unknown> | undefined): Visual {
    if (!obj) {
        return {};
    }
    return obj as Visual;
}

function stateOf(comp: Record<string, unknown>, name: string, issues: ResolveIssue[], path: string): Visual {
    const states = (comp.states as Record<string, Record<string, unknown>> | undefined) ?? {};
    if (states[name]) {
        return asVisual({ ...comp, ...states[name], states: undefined, type: undefined });
    }
    if (name !== "normal" && states.normal) {
        issues.push({ path, message: `状态 ${name} 未定义，回退到 normal`, level: "warn" });
        return asVisual({ ...comp, ...states.normal, states: undefined, type: undefined });
    }
    return asVisual(comp);
}

function num(v: unknown, d: number): number {
    return typeof v === "number" && Number.isFinite(v) ? v : d;
}

function sizeOf(comp: Record<string, unknown>, dw: number, dh: number): { width: number; height: number } {
    const s = comp.size as { width?: number; height?: number } | undefined;
    return { width: num(s?.width, dw), height: num(s?.height, dh) };
}

function radiusOf(comp: Record<string, unknown>, visual: Visual, fallback: number): number {
    if (typeof visual.radius === "number") {
        return visual.radius;
    }
    if (typeof comp.radius === "number") {
        return comp.radius;
    }
    return fallback;
}

const TOKEN_COLOR_ALIASES: Record<string, string[]> = {
    fg: ["textPrimary"],
    "fg-muted": ["textSecondary"],
    "fg-subtle": ["textDim", "textSecondary"],
    "fg-on-accent": ["textOnAccent"],
    bg: ["window", "windowTop"],
    "bg-muted": ["windowBottom"],
    "bg-surface": ["surface"],
    "bg-solid": ["surfaceSolid"],
    "bg-chrome": ["chrome"],
    "bg-track": ["track"],
    "bg-overlay": ["overlay"],
    "border-subtle": ["borderMuted", "hairline"],
    "accent-muted": ["accentDeep"],
    "bg-hover": ["hover", "surfaceHover", "accentHover"],
    "fg-hover": ["textHover"],
    "bg-pressed": ["pressed", "surfacePressed"],
    "fg-pressed": ["textPressed"],
    "bg-focus": ["focus"],
    "fg-focus": ["textFocus"],
    "bg-selected": ["selected", "surfaceSelected"],
    "fg-selected": ["textSelected"],
    "bg-disabled": ["disabled", "surfaceDisabled"],
    "fg-disabled": ["textDisabled"],
    "bg-danger": ["danger"],
    "fg-danger": ["textDanger"],
    "bg-danger-pressed": ["danger-pressed"],
    "fg-danger-pressed": ["textDangerPressed"],
};

function tokenColor(tokens: Record<string, unknown>, key: string, fallback: string): string {
    const colors = tokens.colors as Record<string, string> | undefined;
    if (!colors) {
        return fallback;
    }
    const keys = [key, ...(TOKEN_COLOR_ALIASES[key] ?? [])];
    for (const [canon, aliases] of Object.entries(TOKEN_COLOR_ALIASES)) {
        if (aliases.includes(key)) {
            keys.push(canon);
        }
    }
    for (const k of keys) {
        const v = colors[k];
        if (typeof v === "string") {
            return v;
        }
    }
    return fallback;
}

function ident(name: string): string {
    return name.replace(/[^A-Za-z0-9]/g, "") || "Skin";
}

function windowBgVariant(comp: Record<string, unknown>): "main" | "dialog" {
    return comp.variant === "dialog" ? "dialog" : "main";
}

function windowBgDims(comp: Record<string, unknown>, radius: number): {
    w: number; h: number; horz: [number, number]; vert: [number, number]; file: string;
} {
    const chrome = comp.chrome as { caption?: number; bottom?: number; mid?: number } | undefined;
    const caption = typeof chrome?.caption === "number" ? chrome.caption : undefined;
    const bottom = typeof chrome?.bottom === "number" ? chrome.bottom : undefined;
    const mid = num(chrome?.mid, 20);
    const side = windowBgSize(radius);
    const sized = sizeOf(comp, side, side);
    const w = sized.width;
    const h = (caption != null && bottom != null) ? caption + mid + bottom : sized.height;
    const horz = extendPos(w, radius);
    const vert: [number, number] = (caption != null && bottom != null)
        ? [caption, h - bottom]
        : extendPos(h, radius);
    const file = fileBase(comp, "windowBackground", "bg");
    return { w, h, horz, vert, file };
}

function isHorzScrollbar(name: string, comp?: Record<string, unknown>): boolean {
    const ori = String(comp?.orientation ?? "").toLowerCase();
    if (["horizontal", "horz", "h"].includes(ori)) {
        return true;
    }
    if (["vertical", "vert", "v"].includes(ori)) {
        return false;
    }
    const stem = kebabName(typeof comp?.file === "string" ? String(comp.file) : name);
    return /horz|horiz/.test(stem);
}

function fileBaseFor(type: ComponentType, name: string, comp?: Record<string, unknown>): string {
    if (type === "windowBackground") {
        return (comp && windowBgVariant(comp) === "dialog") ? "bg-dialog" : "bg";
    }
    if (type === "windowFrame") {
        return "frame";
    }
    if (type === "captionToolbar") {
        return "caption-btn";
    }
    if (type === "captionBar") {
        return "caption-bg";
    }
    if (type === "toggleButton") {
        const sn = kebabName(name).replace("play-pause", "playpause");
        return sn === "play-pause" ? "playpause" : sn;
    }
    if (type === "slider") {
        return "progress";
    }
    if (type === "checkbox") {
        return "checks";
    }
    if (type === "edit") {
        return "edit";
    }
    if (type === "comboBox") {
        return "combo-box";
    }
    if (type === "albumArt") {
        return "albumart-mask";
    }
    if (type === "panel") {
        return name === "panel" ? "panel" : kebabName(name);
    }
    if (type === "searchBar") {
        return "search-bg";
    }
    if (type === "scrollbar") {
        return isHorzScrollbar(name, comp) ? "scrollbar-horz" : "scrollbar-vert";
    }
    if (type === "toolbar") {
        return kebabName(name) === "view-tabs" || name === "viewTabs" ? "view-tabs" : kebabName(name);
    }
    if (type === "iconStrip") {
        return kebabName(name) === "lyr-tb" || name === "lyrTb" ? "lyr-tb" : kebabName(name);
    }
    if (type === "tabButton") {
        return "button-group";
    }
    return kebabName(name);
}

function xmlEsc(s: string): string {
    return s.replace(/&/g, "&amp;").replace(/"/g, "&quot;").replace(/</g, "&lt;");
}

function opaqueXmlColor(value: unknown, fallback: string, onto: Rgba = { r: 16, g: 24, b: 40, a: 255 }): string {
    const hex = toXmlColor(value, fallback);
    try {
        const c = parseColor(hex);
        if (c.a >= 250) {
            return colorToHex({ ...c, a: 255 });
        }
        const t = c.a / 255;
        return colorToHex({
            r: Math.round(onto.r * (1 - t) + c.r * t),
            g: Math.round(onto.g * (1 - t) + c.g * t),
            b: Math.round(onto.b * (1 - t) + c.b * t),
            a: 255,
        });
    } catch {
        return fallback;
    }
}

function windowBgSize(radius: number): number {
    return Math.max(64, (radius + 1) * 2 + 8);
}

function toXmlColor(value: unknown, fallback: string): string {
    if (typeof value !== "string") {
        return fallback;
    }
    try {
        return colorToHex(parseColor(value));
    } catch {
        return value.startsWith("#") ? value.toUpperCase() : fallback;
    }
}

export async function compileSkin(opts: CompileOptions): Promise<CompileReport> {
    const abs = path.resolve(opts.inputPath);
    const text = await readFile(abs, "utf8");
    const parsedJson = JSON.parse(text) as unknown;
    const parsed = SkinJsonSchema.safeParse(parsedJson);
    if (!parsed.success) {
        throw new Error(`skin.json 校验失败:\n${formatZodError(parsed.error)}`);
    }
    const skin = parsed.data;
    const resolved = resolveSkin(skin);
    const errors = resolved.issues.filter((i) => i.level === "error");
    if (errors.length) {
        throw new Error(errors.map((e) => `${e.path}: ${e.message}`).join("\n"));
    }

    const typesPresent = new Set<ComponentType>();
    for (const [name, comp] of Object.entries(resolved.components)) {
        const t = assertComponentType(comp.type, `components.${name}`);
        typesPresent.add(t);
        validateLayerTypes(comp.layers, `components.${name}.layers`, resolved.issues);
        const states = comp.states as Record<string, Record<string, unknown>> | undefined;
        if (states) {
            for (const [st, v] of Object.entries(states)) {
                validateLayerTypes(v.layers, `components.${name}.states.${st}.layers`, resolved.issues);
            }
        }
    }
    const lateErrors = resolved.issues.filter((i) => i.level === "error");
    if (lateErrors.length) {
        throw new Error(lateErrors.map((e) => `${e.path}: ${e.message}`).join("\n"));
    }

    const skinFolder = ident(skin.meta.name);
    const parent = opts.outDir ? path.resolve(opts.outDir) : defaultSkinsDir();
    const outDir = path.join(parent, skinFolder);
    await mkdir(outDir, { recursive: true });

    const files: CompileReport["files"] = [];
    const warnings = resolved.issues.filter((i) => i.level === "warn").map((i) => `${i.path}: ${i.message}`);
    for (const t of REQUIRED_TYPES) {
        if (!typesPresent.has(t)) {
            warnings.push(`缺少必备组件 type=${t}，生成的皮肤可能不完整（引擎会回退 assets/）`);
        }
    }

    const ctx = {
        outDir,
        files,
        warnings,
        issues: resolved.issues,
        tokens: resolved.tokens,
        force: opts.force ?? false,
        cache: new Map<string, string>(),
    };

    let playlistName = `${skinFolder}Playlist`;
    let roundWidth = 12;
    let thickWidth = 3;
    let captionCell = 16;
    let extendBg: [number, number] = [11, 21];
    let extendBgVert: [number, number] = [11, 21];
    let extendDialog: [number, number] = [11, 21];
    let extendDialogVert: [number, number] = [11, 21];
    let hasDialogBg = false;
    let extendPanel: [number, number] = [11, 21];
    let sliderEndWidth = 8;
    let playlistAttrs: Record<string, unknown> | undefined;

    for (const [name, comp] of Object.entries(resolved.components)) {
        const type = assertComponentType(comp.type, `components.${name}`);
        const saved = await emitComponent(name, type, comp, ctx);
        for (const s of saved) {
            const item: CompileReport["files"][number] = {
                file: s.files[0], component: name, width: s.width, height: s.height,
            };
            if (type === "windowBackground") {
                const d = windowBgDims(comp, radiusOf(comp, asVisual(comp), 12));
                item.horzExtendPos = `${d.horz[0]},${d.horz[1]}`;
                item.vertExtendPos = `${d.vert[0]},${d.vert[1]}`;
            }
            files.push(item);
        }
        if (type === "windowFrame") {
            roundWidth = num(comp.roundWidth, roundWidth);
            thickWidth = num(comp.thickWidth, thickWidth);
        }
        if (type === "windowBackground") {
            const r = radiusOf(comp, asVisual(comp), 12);
            const d = windowBgDims(comp, r);
            if (d.file === "bg-dialog") {
                hasDialogBg = true;
                extendDialog = d.horz;
                extendDialogVert = d.vert;
            } else {
                extendBg = d.horz;
                extendBgVert = d.vert;
            }
        }
        if (type === "panel") {
            const r = radiusOf(comp, asVisual(comp), 10);
            extendPanel = extendPos(32, r);
        }
        if (type === "captionToolbar") {
            captionCell = num((comp.cellSize as number | undefined), 16);
        }
        if (type === "slider") {
            const track = (comp.track as Record<string, unknown> | undefined) ?? {};
            sliderEndWidth = num(track.endWidth, 8);
        }
        if (type === "playlist") {
            playlistName = `${ident(name) === "playlist" ? skinFolder : ident(name)}Playlist`;
            playlistAttrs = comp;
        }
    }

    if (!opts.pngOnly) {
        const styles = buildStylesXml({
            skinFolder,
            tokens: resolved.tokens,
            roundWidth,
            thickWidth,
            captionCell,
            extendBg,
            extendBgVert,
            extendDialog,
            extendDialogVert,
            extendPanel,
            sliderEndWidth,
            playlistName,
            playlist: playlistAttrs,
            hasCaptionBar: typesPresent.has("captionBar"),
            hasBg: typesPresent.has("windowBackground"),
            hasDialogBg,
            hasFrame: typesPresent.has("windowFrame"),
        });
        await writeFile(path.join(outDir, "Styles.xml"), styles, "utf8");
        files.push({ file: "Styles.xml", component: "*", width: 0, height: 0 });

        if (skin.lyrics) {
            const theme = buildThemeXml(skin.lyrics, resolved.tokens);
            await writeFile(path.join(outDir, "theme.xml"), theme, "utf8");
            files.push({ file: "theme.xml", component: "lyrics", width: 0, height: 0 });
        }

        if (skin.layout) {
            const main = buildMainXml(skin.meta.extraResourceFolder, skin.layout, playlistName, extendBg, extendBgVert, typesPresent.has("windowBackground"));
            await writeFile(path.join(outDir, "main.xml"), main, "utf8");
            files.push({ file: "main.xml", component: "layout", width: 0, height: 0 });
        }
    }

    const report: CompileReport = {
        skin: skin.meta.name,
        outDir,
        files,
        warnings,
        errors: [],
    };
    if (!opts.pngOnly) {
        await writeFile(path.join(outDir, "report.json"), JSON.stringify(report, null, 2), "utf8");
    }
    void hashJson;
    return report;
}

interface EmitCtx {
    outDir: string;
    files: CompileReport["files"];
    warnings: string[];
    issues: ResolveIssue[];
    tokens: Record<string, unknown>;
    force: boolean;
    cache: Map<string, string>;
}

async function emitComponent(name: string, type: ComponentType, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    switch (type) {
        case "windowBackground":
            return emitWindowBg(name, comp, ctx);
        case "windowFrame":
            return emitFrame(name, comp, ctx);
        case "captionToolbar":
            return emitCaptionToolbar(name, comp, ctx);
        case "captionBar":
            return emitNineSlice(name, fileBase(comp, "captionBar", name), comp, ctx, 32, 32);
        case "button":
            return emitButton(name, comp, ctx);
        case "toggleButton":
            return emitToggle(name, comp, ctx);
        case "slider":
            return emitSlider(name, comp, ctx);
        case "checkbox":
            return emitChecks(name, comp, ctx);
        case "edit":
            return emitEdit(name, comp, ctx);
        case "comboBox":
            return emitCombo(name, comp, ctx);
        case "menu":
            return emitMenu(name, comp, ctx);
        case "albumArt":
            return emitAlbumArt(name, comp, ctx);
        case "panel":
            return emitNineSlice(name, fileBase(comp, "panel", name), comp, ctx, 32, 32);
        case "searchBar":
            return emitNineSlice(name, fileBase(comp, "searchBar", name), comp, ctx, 48, 28);
        case "scrollbar":
            return emitScrollbar(name, comp, ctx);
        case "toolbar":
            return emitToolbar(name, comp, ctx);
        case "iconStrip":
            return emitIconStrip(name, comp, ctx);
        case "tabButton":
            return emitTabButton(name, comp, ctx);
        case "icon":
            return emitIcon(name, comp, ctx);
        case "sprite":
            return emitSprite(name, comp, ctx);
        case "playlist":
            return [];
        default:
            return [];
    }
}

async function emitWindowBg(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const r = radiusOf(comp, vis, 12);
    const d = windowBgDims(comp, r);
    const chrome = (comp.chrome as Record<string, unknown> | undefined) ?? {};
    const capH = typeof chrome.caption === "number" ? chrome.caption : 0;
    const botH = typeof chrome.bottom === "number" ? chrome.bottom : 0;
    const capFill = typeof chrome.captionFill === "string" ? chrome.captionFill : undefined;
    const botFill = typeof chrome.bottomFill === "string" ? chrome.bottomFill : capFill;
    const hair = typeof chrome.hairlineColor === "string" ? chrome.hairlineColor : undefined;
    const bottomChrome = chrome.bottomChrome !== false;

    const inner: Record<string, unknown>[] = vis.layers?.length
        ? [...vis.layers]
        : (vis.fill ? [{ type: "roundedRect", fill: vis.fill, radius: r }] : []);
    if (vis.highlight) {
        inner.push({ type: "highlight", opacity: vis.highlight.opacity ?? 0.12, radius: r });
    }
    if (capH && capFill) {
        inner.push({ type: "rect", fill: capFill, inset: [0, 0, d.h - capH, 0] });
        if (hair) {
            inner.push({ type: "line", from: [0, capH - 0.5], to: [d.w, capH - 0.5], color: hair, width: 1 });
        }
    }
    if (botH && bottomChrome && botFill) {
        inner.push({ type: "rect", fill: botFill, inset: [d.h - botH, 0, 0, 0] });
        if (hair) {
            inner.push({ type: "line", from: [0, d.h - botH + 0.5], to: [d.w, d.h - botH + 0.5], color: hair, width: 1 });
        }
    }
    if (vis.border) {
        inner.push({
            type: "border",
            width: vis.border.width ?? 2,
            color: vis.border.color,
            radius: r,
        });
    }
    const wrapped: Visual = { layers: [{ type: "clip", radius: r, layers: inner }] };
    const svg = visualToSvg(wrapped, d.w, d.h);
    return [await renderSvgToPair(ctx.outDir, d.file, svg, d.w, d.h)];
}

async function emitFrame(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const s = sizeOf(comp, 64, 64);
    const size = Math.max(s.width, s.height);
    const round = num(comp.roundWidth, radiusOf(comp, vis, 12));
    const thick = num(comp.thickWidth, 3);
    const color = typeof vis.border === "object" && vis.border?.color
        ? String(vis.border.color)
        : tokenColor(ctx.tokens, "border", "#FFFFFF55");
    const svg = vis.layers?.length
        ? visualToSvg({ ...vis, radius: round }, size, size)
        : frameRingSvg(size, round, thick, color);
    return [await renderSvgToPair(ctx.outDir, fileBase(comp, "windowFrame", name), svg, size, size)];
}

async function emitNineSlice(name: string, file: string, comp: Record<string, unknown>, ctx: EmitCtx, dw: number, dh: number): Promise<SavedPng[]> {
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const s = sizeOf(comp, dw, dh);
    vis.radius = radiusOf(comp, vis, 10);
    const svg = visualToSvg(vis, s.width, s.height);
    return [await renderSvgToPair(ctx.outDir, file, svg, s.width, s.height)];
}

const CAPTION_GLYPHS = ["", "minimize", "maximize", "restore", "close"];

async function emitCaptionToolbar(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const cell = num(comp.cellSize as number | undefined, 16);
    const buttons = (comp.buttons as string[] | undefined) ?? ["minimize", "maximize", "restore", "close"];
    const cols = 5;
    const rows = 3;
    const states = ["normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let col = 0; col < cols; col++) {
        const icon = col === 0 ? "" : buttons[col - 1] ?? CAPTION_GLYPHS[col];
        for (let row = 0; row < rows; row++) {
            const vis = stateOf(comp, states[row], ctx.issues, `components.${name}.states.${states[row]}`);
            vis.radius = vis.radius ?? cell / 2;
            if (icon === "close" && states[row] !== "normal") {
                vis.fill = row === 1 ? "#E5484D" : "#B23338";
                vis.iconColor = "#FFFFFF";
            }
            const svg = visualToSvg(vis, cell, cell, { icon: icon && resolveIconName(icon) ? icon : undefined });
            cells.push({ col, row, svg });
        }
    }
    const sheet = await compositeSheet(cols, rows, cell, cell, cells);
    const base = fileBase(comp, "captionToolbar", name);
    const win = await savePngPair(ctx.outDir, base, sheet.hi, sheet.width, sheet.height);
    if (base === "caption-btn") {
        const mac = await emitCaptionToolbarMac(ctx, 16);
        return [win, ...mac];
    }
    return [win];
}

function macTrafficSvg(cell: number, fill: string, glyph: string): string {
    const r = cell / 2 - 2;
    const c = cell / 2;
    const stroke = "#3C3C3C";
    let g = "";
    if (glyph === "min") {
        g = `<line x1="${c - 3}" y1="${c}" x2="${c + 3}" y2="${c}" stroke="${stroke}" stroke-width="1.2"/>`;
    } else if (glyph === "max" || glyph === "restore") {
        g = `<line x1="${c}" y1="${c - 3}" x2="${c}" y2="${c + 3}" stroke="${stroke}" stroke-width="1.2"/>
             <line x1="${c - 3}" y1="${c}" x2="${c + 3}" y2="${c}" stroke="${stroke}" stroke-width="1.2"/>`;
    } else if (glyph === "close") {
        g = `<line x1="${c - 2.5}" y1="${c - 2.5}" x2="${c + 2.5}" y2="${c + 2.5}" stroke="${stroke}" stroke-width="1.2"/>
             <line x1="${c + 2.5}" y1="${c - 2.5}" x2="${c - 2.5}" y2="${c + 2.5}" stroke="${stroke}" stroke-width="1.2"/>`;
    }
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${cell}" height="${cell}">
      <circle cx="${c}" cy="${c}" r="${r}" fill="${fill}"/>${g}</svg>`;
}

async function emitCaptionToolbarMac(ctx: EmitCtx, cell: number): Promise<SavedPng[]> {
    const colors = ["", "#FEBC2E", "#28C840", "#28C840", "#FF5F57"];
    const dark = ["", "#D2961E", "#1C9630", "#1C9630", "#C84640"];
    const glyphs = ["", "min", "max", "restore", "close"];
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let col = 0; col < 5; col++) {
        for (let row = 0; row < 3; row++) {
            const fill = row === 2 ? dark[col] : colors[col];
            const glyph = row === 0 || col === 0 ? "" : glyphs[col];
            const svg = col === 0
                ? `<svg xmlns="http://www.w3.org/2000/svg" width="${cell}" height="${cell}"></svg>`
                : macTrafficSvg(cell, fill, glyph);
            cells.push({ col, row, svg });
        }
    }
    const sheet = await compositeSheet(5, 3, cell, cell, cells);
    return [await savePngPair(ctx.outDir, "caption-btn-mac", sheet.hi, sheet.width, sheet.height)];
}

async function emitButton(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const s = sizeOf(comp, 40, 40);
    const icons = (comp.icons as string[] | undefined) ?? (comp.icon ? [String(comp.icon)] : ["play"]);
    const cols = icons.length;
    const iconInset = typeof comp.iconInset === "number" ? comp.iconInset : undefined;
    const states = ["normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    const maskCells: Array<{ col: number; row: number; svg: string }> = [];
    for (let col = 0; col < cols; col++) {
        const icon = icons[col];
        for (let row = 0; row < 3; row++) {
            const vis = stateOf(comp, states[row], ctx.issues, `components.${name}`);
            vis.radius = radiusOf(comp, vis, s.width / 2);
            if (typeof iconInset === "number") {
                vis.iconInset = iconInset;
            }
            cells.push({ col, row, svg: visualToSvg(vis, s.width, s.height, { icon, iconInset }) });
            maskCells.push({ col, row, svg: visualToSvg({ radius: vis.radius }, s.width, s.height, { maskOnly: true, icon, iconInset }) });
        }
    }
    const base = fileBase(comp, "button", name);
    const sheet = await compositeSheet(cols, 3, s.width, s.height, cells);
    const out = [await savePngPair(ctx.outDir, base, sheet.hi, sheet.width, sheet.height)];
    if (comp.mask === "hitbox") {
        const hit = Math.round(Math.min(s.width, s.height) * 0.55);
        const maskHit: Array<{ col: number; row: number; svg: string }> = [];
        for (let col = 0; col < cols; col++) {
            for (let row = 0; row < 3; row++) {
                const x = (s.width - hit) / 2;
                const y = (s.height - hit) / 2;
                maskHit.push({
                    col, row,
                    svg: `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${s.width}" height="${s.height}"><rect x="${x}" y="${y}" width="${hit}" height="${hit}" fill="white"/></svg>`,
                });
            }
        }
        const mask = await compositeSheet(cols, 3, s.width, s.height, maskHit);
        out.push(await savePngPair(ctx.outDir, `${base}-mask`, mask.hi, mask.width, mask.height));
    } else if (comp.mask === true) {
        const mask = await compositeSheet(cols, 3, s.width, s.height, maskCells);
        out.push(await savePngPair(ctx.outDir, `${base}-mask`, mask.hi, mask.width, mask.height));
    }
    return out;
}

async function emitIcon(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const s = sizeOf(comp, 16, 16);
    vis.radius = radiusOf(comp, vis, 0);
    const icon = typeof comp.icon === "string" ? comp.icon : undefined;
    const iconInset = typeof comp.iconInset === "number" ? comp.iconInset
        : (typeof vis.iconInset === "number" ? vis.iconInset : undefined);
    if (typeof iconInset === "number") {
        vis.iconInset = iconInset;
    }
    const svg = visualToSvg(vis, s.width, s.height, { icon, iconInset });
    return [await renderSvgToPair(ctx.outDir, fileBase(comp, "icon", name), svg, s.width, s.height)];
}

async function emitSprite(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const cell = (comp.cell as { width?: number; height?: number } | undefined) ?? {};
    const cw = num(cell.width, 40);
    const ch = num(cell.height, 26);
    const layout = (comp.layout as string[] | undefined)
        ?? Object.keys((comp.states as Record<string, unknown> | undefined) ?? { normal: true });
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let row = 0; row < layout.length; row++) {
        const vis = stateOf(comp, layout[row], ctx.issues, `components.${name}.states.${layout[row]}`);
        vis.radius = radiusOf(comp, vis, 0);
        const icon = typeof vis.icon === "string" ? vis.icon : (typeof comp.icon === "string" ? String(comp.icon) : undefined);
        cells.push({ col: 0, row, svg: visualToSvg(vis, cw, ch, { icon }) });
    }
    const sheet = await compositeSheet(1, layout.length, cw, ch, cells);
    return [await savePngPair(ctx.outDir, fileBase(comp, "sprite", name), sheet.hi, cw, ch * layout.length)];
}

async function emitToggle(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const s = sizeOf(comp, 48, 48);
    const off = String(comp.iconOff ?? "play");
    const on = String(comp.iconOn ?? "pause");
    const states = ["normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let col = 0; col < 2; col++) {
        const icon = col === 0 ? off : on;
        for (let row = 0; row < 3; row++) {
            const vis = stateOf(comp, states[row], ctx.issues, `components.${name}`);
            vis.radius = radiusOf(comp, vis, s.width / 2);
            cells.push({ col, row, svg: visualToSvg(vis, s.width, s.height, { icon }) });
        }
    }
    const base = fileBase(comp, "toggleButton", name);
    const sheet = await compositeSheet(2, 3, s.width, s.height, cells);
    return [await savePngPair(ctx.outDir, base, sheet.hi, sheet.width, sheet.height)];
}

async function emitSlider(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const track = (comp.track as Record<string, unknown> | undefined) ?? {};
    const thumb = (comp.thumb as Record<string, unknown> | undefined) ?? {};
    const tw = num(track.width, 60);
    const th = num(track.height, 8);
    const half = Math.max(2, Math.floor(th / 2));
    const radius = num(track.radius, half);
    const remaining = asVisual((track.remaining as Record<string, unknown>) ?? { fill: tokenColor(ctx.tokens, "bg-track", "#FFFFFF33") });
    const played = asVisual((track.played as Record<string, unknown>) ?? { fill: tokenColor(ctx.tokens, "accent", "#5AA9FF") });
    remaining.radius = radius;
    played.radius = radius;
    const top = visualToSvg(remaining, tw, half);
    const bot = visualToSvg(played, tw, half);
    const trackSheet = await compositeSheet(1, 2, tw, half, [
        { col: 0, row: 0, svg: top },
        { col: 0, row: 1, svg: bot },
    ]);
    const out: SavedPng[] = [await savePngPair(ctx.outDir, "progress", trackSheet.hi, tw, half * 2)];

    const ts = (thumb.size as { width?: number; height?: number } | undefined) ?? {};
    const tsz = num(ts.width, num(ts.height, 15));
    const tstates = ["normal", "hover", "pressed"] as const;
    const tcells: Array<{ col: number; row: number; svg: string }> = [];
    for (let row = 0; row < 3; row++) {
        const stMap = (thumb.states as Record<string, Record<string, unknown>> | undefined) ?? {};
        let vis: Visual;
        if (stMap[tstates[row]]) {
            vis = asVisual(stMap[tstates[row]]);
        } else if (stMap.normal) {
            vis = asVisual(stMap.normal);
            ctx.warnings.push(`components.${name}.thumb.states.${tstates[row]} 未定义，回退到 normal`);
        } else {
            vis = asVisual(thumb);
        }
        vis.radius = vis.radius ?? tsz / 2;
        tcells.push({ col: 0, row, svg: visualToSvg(vis, tsz, tsz) });
    }
    const thumbSheet = await compositeSheet(1, 3, tsz, tsz, tcells);
    out.push(await savePngPair(ctx.outDir, "thumb", thumbSheet.hi, tsz, tsz * 3));
    return out;
}

async function emitChecks(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const cell = 26;
    const rows = ["disabled", "normal", "pressed", "hover"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let row = 0; row < 4; row++) {
        const vis = stateOf(comp, rows[row], ctx.issues, `components.${name}`);
        vis.radius = 3;
        const fill = (vis.fill as string | undefined) ?? "#FFFFFF";
        const border = vis.border?.color ?? tokenColor(ctx.tokens, "border-subtle", "#9AA3B5");
        const mark = vis.iconColor ?? tokenColor(ctx.tokens, "accent", "#1F6FE0");
        const op = rows[row] === "disabled" ? 0.45 : 1;
        const box = (checked: boolean, radio: boolean) => `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${cell}" height="${cell}" viewBox="0 0 ${cell} ${cell}">
  <g opacity="${op}">
    ${radio
        ? `<circle cx="12" cy="13" r="7.5" fill="${fill}" stroke="${border}" stroke-width="1"/>${checked ? `<circle cx="12" cy="13" r="3.5" fill="${mark}"/>` : ""}`
        : `<rect x="4" y="5" width="15" height="15" rx="3" fill="${fill}" stroke="${border}" stroke-width="1"/>${checked ? `<path d="M7 13 L10 16 L16 8" fill="none" stroke="${mark}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>` : ""}`}
  </g>
</svg>`;
        cells.push({ col: 0, row, svg: box(false, false) });
        cells.push({ col: 1, row, svg: box(true, false) });
        cells.push({ col: 2, row, svg: box(false, true) });
        cells.push({ col: 3, row, svg: box(true, true) });
    }
    const sheet = await compositeSheet(4, 4, cell, cell, cells);
    return [await savePngPair(ctx.outDir, "checks", sheet.hi, sheet.width, sheet.height)];
}

async function emitEdit(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const w = 40;
    const ch = 26;
    const rows = ["disabled", "normal", "focused"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    const muted = tokenColor(ctx.tokens, "border-subtle", "#C9D2E0");
    const focus = tokenColor(ctx.tokens, "bg-focus", tokenColor(ctx.tokens, "accent", "#4A9DFF"));
    const disabledFill = tokenColor(ctx.tokens, "bg-disabled", "#F5F7FA");
    const solid = tokenColor(ctx.tokens, "bg-solid", "#FFFFFF");
    for (let row = 0; row < 3; row++) {
        const vis = stateOf(comp, rows[row], ctx.issues, `components.${name}`);
        vis.radius = radiusOf(comp, vis, 4);
        if (!vis.border) {
            vis.border = { width: 2, color: row === 2 ? focus : muted };
        }
        vis.fill = vis.fill ?? (row === 0 ? disabledFill : solid);
        cells.push({ col: 0, row, svg: visualToSvg(vis, w, ch) });
    }
    const sheet = await compositeSheet(1, 3, w, ch, cells);
    return [await savePngPair(ctx.outDir, "edit", sheet.hi, sheet.width, sheet.height)];
}

async function emitCombo(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    // 与 StyleBase NormalComboBox 一致：60×26，ExtendPos="10,40"。中间 10~40 会平铺，
    // 箭头必须只落在右侧 20px，否则拉宽后会出现一排 chevron。
    const w = 60;
    const ch = 26;
    const arrowW = 20;
    const rows = ["disabled", "normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let row = 0; row < 4; row++) {
        const vis = stateOf(comp, rows[row], ctx.issues, `components.${name}`);
        vis.radius = radiusOf(comp, vis, 6);
        vis.iconColor = vis.iconColor ?? vis.textColor ?? tokenColor(ctx.tokens, "fg", "#FFFFFF");
        const svg = visualToSvg(vis, w, ch, {
            icon: "chevron-down",
            iconOffset: [w / 2 - arrowW / 2, 0],
            iconInset: 0.28,
        });
        cells.push({ col: 0, row, svg });
    }
    const sheet = await compositeSheet(1, 4, w, ch, cells);
    return [await savePngPair(ctx.outDir, fileBase(comp, "comboBox", name), sheet.hi, sheet.width, sheet.height)];
}

async function emitMenu(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const frameVis = asVisual((comp.frame as Record<string, unknown>) ?? stateOf(comp, "normal", ctx.issues, `components.${name}`));
    frameVis.radius = frameVis.radius ?? 4;
    const frameSvg = visualToSvg(frameVis, 50, 20);
    const out = [await renderSvgToPair(ctx.outDir, "menu-frame", frameSvg, 50, 20)];
    const mark = tokenColor(ctx.tokens, "accent", "#1F6FE0");
    const dim = tokenColor(ctx.tokens, "fg-subtle", "#8A93A3");
    const check = visualToSvg({ iconColor: mark }, 18, 18, { icon: "check" });
    out.push(await renderSvgToPair(ctx.outDir, "menu-check", check, 18, 18));
    for (const [file, icon] of [["menu-expand-up", "chevron-up"], ["menu-expand-down", "chevron-down"], ["menu-expand-right", "chevron-right"]] as const) {
        out.push(await renderSvgToPair(ctx.outDir, file, visualToSvg({ iconColor: dim }, 18, 18, { icon }), 18, 18));
    }
    return out;
}

async function emitAlbumArt(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    // 默认封面 albumart.png 由皮肤手绘/外置，Compiler 只出 mask。
    const s = sizeOf(comp, 220, 220);
    const inset = num(comp.maskInset, 4);
    const shape = (comp.maskShape as "roundedRect" | "circle" | undefined) ?? "roundedRect";
    const maskW = s.width - inset * 2;
    const maskH = s.height - inset * 2;
    const r = radiusOf(comp, asVisual(comp), shape === "circle" ? Math.min(maskW, maskH) / 2 : 12);
    const mask = maskShapeSvg(maskW, maskH, shape, Math.max(0, r - 2), 0);
    return [await renderSvgToPair(ctx.outDir, fileBase(comp, "albumArt", name), mask, maskW, maskH)];
}

async function emitScrollbar(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const bar = 15;
    const push = 15;
    const thumb = 30;
    const track = 20;
    const states = 4;
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const fill = (vis.fill as string | undefined) ?? tokenColor(ctx.tokens, "fg-subtle", "#7A7A8C");
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    const total = push + thumb + track + push;
    const horz = isHorzScrollbar(name, comp);
    for (let i = 0; i < states; i++) {
        const op = [0.78, 0.9, 0.86, 0.78][i];
        const svg = horz
            ? `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${total}" height="${bar}" viewBox="0 0 ${total} ${bar}">
  <rect x="${push + 1}" y="3" width="${thumb - 2}" height="9" rx="4" fill="${fill}" opacity="${op}"/>
</svg>`
            : `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${bar}" height="${total}" viewBox="0 0 ${bar} ${total}">
  <rect x="3" y="${push + 1}" width="9" height="${thumb - 2}" rx="4" fill="${fill}" opacity="${op}"/>
</svg>`;
        if (horz) {
            cells.push({ col: 0, row: i, svg });
        } else {
            cells.push({ col: i, row: 0, svg });
        }
    }
    const sheet = horz
        ? await compositeSheet(1, states, total, bar, cells)
        : await compositeSheet(states, 1, bar, total, cells);
    return [await savePngPair(ctx.outDir, fileBase(comp, "scrollbar", name), sheet.hi, sheet.width, sheet.height)];
}

/** StyleBase NormalTabButton / skin-creator button-group：
 *  每行是一整条圆角胶囊（60×24），中间 2px 竖线切开。
 *  引擎按 ButtunBorderWidth=13 + face=3 切：左端圆角帽、可拉伸面、内侧直边 | 分隔 | 对称右半。
 *  行 0–2 = 未选 normal/hover/pressed（整行同色）；行 3–5 = 选中三态。左右半不是两种状态。 */
async function emitTabButton(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const s = sizeOf(comp, 60, 24);
    const w = s.width;
    const h = s.height;
    const vis0 = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const radius = radiusOf(comp, vis0, Math.min(10, Math.floor(h / 2)));
    const sepW = Math.max(1, num(comp.separatorWidth, 2));
    const path = `components.${name}`;

    const fillOf = (vis: Visual, fallback: string): string =>
        typeof vis.fill === "string" ? vis.fill : fallback;
    const strokeOf = (vis: Visual, fallback: string): string =>
        vis.border?.color ?? fallback;
    const strokeW = (vis: Visual): number => Math.max(1, num(vis.border?.width, 2));

    const rowSvg = (vis: Visual): string => {
        const fill = fillOf(vis, "#FFFFFF");
        const stroke = strokeOf(vis, tokenColor(ctx.tokens, "border-subtle", "#C9D2E0"));
        const sw = strokeW(vis);
        const inset = sw / 2;
        const rr = Math.max(0, Math.min(radius, (h - sw) / 2));
        const cx = w / 2;
        return `<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">
  <rect x="${inset}" y="${inset}" width="${w - sw}" height="${h - sw}" rx="${rr}" ry="${rr}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}"/>
  <line x1="${cx}" y1="${sw}" x2="${cx}" y2="${h - sw}" stroke="${stroke}" stroke-width="${sepW}" stroke-linecap="butt"/>
</svg>`;
    };

    const states = ["normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let i = 0; i < 3; i++) {
        cells.push({ col: 0, row: i, svg: rowSvg(stateOf(comp, states[i], ctx.issues, path)) });
    }
    const checkedRows = ["checked", "selected", "focused"] as const;
    for (let i = 0; i < 3; i++) {
        const st = checkedRows[i];
        const vis = (comp.states as Record<string, unknown> | undefined)?.[st]
            ? stateOf(comp, st, ctx.issues, path)
            : stateOf(comp, "checked", ctx.issues, path);
        cells.push({ col: 0, row: i + 3, svg: rowSvg(vis) });
    }
    const sheet = await compositeSheet(1, 6, w, h, cells);
    return [await savePngPair(ctx.outDir, fileBase(comp, "tabButton", name), sheet.hi, sheet.width, sheet.height)];
}

/** Tab 选中指示条：top=图标上方(底部工具栏)，bottom=图标下方(顶部 Tab)。 */
function toolbarIndicatorLayer(
    position: "top" | "bottom",
    cellH: number,
    color: string,
    height: number,
    marginX: number,
    edgePad: number,
): Record<string, unknown> {
    const h = Math.max(1, height);
    const edge = Math.max(0, edgePad);
    const inset = position === "top"
        ? [edge, marginX, cellH - edge - h, marginX]
        : [cellH - edge - h, marginX, edge, marginX];
    return { type: "rect", fill: color, inset };
}

/**
 * 有指示条时，按 indicatorGap 把图标贴到指示条内侧(各态一致，避免选中跳动)。
 * 目标：指示条内沿 ↔ 图标字形外沿 = indicatorGap。
 */
function toolbarIconOffsetY(opts: {
    position: "top" | "bottom" | undefined;
    cellW: number;
    cellH: number;
    indicatorHeight: number;
    indicatorPadding: number;
    indicatorGap: number;
    iconInset: number;
}): number {
    const { position, cellW, cellH, indicatorHeight, indicatorPadding, indicatorGap, iconInset } = opts;
    if (!position) {
        return 0;
    }
    const side = Math.min(cellW, cellH);
    const pad = side * iconInset;
    const barH = Math.max(1, indicatorHeight);
    const edge = Math.max(0, indicatorPadding);
    // 允许负值：图标 path 自身常有内空，可再往指示条方向贴紧
    const gap = indicatorGap;
    const naturalOriginY = (cellH - side) / 2;
    if (position === "top") {
        // naturalOriginY + offset + pad = edge + barH + gap
        return edge + barH + gap - pad - naturalOriginY;
    }
    // naturalOriginY + offset + side - pad = cellH - edge - barH - gap
    return cellH - edge - barH - gap - (side - pad) - naturalOriginY;
}

/** LyricsEditor.xml：lyr-tb.png 单行 40 格 × 22px，Left= 列号。空白列保持透明。 */
const DEFAULT_LYR_TB_SLOTS: Array<{ index: number; icon: string; span?: number; opacity?: number }> = [
    { index: 0, icon: "file-new" },
    { index: 1, icon: "file-open" },
    { index: 2, icon: "file-save" },
    { index: 3, icon: "file-save-as" },
    { index: 4, icon: "play" },
    { index: 5, icon: "pause" },
    { index: 6, icon: "stop" },
    { index: 9, icon: "prev" },
    { index: 10, icon: "next" },
    { index: 11, icon: "jump-line" },
    { index: 12, icon: "insert-tag", span: 2 },
    { index: 14, icon: "insert-tag-sm" },
    { index: 20, icon: "file-open" },
    { index: 21, icon: "lyric-prev" },
    { index: 22, icon: "lyric-next" },
    { index: 23, icon: "toolbar-toggle" },
    { index: 24, icon: "help" },
    { index: 26, icon: "cut" },
    { index: 27, icon: "copy" },
    { index: 28, icon: "paste" },
    { index: 29, icon: "undo" },
    { index: 30, icon: "redo" },
    { index: 31, icon: "search" },
    { index: 32, icon: "replace" },
    { index: 35, icon: "cut", opacity: 0.35 },
    { index: 36, icon: "copy", opacity: 0.35 },
    { index: 37, icon: "undo", opacity: 0.35 },
    { index: 38, icon: "redo", opacity: 0.35 },
];

async function emitIconStrip(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const cell = (comp.cell as { width?: number; height?: number } | undefined)
        ?? (comp.cellSize as { width?: number; height?: number } | undefined)
        ?? {};
    const cw = num(cell.width, 22);
    const ch = num(cell.height, 26);
    const cols = num(comp.columns, 40);
    const vis = stateOf(comp, "normal", ctx.issues, `components.${name}`);
    const color = vis.iconColor ?? vis.textColor ?? tokenColor(ctx.tokens, "fg-muted", "#C9C9E0");
    const iconInset = typeof comp.iconInset === "number" ? comp.iconInset : 0.1;
    const rawSlots = (comp.slots as Array<Record<string, unknown>> | undefined);
    const slots = rawSlots?.length
        ? rawSlots.map((s) => ({
            index: num(s.index, 0),
            icon: String(s.icon ?? ""),
            span: typeof s.span === "number" ? s.span : 1,
            opacity: typeof s.opacity === "number" ? s.opacity : 1,
        }))
        : DEFAULT_LYR_TB_SLOTS;

    const overlays: Array<{ input: Buffer; left: number; top: number }> = [];
    for (const slot of slots) {
        if (!slot.icon || slot.index < 0 || slot.index >= cols) {
            continue;
        }
        const span = Math.max(1, slot.span ?? 1);
        const w = cw * span;
        let svg: string;
        if (slot.icon === "insert-tag") {
            svg = insertTagStripSvg(w, ch, color);
        } else if (!resolveIconName(slot.icon)) {
            ctx.warnings.push(`components.${name}: 未知图标 ${slot.icon}`);
            continue;
        } else {
            svg = visualToSvg({ iconColor: color, opacity: slot.opacity ?? 1, iconInset }, w, ch, {
                icon: slot.icon,
                iconInset,
            });
        }
        const hi = await rasterSvg(svg, w, ch);
        overlays.push({ input: hi, left: slot.index * cw * SS, top: 0 });
    }

    const sheet = await sharp({
        create: {
            width: cols * cw * SS,
            height: ch * SS,
            channels: 4,
            background: { r: 0, g: 0, b: 0, alpha: 0 },
        },
    }).composite(overlays).png().toBuffer();

    return [await savePngPair(ctx.outDir, fileBase(comp, "iconStrip", name), sheet, cols * cw, ch)];
}

async function emitToolbar(name: string, comp: Record<string, unknown>, ctx: EmitCtx): Promise<SavedPng[]> {
    const items = (comp.items as Array<Record<string, unknown>> | undefined) ?? [
        { icon: "list" },
        { icon: "lyric" },
    ];
    const cell = (comp.cellSize as { width?: number; height?: number } | undefined) ?? {};
    const cw = num(cell.width, 44);
    const ch = num(cell.height, 32);
    const cols = items.length * 2;
    const indicatorRaw = comp.indicator;
    const indicatorPos = indicatorRaw === "top" || indicatorRaw === "bottom"
        ? indicatorRaw
        : undefined;
    const indicatorColor = typeof comp.indicatorColor === "string"
        ? comp.indicatorColor
        : tokenColor(ctx.tokens, "accent", "#4A9DFF");
    const indicatorHeight = num(comp.indicatorHeight, 2);
    const indicatorMargin = num(comp.indicatorMargin, 8);
    const indicatorPadding = num(comp.indicatorPadding, 2);
    const indicatorGap = num(comp.indicatorGap, 1);
    const iconInset = typeof comp.iconInset === "number" ? comp.iconInset : 0.18;
    const iconOffsetY = toolbarIconOffsetY({
        position: indicatorPos,
        cellW: cw,
        cellH: ch,
        indicatorHeight,
        indicatorPadding,
        indicatorGap,
        iconInset,
    });
    const states = ["normal", "hover", "pressed"] as const;
    const cells: Array<{ col: number; row: number; svg: string }> = [];
    for (let i = 0; i < items.length; i++) {
        const icon = String(items[i].icon ?? "list");
        for (const checked of [false, true]) {
            const col = i + (checked ? items.length : 0);
            for (let row = 0; row < 3; row++) {
                const vis = stateOf(comp, checked ? "checked" : states[row], ctx.issues, `components.${name}`);
                vis.radius = radiusOf(comp, vis, 8);
                if (typeof iconInset === "number") {
                    vis.iconInset = iconInset;
                }
                if (checked && indicatorPos) {
                    const bar = toolbarIndicatorLayer(
                        indicatorPos, ch, indicatorColor, indicatorHeight, indicatorMargin, indicatorPadding);
                    vis.layers = [bar, ...(vis.layers ?? [])];
                }
                cells.push({
                    col, row,
                    svg: visualToSvg(vis, cw, ch, {
                        icon,
                        iconInset,
                        iconOffset: iconOffsetY ? [0, iconOffsetY] : undefined,
                    }),
                });
            }
        }
    }
    const base = fileBase(comp, "toolbar", name);
    const sheet = await compositeSheet(cols, 3, cw, ch, cells);
    return [await savePngPair(ctx.outDir, base, sheet.hi, sheet.width, sheet.height)];
}

function fontName(tokens: Record<string, unknown>): string {
    const body = (tokens.typography as Record<string, { font?: string }> | undefined)?.body;
    if (!body?.font || body.font === "system") {
        return "Verdana";
    }
    return body.font;
}

function fontSize(tokens: Record<string, unknown>, key: string, d: number): number {
    const t = (tokens.typography as Record<string, { size?: number }> | undefined)?.[key];
    return t?.size ?? d;
}

function buildStylesXml(p: {
    skinFolder: string;
    tokens: Record<string, unknown>;
    roundWidth: number;
    thickWidth: number;
    captionCell: number;
    extendBg: [number, number];
    extendBgVert: [number, number];
    extendDialog: [number, number];
    extendDialogVert: [number, number];
    extendPanel: [number, number];
    sliderEndWidth: number;
    playlistName: string;
    playlist?: Record<string, unknown>;
    hasCaptionBar: boolean;
    hasBg: boolean;
    hasDialogBg: boolean;
    hasFrame: boolean;
}): string {
    const text = toXmlColor(tokenColor(p.tokens, "fg", "#2B2F3A"), "#2B2F3A");
    const textSec = toXmlColor(tokenColor(p.tokens, "fg-muted", "#8A93A3"), "#8A93A3");
    const textDis = toXmlColor(tokenColor(p.tokens, "fg-disabled", tokenColor(p.tokens, "fg-subtle", "#808080")), "#808080");
    const accent = toXmlColor(tokenColor(p.tokens, "accent", "#5AA9FF"), "#5AA9FF");
    const teal = toXmlColor(tokenColor(p.tokens, "teal", accent), accent);
    const surface = opaqueXmlColor(tokenColor(p.tokens, "bg-surface", "#E8EAF5"), "#E8EAF5");
    const font = fontName(p.tokens);
    const captionBg = p.hasCaptionBar
        ? `<Property Name="BgImage" Image="caption-bg.png" HorzExtendPos="${p.extendPanel[0]},${p.extendPanel[1]}" BlendPixMode="alpha_blend"/>`
        : "";
    const windowImageDialog = p.hasDialogBg
        ? `<Property Name="WindowImage" Image="bg-dialog.png" HorzExtendPos="${p.extendDialog[0]},${p.extendDialog[1]}" VertExtendPos="${p.extendDialogVert[0]},${p.extendDialogVert[1]}" />`
        : (p.hasBg
            ? `<Property Name="WindowImage" Image="bg.png" HorzExtendPos="${p.extendBg[0]},${p.extendBg[1]}" VertExtendPos="${p.extendBgVert[0]},${p.extendBgVert[1]}" />`
            : "");
    const frame = (!p.hasBg && p.hasFrame)
        ? `<Frame Name="Frame" Rect="0,0,w,h" Image="frame.png" ImageRect="0,0,64,64"
            RoundWidthTop="${p.roundWidth}" RoundWidthBottom="${p.roundWidth}" ThickWidth="${p.thickWidth}" BlendPixMode="copy"/>`
        : "";
    const pl = p.playlist ?? {};
    const stripes = Array.isArray(pl.stripeColors) ? (pl.stripeColors as string[]).join(",") : "#FFFFFF,#F3F6FB";
    const selBg = toXmlColor(pl.selBgColor, accent);
    const nowBg = toXmlColor(pl.nowPlayingBgColor, surface);
    const nowFg = toXmlColor(pl.nowPlayingTextColor, accent);
    const plFg = toXmlColor(pl.textColor, textSec);
    const plSelFg = toXmlColor(pl.selTextColor, "#FFFFFF");
    const lineH = num(pl.lineHeight, 30);
    const custom = Array.isArray(pl.customizedColors) ? ` CustomizedColors="${(pl.customizedColors as string[]).join(",")}"` : "";

    return `<?xml version="1.0" encoding="UTF-8"?>
<styles>
  <include Name="StyleBase.xml"/>
  <style>
    <Window ToolTipBgColor="${surface}" ToolTipTextColor="${text}" ToolTipFontName="${font}" ToolTipFontHeight="13"
        FontName="${font}" FontHeight="${fontSize(p.tokens, "body", 13)}" TextColor="${text}" BgColor="${surface}"
        EnableTranslucency="true" TranslucencyAlpha="255"/>

    <WindowFrame Extends="Window"
        MinWidth="100" MinHeight="100" Width="330" Height="400" IsDialog="true"
        BgColor="${surface}">
        ${windowImageDialog}
        ${frame}
        <DialogCaption Rect="2,2,w-4,31"/>
        <ClientArea Rect="10,33,w-20,h-33-10" ClipChildren="true"/>
    </WindowFrame>

    <DialogCaption Extends="Container" Name="Caption bar" TranslucencyWithSkin="TRUE">
        ${captionBg}
        <Text ID="ID_CAPTION" Rect="24,6,w-80,20" CaptionText="true" FontName="${font}" FontHeight="15" FontBold="false" TextColor="${text}" LeftMargin="0" AlignText="AT_LEFT | AT_VCENTER"/>
        <Toolbar ID="CID_TB_SYSBT" Rect="w-8-16,9,16,16" TranslucencyWithSkin="TRUE"
            Image="caption-btn.png" enablehover="TRUE" units_x="${p.captionCell}"
            blank_x="-1" blank_cx="${p.captionCell}" seperator_x="0" seperator_cx="10"
            MarginX="0" MarginY="0" ButtonSpacesCX="5" FullStatusImage="TRUE">
          <button ID="ID_CLOSE" Left="4" />
        </Toolbar>
    </DialogCaption>
    <DialogCaption.mac Extends="Container" Name="Caption bar" TranslucencyWithSkin="TRUE">
        ${captionBg}
        <Toolbar ID="CID_TB_SYSBT" Rect="12,8,16,16" TranslucencyWithSkin="TRUE"
            Image="caption-btn-mac.png" enablehover="TRUE" units_x="16"
            blank_x="-1" blank_cx="16" seperator_x="0" seperator_cx="10"
            MarginX="0" MarginY="0" ButtonSpacesCX="8" FullStatusImage="TRUE">
          <button ID="ID_CLOSE" Left="4" />
        </Toolbar>
        <Text ID="ID_CAPTION" Rect="40,6,w-80,20" CaptionText="true" FontName="${font}" FontHeight="15" FontBold="false" TextColor="${text}" LeftMargin="0" AlignText="AT_CENTER | AT_VCENTER"/>
    </DialogCaption.mac>

    <MenuWindowFrame Extends="Window"
        MinWidth="100" MinHeight="100" Width="330" Height="400"
        BgColor="${surface}">
        ${windowImageDialog}
        ${frame}
        <MenuCaption Rect="2,2,w-4,31"/>
        <ClientArea Rect="6,33,w-12,h-33-6" ClipChildren="true"/>
    </MenuWindowFrame>

    <MenuCaption Extends="Container" Name="Caption bar" TranslucencyWithSkin="TRUE">
        ${captionBg}
        <MenuBar Rect="13,6,w/2-20,20" Name="Menu"
            TranslucencyWithSkin="TRUE" FontName="${font}"
            FontHeight="13" FontBold="false" TextColor="${textSec}"
            TextColorPressed="${text}"
            BgColorPressed="${accent}" BgPressedAlpha="200"/>
        <Text ID="ID_CAPTION" Rect="w/2,6,w/2-10-60-10,20" CaptionText="true" FontName="${font}" FontHeight="15" FontBold="false" TextColor="${text}" LeftMargin="0" AlignText="AT_LEFT | AT_VCENTER"/>
        <Toolbar ID="CID_TB_SYSBT" Rect="w-8-60,9,60,16" TranslucencyWithSkin="TRUE"
            Image="caption-btn.png" enablehover="TRUE" units_x="${p.captionCell}"
            blank_x="-1" blank_cx="${p.captionCell}" seperator_x="0" seperator_cx="10"
            MarginX="0" MarginY="0" ButtonSpacesCX="5" FullStatusImage="TRUE">
            <button ID="ID_MINIMIZE" Left="1" />
            <button ID="ID_MAXIMIZE" Left="2" CanCheck="TRUE" checked_left="3"/>
            <button ID="ID_CLOSE" Left="4" />
        </Toolbar>
    </MenuCaption>
    <MenuCaption.mac Extends="Container" Name="Caption bar" TranslucencyWithSkin="TRUE">
        ${captionBg}
        <Toolbar ID="CID_TB_SYSBT" Rect="12,8,64,16" TranslucencyWithSkin="TRUE"
            Image="caption-btn-mac.png" enablehover="TRUE" units_x="16"
            blank_x="-1" blank_cx="16" seperator_x="0" seperator_cx="10"
            MarginX="0" MarginY="0" ButtonSpacesCX="8" FullStatusImage="TRUE">
            <button ID="ID_CLOSE" Left="4" />
            <button ID="ID_MINIMIZE" Left="1" />
            <button ID="ID_MAXIMIZE" Left="2" CanCheck="TRUE" checked_left="3"/>
        </Toolbar>
        <Text ID="ID_CAPTION" Rect="80,6,w-160,20" CaptionText="true" FontName="${font}" FontHeight="15" FontBold="false" TextColor="${text}" LeftMargin="0" AlignText="AT_CENTER | AT_VCENTER"/>
    </MenuCaption.mac>

    <NormalText Extends="Text" Height="22" FontName="${font}" FontHeight="13" TextColor="${text}" AlignText="AT_VCENTER"/>
    <NormalTextRight Extends="Text" Height="22" FontName="${font}" FontHeight="13" TextColor="${text}" AlignText="AT_VCENTER|AT_RIGHT"/>
    <TitleText Extends="Text" Height="25" FontName="${font}" FontHeight="20" TextColor="${text}" AlignText="AT_CENTER"/>
    <NormalLink Extends="TxtLink" Height="22" FontName="${font}" FontHeight="13" TextColor="${accent}" AlignText="AT_VCENTER"/>
    <InfoText Name="InfoText" Rect="0,h-26,w,26" FontName="${font}" FontHeight="16" FontItalic="1" TextColor="${teal}"
        LeftMargin="10" TranslucencyWithSkin="TRUE"/>
    <NormalComboBox Extends="ComboBox" FontName="${font}" FontHeight="13" TextColor="${text}" DisabledTextColor="${textDis}" TabFocus="true"
        Height="22" Padding="5,4" AlignText="AT_LEFT" TextLeftMargin="14" TextRightMargin="30"
        Image="combo-box.png" ImageSize="60,26" ExtendPos="10,40"
        ImagePos="0,26" ImageSelPos="0,78" ImageFocusPos="0,52" ImageDisabledPos="0,0"/>
    <NormalCheckBox Extends="TextButton" FontName="${font}" FontHeight="13" TextColor="${text}" DisabledTextColor="${textDis}"
        Height="24" Padding="3,2" TextLeftMargin="26" AlignText="AT_LEFT" CheckBox="true" TabFocus="true"
        Image="checks.png" ImageSize="26,26"
        ImagePos="0,26" ImageSelPos="0,52" ImageFocusPos="0,78" ImageDisabledPos="0,0"
        S1_Image="checks.png" S1_ImageSize="26,26"
        S1_ImagePos="26,26" S1_ImageSelPos="26,52" S1_ImageFocusPos="26,78" S1_ImageDisabledPos="26,0"/>
    <NormalRadioBt Extends="TextButton" FontName="${font}" FontHeight="13" TextColor="${text}" DisabledTextColor="${textDis}"
        Height="24" Padding="3,2" TextLeftMargin="26" AlignText="AT_LEFT"
        Radio="true" TabFocus="true"
        Image="checks.png" ImageSize="26,26"
        ImagePos="52,26" ImageSelPos="52,52" ImageFocusPos="52,78" ImageDisabledPos="52,0"
        S1_Image="checks.png" S1_ImageSize="26,26"
        S1_ImagePos="78,26" S1_ImageSelPos="78,52" S1_ImageFocusPos="78,78" S1_ImageDisabledPos="78,0"/>
    <NormalFrame Extends="Frame" Image="frame-ctrl.png" RoundWidth="6" ThickWidth="2"
        XBorder="0" YBorder="10" FontName="${font}" FontHeight="13" TextColor="${text}"/>

    <${p.playlistName} Extends="NormalPlaylist" StripeColor="${stripes}" SelBgColor="${selBg}"
        NowPlayingBgColor="${nowBg}" NowPlayingTextColor="${nowFg}"${custom}
        TextColor="${plFg}" SelTextColor="${plSelFg}" FontName="${font}" FontHeight="14" LineHeight="${lineH}"/>

    <Slider Extends="SeekCtrl" Height="15" EndWidth="${p.sliderEndWidth}" ImageThumb="thumb.png" ImageTrack="progress.png"/>
  </style>
</styles>
`;
}

function buildThemeXml(lyrics: Record<string, unknown>, tokens: Record<string, unknown>): string {
    const fg = toXmlColor(lyrics.highlightColor, tokenColor(tokens, "accent", "#5AA9FF"));
    const low = toXmlColor(lyrics.textColor, tokenColor(tokens, "fg-muted", "#C9C9E0"));
    const bg = toXmlColor(lyrics.bgColor, "#00000000");
    const fbg = toXmlColor(lyrics.floatingBgColor, bg);
    return `<?xml version="1.0" encoding="utf-8"?>
<Theme>
  <LyrDispaly>
    <BgColor>${bg}</BgColor>
    <FgColor>${fg}</FgColor>
    <FgLowColor>${low}</FgLowColor>
    <OutlineLyrText>0</OutlineLyrText>
    <HilightOBM>1</HilightOBM>
    <LowlightOBM>1</LowlightOBM>
    <ThemeFont>, 16, normal, , , </ThemeFont>
    <LyrDrawOpt>fadein</LyrDrawOpt>
    <LyrDisplayStyle>LyricShowMultiRow</LyrDisplayStyle>
  </LyrDispaly>
  <FloatingLyr>
    <BgColor>${fbg}</BgColor>
    <FgColor>${fg}</FgColor>
    <FgLowColor>${low}</FgLowColor>
    <OutlineLyrText>1</OutlineLyrText>
    <HilightOBM>1</HilightOBM>
    <LowlightOBM>1</LowlightOBM>
    <ThemeFont>, 16, bold, , , </ThemeFont>
    <LyrDrawOpt>fadeout</LyrDrawOpt>
    <LyrDisplayStyle>LyricShowMultiRow</LyrDisplayStyle>
  </FloatingLyr>
</Theme>
`;
}

function buildMainXml(
    extraResourceFolder: string | undefined,
    layout: { window: { width: number; height: number; minWidth?: number; minHeight?: number }; children: Array<Record<string, unknown>> },
    playlistName: string,
    extendBg: [number, number],
    extendBgVert: [number, number],
    hasBg: boolean,
): string {
    const extra = extraResourceFolder
        ? ` ExtraResouceFolder="${xmlEsc(extraResourceFolder)}"`
        : "";
    const w = layout.window;
    const children = layout.children.map((c) => layoutNode(c, playlistName)).join("\n");
    const windowImage = hasBg
        ? `        <Property Name="WindowImage" Image="bg.png" HorzExtendPos="${extendBg[0]},${extendBg[1]}" VertExtendPos="${extendBgVert[0]},${extendBgVert[1]}" />\n`
        : "";
    return `<?xml version="1.0" encoding="UTF-8"?>
<skin defmainwnd="MainWnd" mainwnds="MainWnd"${extra}>
    <include Name="Styles.xml"/>
    <skinwnd Extends="Window" Name="MainWnd"
            MinWidth="${w.minWidth ?? 640}" MinHeight="${w.minHeight ?? 400}" Width="${w.width}" Height="${w.height}" Menu="MainWndMenu"
            CmdHandler="ch_common,ch_playlist"
            ContextMenu="MainContextMenu">
${windowImage}${children}
    </skinwnd>
</skin>
`;
}

function layoutNode(node: Record<string, unknown>, playlistName: string, indent = "        "): string {
    const control = String(node.control ?? "Container");
    const tag = control === "Playlist" ? playlistName : control;
    const attrs: string[] = [`Rect="${xmlEsc(String(node.rect ?? "0,0,w,h"))}"`];
    if (node.id) {
        attrs.push(`ID="${xmlEsc(String(node.id))}"`);
    }
    if (node.name) {
        attrs.push(`Name="${xmlEsc(String(node.name))}"`);
    }
    const kids = Array.isArray(node.children)
        ? (node.children as Record<string, unknown>[]).map((c) => layoutNode(c, playlistName, indent + "    ")).join("\n")
        : "";
    if (kids) {
        return `${indent}<${tag} ${attrs.join(" ")}>\n${kids}\n${indent}</${tag}>`;
    }
    return `${indent}<${tag} ${attrs.join(" ")}/>`;
}
