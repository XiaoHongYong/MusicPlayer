# Skin Compiler

把 `skin.json` 编译成 MusicPlayer 可加载的皮肤目录（PNG 精灵图 + 可选 XML）。

格式：[`docs/skin-compiler.md`](../../../docs/skin-compiler.md)。引擎约定：[`docs/skin-authoring.md`](../../../docs/skin-authoring.md)。**App 不解析 skin.json。**

## 给 AI agent

1. **只改 JSON，不要写 PIL。** 设计源文件：
   - `Skins-Design/skin-for-ai/glass.skin.json`
   - `Skins-Design/skin-for-ai/neon.skin.json`
   - `Skins-Design/skin-for-ai/crystal.skin.json`
2. 改 tokens / components 后编译（已有皮肤默认只更新 PNG，保留手写 XML）：

```bash
cd Skins-Design/skin-for-ai
./compile.sh glass          # 或 neon、glass.skin.json、--all
./compile.sh --list
./compile.sh crystal --xml  # 新皮肤 / 需要覆盖 XML
```

3. 终端会打印每张图的 `Horz=/Vert=` 9-slice 点。若与 XML 里 `HorzExtendPos`/`VertExtendPos` 不一致，改 XML 或改 `chrome`/`radius`/`size` 后重编。
4. `file` 可覆盖输出文件名（如 `"file": "caption-btn-lg"`）。`windowBackground.variant: "dialog"` 输出 `bg-dialog.png`。文件名一律 kebab-case。
5. 新皮肤且需要生成 XML 时去掉 `--png-only`；缺 `layout` 时仍只出 `Styles.xml` + PNG。

## 环境

- Node.js ≥ 18，**pnpm** ≥ 9（不要用 npm / yarn）

```bash
cd Skins-Design/skin-for-ai/compiler
pnpm install
```

## 用法

```bash
pnpm exec tsx src/cli.ts compile ../glass.skin.json --out ../../skins --png-only --force
pnpm example    # 小示例 → examples/out/GlassBlue
```
