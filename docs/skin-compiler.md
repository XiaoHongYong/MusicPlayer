# Skin Compiler 设计文档：AI 生成 JSON → 渲染皮肤资源

本文档描述一个 **Skin Compiler** 程序的设计：输入是一份由 AI 生成的
`skin.json`（设计源文件），输出是 MusicPlayer 皮肤引擎可直接加载的完整皮肤
目录（PNG 精灵图 + XML 控件描述片段）。文档前半部分是系统设计，后半部分
（第 6 章起）是**面向 AI code agent 的 skin.json 格式定义**——AI 只需要读
那一部分即可生成合法的 JSON。

> 前置阅读：[skin-authoring.md](skin-authoring.md)（现有皮肤引擎的目录约定、
> 精灵图布局、XML 语法）。Skin Compiler 的所有输出都必须符合该文档的约定。

## 1. 定位：JSON 是设计源文件，不是运行时格式

```
                 AI Agent
                    │
                    ↓
               skin.json          ← 设计源文件（人/AI 可读写）
                    │
             ┌──────┴──────┐
             │ Skin Compiler │
             └──────┬──────┘
                    │
        ┌───────────┼────────────────┐
        ↓           ↓                ↓
   *.png/@2x.png  Styles.xml 片段   main.xml/theme.xml
        │           │                │
        └───────────┼────────────────┘
                    ↓
        Skins-Design/skins/<SkinName>/   ← 引擎直接加载
                    ↓
                MusicPlayer App
```

**App 不解析 skin.json。** skin.json 只服务于 AI / 设计工具 / Compiler；
App 加载的仍是现有格式的皮肤目录（`main.xml` + `Styles.xml` + PNG），
皮肤引擎（`Skin/`，TinyJS + Agg）零改动。

与「运行时动态 Skin」区分开：本方案把不确定性全部留在编译期，
运行时行为与手写皮肤完全一致，可用 skin-authoring.md 第 8 节的方法调试。

当前源文件在 `Skins-Design/skin-for-ai/`：`glass`、`neon` 全量由 compiler
产出控件 PNG；`classic`、`metal`、`fantasy` 同样产出 StyleBase 控件 PNG，
但主窗 `frame.png`（及 Fantasy 的 `frame-mask.png`/`title_line.png`）保持手绘，
JSON 里不要用 `windowFrame` 覆盖它们。编译：`./compile.sh classic`。

## 2. 设计原则

1. **JSON 描述"视觉"，不描述"程序逻辑"**。只允许颜色、尺寸、圆角、边框、
   阴影、渐变、材质、图片、Mask、字体、状态、布局参数。禁止
   `onClick`/`script`/`if` 等字段——交互由内置命令 ID 和 C++/JS 侧负责
   （见 skin-authoring.md 第 5 节）。
2. **控件与视觉资源分离**。定义 `button` 及其 `normal/hover/pressed` 状态，
   由 Compiler 决定渲染成哪几张精灵图，而不是让 AI 直接罗列
   `button-normal.png` 之类的文件名。
3. **Token 优先**。颜色/圆角/尺寸/字体集中在 `tokens` 定义，控件用
   `$tokens.colors.accent` 引用。AI 改主题 = 改 tokens，不动控件。
4. **State 是一等公民**。至少支持
   `normal/hover/pressed/disabled/focused/selected/checked`，
   各控件声明自己用到哪些。
5. **状态支持继承**（`$extends`），hover 只写与 normal 的差异，
   避免 AI 生成大量重复 JSON。
6. **矢量优先、位图兜底**。圆角、边框、渐变、Mask、Icon、阴影全部用
   矢量描述（Compiler 内部转 SVG 渲染）；只有复杂纹理/照片级素材才允许
   引用外部 PNG/WebP。
7. **Compiler 内部走 SVG**：`skin.json → Scene Graph → SVG → Rasterize →
   PNG`。SVG 的 rect/path/gradient/mask/clip/filter 语义与 Skin JSON
   一一对应，且天然规避了 PIL 逐像素绘制半透明图层的 alpha 覆盖坑
   （skin-authoring.md 第 8.10 节）。
8. **导出由声明驱动**。JSON 里不为每张输出图片写路径；Compiler 按
   `component + state + 精灵图布局约定 + @1x/@2x` 自动推导全部输出文件。

## 3. Compiler 技术选型

第一版推荐 **TypeScript / Node.js**：

```
Node.js + TypeScript
├── Zod            ← skin.json schema 校验（给 AI 明确的报错）
├── (内部) Scene Graph → SVG 字符串
└── Sharp (libvips) ← SVG 栅格化、resize、composite、PNG/WebP 输出
```

