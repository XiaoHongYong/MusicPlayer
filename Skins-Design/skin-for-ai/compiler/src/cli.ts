#!/usr/bin/env tsx
import path from "node:path";
import { fileURLToPath } from "node:url";
import { compileSkin } from "./compile.js";

function printHelp(): void {
    console.log(`Skin Compiler — skin.json → PNG + XML

用法:
  pnpm exec tsx src/cli.ts compile <skin.json> [--out <目录>] [--force] [--png-only]

--out       皮肤父目录（默认 Skins-Design/skins），实际写到 <out>/<meta.name>/
--force     忽略缓存，全部重渲
--png-only  只写 PNG，不覆盖 Styles.xml / main.xml / theme.xml（给已有手写皮肤换图）

也可用 pnpm 脚本:
  pnpm example
`);
}

async function main(): Promise<void> {
    const args = process.argv.slice(2);
    if (args.length === 0 || args.includes("-h") || args.includes("--help")) {
        printHelp();
        process.exit(args.length === 0 ? 1 : 0);
    }
    const cmd = args[0];
    if (cmd !== "compile") {
        console.error(`未知命令: ${cmd}`);
        printHelp();
        process.exit(1);
    }
    const input = args[1];
    if (!input) {
        console.error("缺少 skin.json 路径");
        process.exit(1);
    }
    let outDir: string | undefined;
    let force = false;
    let pngOnly = false;
    for (let i = 2; i < args.length; i++) {
        if (args[i] === "--out") {
            outDir = args[++i];
        } else if (args[i] === "--force") {
            force = true;
        } else if (args[i] === "--png-only") {
            pngOnly = true;
        }
    }
    const report = await compileSkin({ inputPath: input, outDir, force, pngOnly });
    console.log(`皮肤: ${report.skin}`);
    console.log(`输出: ${report.outDir}`);
    for (const f of report.files) {
        const dim = f.width ? ` ${f.width}x${f.height}` : "";
        const slice = f.horzExtendPos ? `  Horz=${f.horzExtendPos} Vert=${f.vertExtendPos}` : "";
        console.log(`  ${f.file}${dim}${slice}  ← ${f.component}`);
    }
    for (const w of report.warnings) {
        console.warn(`警告: ${w}`);
    }
    if (!pngOnly) {
        console.log(`report.json: ${path.join(report.outDir, "report.json")}`);
    }
}

const isMain = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isMain || process.argv[1]?.endsWith("cli.ts")) {
    main().catch((err) => {
        console.error(err instanceof Error ? err.message : err);
        process.exit(1);
    });
}
