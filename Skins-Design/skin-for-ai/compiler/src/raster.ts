import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";
import sharp from "sharp";

export const SS = 4;

export interface SavedPng {
    name: string;
    width: number;
    height: number;
    files: string[];
}

export async function rasterSvg(svg: string, outW: number, outH: number): Promise<Buffer> {
    const hiW = Math.max(1, Math.round(outW * SS));
    const hiH = Math.max(1, Math.round(outH * SS));
    const buf = Buffer.from(svg);
    return sharp(buf, { density: 96 * SS })
        .resize(hiW, hiH, { fit: "fill" })
        .png()
        .toBuffer();
}

export async function downscalePair(hi: Buffer, outW: number, outH: number): Promise<{ x1: Buffer; x2: Buffer }> {
    const x2 = await sharp(hi).resize(outW * 2, outH * 2, { kernel: "lanczos3" }).png().toBuffer();
    const x1 = await sharp(hi).resize(outW, outH, { kernel: "lanczos3" }).png().toBuffer();
    return { x1, x2 };
}

export async function savePngPair(outDir: string, name: string, hi: Buffer, w: number, h: number): Promise<SavedPng> {
    const { x1, x2 } = await downscalePair(hi, w, h);
    await mkdir(outDir, { recursive: true });
    const f1 = `${name}.png`;
    const f2 = `${name}@2x.png`;
    await writeFile(path.join(outDir, f1), x1);
    await writeFile(path.join(outDir, f2), x2);
    return { name, width: w, height: h, files: [f1, f2] };
}

export async function renderSvgToPair(outDir: string, name: string, svg: string, w: number, h: number): Promise<SavedPng> {
    const hi = await rasterSvg(svg, w, h);
    return savePngPair(outDir, name, hi, w, h);
}

export async function compositeSheet(
    cols: number,
    rows: number,
    cellW: number,
    cellH: number,
    cells: Array<{ col: number; row: number; svg: string }>,
): Promise<{ hi: Buffer; width: number; height: number }> {
    const w = cols * cellW;
    const h = rows * cellH;
    const overlays: sharp.OverlayOptions[] = [];
    for (const cell of cells) {
        const hi = await rasterSvg(cell.svg, cellW, cellH);
        overlays.push({
            input: hi,
            left: cell.col * cellW * SS,
            top: cell.row * cellH * SS,
        });
    }
    const sheet = await sharp({
        create: {
            width: w * SS,
            height: h * SS,
            channels: 4,
            background: { r: 0, g: 0, b: 0, alpha: 0 },
        },
    })
        .composite(overlays)
        .png()
        .toBuffer();
    return { hi: sheet, width: w, height: h };
}

export function extendPos(size: number, radius: number): [number, number] {
    const a = Math.max(1, Math.min(radius + 1, Math.floor(size / 2) - 1));
    return [a, size - a];
}

export function kebabName(name: string): string {
    return name
        .replace(/([a-z0-9])([A-Z])/g, "$1-$2")
        .replace(/[\s_]+/g, "-")
        .toLowerCase();
}

export async function hashJson(value: unknown): Promise<string> {
    const { createHash } = await import("node:crypto");
    return createHash("sha1").update(JSON.stringify(value)).digest("hex").slice(0, 16);
}