理由：这个程序的本质是「JSON → 计算 → SVG → 位图 + XML 文本」，
TypeScript 在 JSON/文件/模板/CLI 上开发效率最高；Sharp 底层的 libvips
做栅格化与合成足够成熟。不需要 Rust/C++。

渲染分辨率：所有图按 **4x 超采样** 渲染 SVG 再缩到 1x/2x 输出，保证圆角/细边框质量。

## 4. 输出产物

```
Skins-Design/skins/<SkinName>/
├── main.xml            ← 由 layout 节生成（或 AI 手写，引用生成的样式）
├── Styles.xml          ← 由 components 生成，<include StyleBase.xml> + 覆盖
├── theme.xml           ← 由 lyrics 节生成（可选）
├── main.js             ← 不由 Compiler 生成；需要交互时 AI 手写
├── bg.png / bg@2x.png              ← 主窗口 WindowImage（圆角+描边+标题条/底栏镀铬）
├── bg-dialog.png / bg-dialog@2x.png ← 对话框 WindowImage（仅标题条，无底栏）
├── frame.png / frame@2x.png  ← 可选；仅旧式独立 Frame 控件需要
├── prev-next.png (+ -mask.png) / @2x
├── playpause.png / @2x
├── caption-btn.png / @2x
├── caption-btn-mac.png / @2x   ← mac 红绿灯（与 captionToolbar 一并产出）
├── progress.png, thumb.png / @2x
├── panel.png, edit.png, checks.png, combo-box.png / @2x
├── menu-frame.png, menu-check.png, menu-expand-*.png / @2x
├── albumart-mask.png / @2x   ← 仅蒙版；albumart.png 不由 Compiler 生成
└── ...（见第 7 章组件清单）
```

Compiler 同时输出一份 `report.json`：列出每个生成的文件、来源组件、
尺寸、以及校验警告（如某个状态未定义被回退到 normal）。

## 5. 从组件到精灵图：布局映射表

Compiler 的核心职责是把"组件 + 状态"翻译成引擎约定的精灵图布局。
下表是映射规则（引擎侧约定详见 skin-authoring.md 第 4 节）：

| JSON `type` | 输出文件 | 精灵图布局（与引擎一致） | 生成的 XML |
|---|---|---|---|
| `windowBackground` | `bg.png` 或 `bg-dialog.png` | 非方形 9-slice。圆角**外必须透明**，描边/标题镀铬画在同一张图。`variant` 缺省/`main` → `bg.png`；`dialog` → `bg-dialog.png`。`chrome.caption/bottom` 决定高度与 `VertExtendPos={caption},{H-bottom}`；`HorzExtendPos` = `{R+1},{W-R-1}`。无 chrome 时仍可方形（如 64/R12 → `13,51`） | 主窗口 / `WindowFrame` 的 `<Property Name="WindowImage">`（**固定 copy**，不再写 Frame） |
| `windowFrame` | `frame.png` | **可选**。默认 64×64@1x；仅当不要把边框画进 `bg.png`、仍要独立 `<Frame>` 时使用。有 `windowBackground` 时 Compiler **不**往 XML 里塞 Frame | 无 `WindowImage` 时才生成 `<Frame BlendPixMode="copy">` |
| `captionToolbar` | `caption-btn.png` + `caption-btn-mac.png` | Win/Linux：5 列 × 3 行，格宽默认 16。列 0 留空，**1=最小化 2=最大化 3=还原 4=关闭**。mac 图为同布局的红绿灯（关闭/最小化/最大化顺序由 XML 按钮排列，不改列号）。`FullStatusImage` 时整格含背景 | 样式 `DialogCaption`/`MenuCaption` + `.mac` 后缀；窗口里只写一份实例，见 skin-authoring.md 3.1 |
| `captionBar` | `caption-bg.png` | 9-slice 标题条底（对话框 `WindowFrame` 也引用） | 标题栏容器 `BgImage` |
| `button` | `{name}.png`（kebab-case）+ 可选 `-mask.png` | 默认 **1 列 × 3 行**（normal/hover/pressed）。`icons: ["prev","next"]` 时改为 **N 列 × 3 行** 打进同一张图（如 `prev-next.png`）。mask **必须同布局**（引擎用按钮图精灵坐标采样蒙版） | `<Button Image= ImagePos= ImageFocusPos= ImageSelPos= ImageMask=>` |
| `toggleButton` | `{name}.png`（`playPause` → `playpause.png`） | **2 列 × 3 行**（列 = S0=`iconOff` / S1=`iconOn`，行 = 三状态） | `<Button S0_Image=... S1_Image=...>` |
| `slider` | `progress.png` + `thumb.png` | track：**上半未播放 / 下半已播放**（`SkinSeekCtrl` 对半切，`EndWidth` 端点不拉伸）；thumb：竖排 3 态。多个 slider **共用**这两张图（音量与进度同款，与 StyleBase 一致） | `<Slider ImageTrack= ImageThumb= EndWidth=>` |
| `checkbox` | `checks.png` | **4 列 × 4 行、26px/格**（与 `StyleBase.xml` 的 `NormalCheckBox`/`NormalRadioBt` 一致）：列 = 复选关/开、单选关/开；行 = **disabled / normal / pressed / hover**。一张图同时服务复选和单选 | 覆盖 `NormalCheckBox`/`NormalRadioBt` 即可，不必再出 radio 专图 |
| `edit` | `edit.png` | **3 行 × 26px**（disabled / normal / hover+focus），`RoundWidth=4` `ThickWidth=3` 的框。编辑框**内容底色引擎写死白色**，深色皮肤应用 `searchBar`/`panel` 做容器 | `EditCtrlFrame` 样式 |
| `comboBox` | `combo-box.png` | **4 行 × 26px**（disabled/normal/hover/pressed）+ 右侧箭头区（`ExtendPos="10,40"`） | `NormalComboBox` 样式 |
| `menu` | `menu-frame.png`（50×20）、`menu-check.png`、`menu-expand-{up,down,right}.png`（18×18） | frame 给 `PopupWndFrame` 切顶/底/边；箭头/勾为单图 | `MenuItemsContainer` / `PopupWndFrame` |
| `albumArt` | **仅** `albumart-mask.png`（`albumart.png` 手绘/外置，Compiler 不生成） | mask = 白 RGB + alpha 形状，比默认封面小一圈并居中（引擎把封面缩到 mask 尺寸后相乘；mask 跟的是 `Image` 不是控件 Rect） | `<AlbumArt Image= FrameMask=>` |
| `panel` | `panel.png`（或 `{name}.png`） | 9-slice，ExtendPos 由圆角推导 | 容器 `BgImage` |
| `searchBar` | `search-bg.png` | 9-slice 搜索条底 | 搜索容器 `BgImage` |
| `scrollbar` | `scrollbar-vert.png` / `scrollbar-horz.png` | 竖直：**4 列 × 4 段**（列 = normal/pushdown/focus/spare；行高 15+30+20+15）。水平：同样四段横排、**4 行**状态，`orientation: "horizontal"`。与 StyleBase `VScrollBar`/`HScrollBar` 一致 | 同名覆盖即可 |
| `tabButton` | `button-group.png` | **60×144**，与普通 button 精灵图不同。每行是**一条**圆角胶囊（高 24，圆角约 10）+ 中缝 2px；`ButtunBorderWidth=13` 切左右端帽，中间 3px 作拉伸面。行 0–2 未选三态、行 3–5 选中三态（左右半边是端帽几何，不是未选/选中）。对应 StyleBase `NormalTabButton` | 同名覆盖 |
| `toolbar` | `{name}.png`（视图 Tab 用 `view-tabs.png`） | 列 = 按钮×（未选/选中），行 = 三状态；`Left` 0-based。`indicator: "top"\|"bottom"` 画 focus 横线；`indicatorGap` 控制横线与图标间距（底部 Tab 用 `top`，顶部 Tab 用 `bottom`） | `<Toolbar RadioGroup=... checked_left=>` |
| `iconStrip` | `{file}.png`（歌词编辑器用 `lyr-tb.png`） | **单行**图标条。默认 40 列 × 22×26，列号与 `LyricsEditor.xml` 的 `Left=` 一致；`iconColor` 用皮肤 fg。暗色皮肤覆盖 `assets/lyr-tb.png`（那张是给浅色底的深色图标） | 无需改 XML，同名覆盖即可 |
| `playlist` | （无图） | 纯配色。XML 属性名是 **`StripeColor`**（逗号分隔两色），不是 `StripeColors` | `<XxxPlaylist Extends="NormalPlaylist">` |
| `lyrics`（顶层节，不是 component） | （无图） | JSON `textColor`→`FgLowColor`，`highlightColor`→`FgColor`，`bgColor`→`BgColor` | `theme.xml` 的 **`LyrDispaly`**（引擎历史拼写）/ `FloatingLyr` |

所有 PNG 自动产出 `@2x`（Retina），XML 中一律写 1x 逻辑尺寸。

---

# 以下为面向 AI code agent 的 skin.json 格式定义

> AI 使用指南：你只产出**一个 `skin.json` 文件**。不要写文件路径到
> components 里（路径由 Compiler 按上表推导）；不要写任何脚本/逻辑；
> 所有颜色、圆角、尺寸必须先定义为 token 再引用。JSON 必须符合第 6 章
> 的顶层结构，未知字段会被 Compiler 拒绝并报错。

## 6. 顶层结构

```json
{
  "schema": "skin.v1",
  "meta":      { ... },   // 必需：皮肤名称/版本
  "tokens":    { ... },   // 必需：颜色/尺寸/圆角/字体/阴影
  "materials": { ... },   // 可选：可复用的材质（玻璃、金属…）
  "assets":    { ... },   // 可选：外部图片/SVG 资源注册表
  "templates": { ... },   // 可选：共享 layers 模板（组件用 template 引用）
  "components": { ... },  // 必需：各控件视觉定义
  "layout":    { ... },   // 可选：主窗口布局（省略则 Compiler 只出图和样式）
  "lyrics":    { ... },   // 可选：歌词配色 → theme.xml
  "exports":   { ... }    // 可选：覆盖默认导出参数
}
```

- `schema`：固定 `"skin.v1"`。Compiler 按它选择解析器版本。
- 引用语法：字符串以 `$` 开头表示引用，如 `"$tokens.colors.accent"`、
  `"$materials.glass"`。只允许引用，不允许表达式。

### 6.1 meta

```json
"meta": {
  "name": "Glass Blue",        // 皮肤目录名（英文，即引擎里的皮肤名）
  "version": "1.0.0",
  "author": "AI",
  "description": "Light blue glass desktop skin",
  "extraResourceFolder": "Classic"   // 可选；XML 必须写成 ExtraResouceFolder（引擎历史拼写）
}
```

## 7. tokens

五类，全部可选但强烈建议定义；控件中禁止出现"魔法数字"，应引用 token。

颜色 token 用语义名：基础层是 `bg` / `fg` / `border`。叠加状态必须成对写成
`bg-hover`/`fg-hover`、`bg-pressed`/`fg-pressed`、`bg-focus`/`fg-focus`、
`bg-selected`/`fg-selected`、`bg-disabled`/`fg-disabled`（以及 `bg-danger`/
`fg-danger` 等），**同一状态的 bg 与 fg 要保持对比度**（选中行用 `bg-selected`
填底、`fg-selected` 写字）。再按需要加 `bg-surface`、`fg-muted`、`fg-subtle`、
`border-subtle`。皮肤特有色（如 Neon 的 `teal`）可以额外加。

```json
"tokens": {
  "colors": {
    "bg":            "#DDE9F7E4",
    "bg-muted":      "#F3EDF5E4",
    "bg-surface":    "#FFFFFFBE",
    "bg-solid":      "#FFFFFF",
    "bg-chrome":     "#FAFBFDEB",
    "bg-track":      "#D8DEE8",
    "fg":            "#2B2F3A",
    "fg-muted":      "#5B6372",
    "fg-subtle":     "#8A93A3",
    "fg-on-accent":  "#FFFFFF",
    "border":        "#FFFFFF99",
    "border-subtle": "#C9D2E0",
    "accent":        "#4A9DFF",
    "accent-muted":  "#2F7FE0",
    "bg-hover":      "#F4F8FE",
    "fg-hover":      "#2B2F3A",
    "bg-pressed":    "#E8F2FC",
    "fg-pressed":    "#2B2F3A",
    "bg-focus":      "#4A9DFF",
    "fg-focus":      "#FFFFFF",
    "bg-selected":   "#D6E8FA",
    "fg-selected":   "#1E4A8C",
    "bg-disabled":   "#F5F7FA",
    "fg-disabled":   "#8A93A3",
    "bg-danger":     "#E5484D",
    "fg-danger":     "#FFFFFF",
    "bg-danger-pressed": "#B23338",
    "fg-danger-pressed": "#FFFFFF"
  },
  "dimensions": {
    "buttonHeight": 40,
    "inputHeight":  36,
    "captionHeight": 28,
    "paddingSmall":  8,
    "paddingMedium": 12
  },
  "radius": {
    "small": 6, "medium": 10, "large": 16, "window": 24, "pill": 999
  },
  "typography": {
    "body":   { "font": "system", "size": 13, "weight": 400 },
    "button": { "font": "system", "size": 13, "weight": 500 },
    "title":  { "font": "system", "size": 15, "weight": 600 }
  },
  "shadows": {
    "control": { "offset": [0, 2],  "blur": 8,  "spread": 0, "color": "#00000020" },
    "window":  { "offset": [0, 12], "blur": 40, "spread": 0, "color": "#00000030" }
  }
}
```

颜色格式：`#RRGGBB` 或 `#RRGGBBAA`。尺寸单位 = 1x 逻辑像素。

## 8. materials

可复用的"材质"，供多个组件引用（`"material": "$materials.glass"`）：

```json
"materials": {
  "glass": {
    "fill":   "$tokens.colors.bg-surface",
    "blur":   24,
    "border": { "width": 1, "color": "$tokens.colors.border" },
    "noise":  0.03
  }
}
```

字段：`fill`（颜色或渐变，见 9.3）、`blur`（背景模糊半径，Compiler 用
SVG filter 实现）、`border`、`noise`（0~1 噪点强度）、`highlight`
（顶部高光线，`{ "opacity": 0.15 }`）。

## 9. components

每个键是一个组件。通用结构：

```json
"components": {
  "<name>": {
    "type":     "<组件类型>",        // 见下表
    "size":     { "width": 40, "height": 40 },
    "radius":   "$tokens.radius.medium",
    "template": "<模板名>",          // 可选：引用 9.2 的共享视觉模板
    "states":   { ... },             // 状态表，见 9.1
    ...                              // 组件特有字段
  }
}
```

组件类型（对应第 5 章映射表）：

| type | 说明 | 特有字段 |
|---|---|---|
| `windowBackground` | 窗口 9-slice 底座（圆角外透明） | `variant: "main"\|"dialog"`；`file`；`chrome: { caption, bottom, mid, captionFill, bottomFill, hairlineColor, bottomChrome }` |
| `windowFrame` | 可选的独立边框精灵图 | `roundWidth`, `thickWidth`, `layers` |
| `captionToolbar` | 标题栏按钮条 | `file`（如 `caption-btn-lg`）、`cellSize`。`caption-btn` 会额外出 `caption-btn-mac` 红绿灯 |
| `captionBar` | 标题栏 9-slice 底 | `layers` / `file` |
| `button` | 图标按钮 | `icon`/`icons`，`mask: true\|"hitbox"`（hitbox=居中方块点击区） |
| `toggleButton` | 双态按钮 | `iconOff`, `iconOn` |
| `slider` | 进度/音量 | `track`、`thumb` |
| `checkbox` | 勾选+单选 | 一张 `checks.png` |
| `edit` | 输入框边框 | `layers` |
| `comboBox` | 下拉框 | `layers` |
| `menu` | 菜单框与箭头 | `frame` |
| `albumArt` | 仅蒙版（不出默认封面图） | `maskShape`, `maskInset`, `size` |
| `panel` | 圆角面板底 | `file` 可改输出名（如 lyrCtrlBg → `lyr-ctrl-bg.png`） |
| `searchBar` | 搜索条底 | `size`, `layers` |
| `scrollbar` | 滚动条 | `orientation: "vertical"\|"horizontal"`（默认竖直 → `scrollbar-vert.png`） |
| `tabButton` | 分段 Tab（StyleBase `NormalTabButton`） | 输出 `button-group.png` |
| `toolbar` | 视图 Tab | `items`, `cellSize`；`indicator: "top"\|"bottom"`；可选 `indicatorColor` / `indicatorHeight` / `indicatorMargin` / `indicatorPadding`（贴边距，默认 2）/ `indicatorGap`（横线与图标盒间距，默认 1，可负值贴得更紧）/ `iconInset` |
| `iconStrip` | 单行工具条/图条（`lyr-tb.png`、`lyric-file-type.png`） | `columns`、`cell`、`iconColor`；`slots[].icon` / 可选 `slots[].iconColor`；省略 `slots` 时用歌词编辑器列布局 |
| `icon` | 单张小图 | `icon` 内置名或 `layers`（volume/search/vinyl） |
| `sprite` | 竖排状态条 | `file`, `cell`, `layout: ["normal","hover",…]` |
| `playlist` | 播放列表配色 | 见 9.5 |

### 9.1 states 与继承

```json
"states": {
  "normal": {
    "material": "$materials.glass",
    "textColor": "$tokens.colors.fg"
  },
  "hover": {
    "$extends": "normal",
    "fill": "$tokens.colors.bg-hover",
    "textColor": "$tokens.colors.fg-hover"
  },
  "pressed": {
    "$extends": "hover",
    "scale": 0.98,
    "offset": [0, 1]
  },
  "disabled": {
    "$extends": "normal",
    "opacity": 0.4
  }
}
```

规则：

- `$extends` 指向同组件内另一个状态，做浅合并（引用方字段覆盖被继承方）。
- 未定义的状态由 Compiler 回退到 `normal` 并在 report 中给警告。
- 状态内可用字段：`material`、`fill`、`border`、`shadow`、`textColor`、
  `iconColor`、`opacity`、`scale`、`offset`、`layers`（整层替换）。

### 9.2 视觉模板（layers）

组件外观最终归结为一组**绘制层**，自底向上渲染。可直接写在组件/状态上，
也可放进顶层 `templates` 供多个组件共享（组件用 `"template"` 引用）。

```json
"layers": [
  { "type": "shadow",      "shadow": "$tokens.shadows.control" },
  { "type": "roundedRect", "fill": "$tokens.colors.bg-surface",
    "radius": "$tokens.radius.medium" },
  { "type": "border",      "width": 1, "color": "$tokens.colors.border",
    "radius": "$tokens.radius.medium" },
  { "type": "highlight",   "opacity": 0.15 }
]
```

第一版只支持这些 layer 类型（**不要发明新类型**）：

| type | 字段 |
|---|---|
| `rect` / `roundedRect` | `fill`, `radius`, `inset` |
| `circle` | `fill`, `center`, `r` |
| `path` | `d`（SVG path 字符串）, `fill`, `stroke` |
| `line` | `from`, `to`, `color`, `width` |
| `image` | `asset`（`$assets.xxx` 引用）, `fit: "contain"/"cover"/"stretch"` |
| `text` | `content`, `typography`, `color`, `align` |
| `gradient` | 见 9.3 |
| `shadow` | `shadow`（token 引用或内联） |
| `blur` | `radius` |
| `mask` | `mask`（见 9.4）, `layers`（被裁剪的子层） |
| `clip` | `radius`/`path`, `layers` |
| `border` | `width`, `color`, `radius`, `inset`（圆角描边；也可写在 material 上） |
| `highlight` | `opacity`（顶部内高光线/淡渐变） |

不支持：表达式、条件、循环、脚本、自定义 shader。

### 9.3 fill / gradient

`fill` 取值三选一：

```json
"#RRGGBBAA"                                        // 纯色
"$tokens.colors.accent"                            // token 引用
{ "type": "linear", "angle": 90,                   // 渐变
  "stops": [ [0, "#FFFFFF30"], [1, "#FFFFFF08"] ] }
```

### 9.4 mask

```json
"mask": {
  "type":   "alpha",              // alpha | luma | binary
  "shape":  "roundedRect",        // 或 "circle" / "asset"
  "radius": "$tokens.radius.window",
  "inset":  4,                    // 相对目标区域的内缩（albumArt mask 必需）
  "asset":  "$assets.windowMask"  // type/shape 为 asset 时使用
}
```

### 9.5 playlist / lyrics（纯配色组件）

```json
"components": {
  "playlist": {
    "type": "playlist",
    "stripeColors":       ["$tokens.colors.bg-surface", "$tokens.colors.bg-muted"],
    "selBgColor":         "$tokens.colors.bg-selected",
    "nowPlayingBgColor":  "$tokens.colors.bg-pressed",
    "nowPlayingTextColor":"$tokens.colors.fg-pressed",
    "textColor":          "$tokens.colors.fg-muted",
    "selTextColor":       "$tokens.colors.fg-selected",
    "lineHeight":         30
  }
},
"lyrics": {
  "textColor":      "$tokens.colors.fg-muted",
  "highlightColor": "$tokens.colors.accent",
  "bgColor":        "#00000000"
}
```

Compiler 写出的 XML/INI 键必须跟引擎一致：

- 播放列表：`StripeColor="#1B1B36,#17172E"`（JSON 数组 → 逗号拼接），另有可选 `customizedColors` → `CustomizedColors`
- `theme.xml` 节名是 `LyrDispaly`（不是 Display）；`highlightColor`→`<FgColor>`，`textColor`→`<FgLowColor>`，`bgColor`→`<BgColor>`

## 10. assets（外部资源注册表）

仅当矢量描述无法满足（复杂纹理、AI 生成素材、现成 SVG 图标）时使用：

```json
"assets": {
  "icons.play":   { "type": "svg",   "source": "assets/icons/play.svg" },
  "window.noise": { "type": "image", "source": "assets/noise.png" },
  "window.mask":  { "type": "mask",  "source": "assets/window-mask.svg" }
}
```

组件内用 `"$assets.icons.play"` 引用。`source` 相对 skin.json 所在目录。

## 11. layout（可选：主窗口布局）

省略时 Compiler 只生成图片 + Styles.xml，`main.xml` 由人/AI 手写。
若提供，结构为控件树，直接映射 skin-authoring.md 第 3 节的 Rect 公式：

```json
"layout": {
  "window": { "width": 800, "height": 480, "minWidth": 640, "minHeight": 400 },
  "children": [
    { "control": "Button", "id": "ID_PLAYPAUSE", "component": "playPause",
      "rect": "w/2-24, h-64, 48, 48" },
    { "control": "Slider", "id": "ID_SEEK", "component": "seekSlider",
      "rect": "12, h-12, w-24, 8" }
  ]
}
```

- `id`：内置命令 ID（`ID_PLAYPAUSE`/`ID_SEEK`/…，自动获得行为）或
  `CID_*` 自定义 ID（由手写 main.js 处理，不在 Compiler 范围内）。
- `component`：引用 `components` 中的键，Compiler 据此填图片属性。
- `rect`：四元组公式字符串，语义与引擎完全一致（`w`/`h` 为父容器宽高）。

## 12. exports（可选）

覆盖 Compiler 的默认导出行为；一般不需要，默认值即第 5 章映射表 +
`scale: [1, 2]` + `format: "png"`。

```json
"exports": {
  "scale":  [1, 2],
  "format": "png",
  "button": { "sizes": [[40, 40]] },
  "window": { "size": [800, 480] }
}
```

## 13. 完整示例

```json
{
  "schema": "skin.v1",
  "meta": { "name": "Glass Blue", "version": "1.0.0", "author": "AI" },

  "tokens": {
    "colors": {
      "bg": "#20304ADD", "bg-muted": "#101828EE",
      "bg-surface": "#FFFFFF20", "bg-hover": "#FFFFFF30", "fg-hover": "#FFFFFF",
      "fg": "#FFFFFF", "fg-muted": "#FFFFFF99", "fg-on-accent": "#FFFFFF",
      "border": "#FFFFFF45", "accent": "#5AA9FF",
      "bg-selected": "#3D7ED0", "fg-selected": "#FFFFFF"
    },
    "radius": { "medium": 10, "window": 24 },
    "dimensions": { "buttonHeight": 40, "captionHeight": 28 },
    "typography": { "button": { "font": "system", "size": 13, "weight": 500 } },
    "shadows": {
      "control": { "offset": [0, 2], "blur": 8, "color": "#00000020" }
    }
  },

  "materials": {
    "glass": {
      "fill": "$tokens.colors.bg-surface", "blur": 24,
      "border": { "width": 1, "color": "$tokens.colors.border" }
    }
  },

  "components": {
    "windowBg": {
      "type": "windowBackground",
      "variant": "main",
      "chrome": { "caption": 43, "bottom": 30, "mid": 20 },
      "layers": [
        { "type": "roundedRect", "radius": "$tokens.radius.window",
          "fill": { "type": "linear", "angle": 180,
                    "stops": [[0, "#20304ADD"], [1, "#101828EE"]] } },
        { "type": "border", "width": 2,
          "color": "$tokens.colors.border", "radius": "$tokens.radius.window" }
      ]
    },
    "windowBgDialog": {
      "type": "windowBackground",
      "variant": "dialog",
      "chrome": { "caption": 33, "bottom": 13, "mid": 18 },
      "layers": [
        { "type": "roundedRect", "radius": "$tokens.radius.window", "fill": "#EBEBEB" },
        { "type": "border", "width": 2,
          "color": "$tokens.colors.border", "radius": "$tokens.radius.window" }
      ]
    },

    "playPause": {
      "type": "toggleButton",
      "size": { "width": 48, "height": 48 },
      "iconOff": "triangle-right",
      "iconOn":  "pause-bars",
      "states": {
        "normal":  { "iconColor": "$tokens.colors.fg" },
        "hover":   { "$extends": "normal", "iconColor": "$tokens.colors.accent" },
        "pressed": { "$extends": "hover", "scale": 0.94 }
      }
    },

    "seekSlider": {
      "type": "slider",
      "track": {
        "height": 8, "endWidth": 8, "radius": 4,
        "remaining": { "fill": "$tokens.colors.bg-surface" },
        "played":    { "fill": "$tokens.colors.accent" }
      },
      "thumb": {
        "size": { "width": 15, "height": 15 },
        "states": {
          "normal":  { "fill": "$tokens.colors.fg",
                       "shadow": "$tokens.shadows.control" },
          "hover":   { "$extends": "normal", "fill": "$tokens.colors.accent" },
          "pressed": { "$extends": "hover" }
        }
      }
    },

    "playlist": {
      "type": "playlist",
      "stripeColors": ["#FFFFFF08", "#FFFFFF04"],
      "selBgColor": "$tokens.colors.bg-selected",
      "textColor": "$tokens.colors.fg-muted",
      "selTextColor": "$tokens.colors.fg-selected",
      "lineHeight": 30
    }
  },

  "lyrics": { "textColor": "#FFFFFFCC", "highlightColor": "$tokens.colors.accent" }
}
```

## 14. AI 生成规则速查（给 agent 的硬约束）

1. 顶层键只能是第 6 章列出的那些（含可选 `templates`）；`schema` 必须为 `"skin.v1"`。
2. 任何颜色/尺寸/圆角先定义 token 再用 `$tokens...` 引用；允许的直接
   字面量仅限：渐变 stops、`inset`、`offset`、`roundWidth`/`thickWidth`、
   `size`/`endWidth` 这类局部几何值。
3. 状态键只能是 `normal/hover/pressed/disabled/focused/selected/checked/
   indeterminate`；`$extends` 必须指向同组件内已定义的状态。
4. layer `type` 只能用 9.2 表格里的类型（含 `border`/`highlight`）。
5. 主窗口必备：`windowBackground`、`captionToolbar`、`toggleButton`(播放)、
   `slider`、`playlist`。建议再给对话框一份 `variant:"dialog"`。
   `windowFrame` 不是必备。Classic/Metal/Fantasy 的主窗 `frame.png` 为手绘纹理，
   JSON 里不要用 `windowFrame`/`file: frame` 覆盖它。现有皮肤用 `--png-only` 只换 PNG。
6. 不写任何脚本、事件、表达式；不要输出 `@2x` 相关字段（Compiler 自动）。
7. 需要交互（视图切换 tab 等）时，只在 `layout` 里放 `CID_*` 控件并
   在 `meta.description` 注明需要手写 main.js，不要在 JSON 里写 JS。

## 15. Compiler 实现要点（给实现者）

- **校验**：Zod schema 完整表达第 6~12 章；引用解析分两遍（先收集
  tokens/materials/assets/templates，再解析 components），对未解析的 `$` 引用、
  非法状态名、循环 `$extends` 给出带 JSON 路径的报错。
- **渲染管线**：组件 + 状态 → 解析继承/引用 → Scene Graph → SVG 字符串
  → Sharp 4x 超采样栅格化 → 缩放到 1x/2x → 按第 5 章布局拼装精灵图
  （`sharp.composite` 把各 state 格拼进一张图）。
- **mask**：9.4 的 mask 渲染为独立 RGBA 图（白 RGB + alpha 形状），
  按钮 mask 与按钮图**保持相同精灵布局**（引擎按同一精灵坐标采样）。
- **内置图标名**（`icon` / `iconOff` / `iconOn` / `icons`，无需 assets）：
  `play`/`triangle-right`、`pause`/`pause-bars`、`prev`、`next`、
  `minimize`、`maximize`、`restore`、`close`、`check`、`search`、
  `volume`、`spectrum`、`list`、`lyric`、`media-center`、`chevron-up`/`down`/`right`；
  歌词文件类型：`lyr-file` / `lyr-file-txt` / `lyr-file-lrc` / `lyr-file-unknown`
  及对应 `*-net`（网络来源，右下角云标）。
- **XML 生成**：用模板生成 `Styles.xml`（`<include Name="StyleBase.xml"/>`
  + 各样式覆盖）与可选 `main.xml`；数值一律写 1x 逻辑像素。窗口底座输出
  `<Property Name="WindowImage" .../>`（固定 copy），**默认不生成 Frame**。
  可同时输出 `bg.png`（main）与 `bg-dialog.png`（dialog）。
  引擎历史拼写原样输出：`ExtraResouceFolder`、`LyrDispaly`。
- **增量**：以组件解析后的 hash 为缓存键，未变化的组件不重渲染。
- **自检**：编译后可用 skin-authoring.md 第 8 节的日志告警
  （`Unknown property` 等）做冒烟验证。
- **代码位置**：`Skins-Design/skin-for-ai/compiler/`（Node.js + TypeScript）。
