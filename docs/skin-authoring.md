# 皮肤制作指南

本文档介绍如何为 MusicPlayer 制作皮肤。皮肤引擎位于 `Skin/`（TinyJS + Agg 自绘），
本文以 **Neon** 皮肤（`Skins-Design/skins/Neon/`）为参考实例，它是按本文档制作的完整样板。

## 1. 皮肤目录与发现机制

皮肤 = 皮肤根目录下的一个文件夹，**文件夹名即皮肤名**。必须包含 `main.xml`
（文件名硬编码于 `Skin/SkinFactory.cpp` 的 `_SZ_SKIN_FILE_NAME`）。

皮肤根目录按优先级（`CSkinFactory::getSkinRootDir()`）：

1. 配置文件中 `SkinRootDir`（开发时 `build.sh` 会写入 `build/Debug/MusicPlayer.ini`，
   指向 `Skins-Design/skins`）；
2. App 包内 `<Resources>/skins`（CMake 将 `Skins-Design/skins` 整目录打包）；
3. 开发兜底：`<Resources>/../../Skins-Design/skins`。

**资源查找顺序**（打开任意图片/XML 时，`CSkinFactory::openSkin`）：

1. 当前皮肤目录；
2. `ExtraResouceFolder` 指定的其他皮肤目录（见下）；
3. 共享目录 `skins/assets/`。

因此：与共享资源同名的文件放进皮肤目录即可**局部覆盖**（如 Neon 用同名
`thumb.png`/`progress.png`/`scrollbar-vert.png` 替换了共享样式引用的图片，
无需改动 `StyleBase.xml`）。

新皮肤放入皮肤根目录后，在「偏好设置 → 皮肤」列表中即可选择；选择后写入配置
（`[<skin文件>] DefaultSkin=<文件夹名>`）成为默认皮肤。也可直接改 ini 里的
`DefaultSkin` 调试。

## 2. 皮肤文件构成

| 文件 | 必需 | 作用 |
|---|---|---|
| `main.xml` | ✅ | 皮肤定义：根 `<skin>` + 若干 `<skinwnd>` 窗口定义 |
| `Styles.xml` | 建议 | 样式库。`<include Name="StyleBase.xml"/>` 引入全部共享控件样式 |
| `main.js` | 可选 | TinyJS 脚本，由 `<skinwnd Script="main.js">` 引用，窗口加载时执行一次 |
| `theme.xml` | 可选 | 歌词配色（`LyrDispaly`/`FloatingLyr` 两节），存在时自动加载 |
| `*.png`(+`@2x`) | - | 皮肤专有图片；Retina 屏自动加载 `@2x` 版本 |

根元素：

```xml
<skin defmainwnd="MainWnd" mainwnds="MainWnd" ExtraResouceFolder="Classic">
```

- `mainwnds`：本皮肤可创建的窗口列表；`defmainwnd`：主窗口。
- `ExtraResouceFolder`：额外资源目录（另一个皮肤文件夹名），可整体借用其图片。

`<skinwnd>` 关键属性：

```xml
<skinwnd Extends="Window" Name="MainWnd"
    MinWidth="640" MinHeight="400" Width="800" Height="480"
    Menu="MainWndMenu" CmdHandler="ch_common,ch_playlist"
    Script="main.js" ContextMenu="MainContextMenu">
```

- `Extends`：引用 Styles.xml 里的窗口样式（`Window`/`WindowFrame`/`MenuWindowFrame`）。
- `CmdHandler`：C++ 命令处理链，主窗口固定写 `ch_common,ch_playlist`。
- `Menu`/`ContextMenu`：引用 `assets/menu.json` 中定义的菜单。
- `WindowImage`：窗口级 9-slice **底座**，写在 `<skinwnd>` 下（或窗口样式里）。**固定 copy**，在子控件之前铺满整窗：圆角内画底，圆角外透明像素一并写入画布，半透明窗口下角外直接透出桌面。因此**多数皮肤不必再叠一层 copy 模式的 Frame 打孔**；边框、高光、标题镀铬和底栏应画进同一张图。

主窗口与对话框各用一张（高宽不必相等，竖直方向按标题/底栏切开）：

```xml
<Property Name="WindowImage" Image="bg.png" HorzExtendPos="13,51" VertExtendPos="43,63" />
<Property Name="WindowImage" Image="bg-dialog.png" HorzExtendPos="13,51" VertExtendPos="33,51" />
```

  `ImageRect`/`ImageMask`/`HorzExtendPos`/`VertExtendPos` 与控件 `BgImage` 相同，但不要写 `BlendPixMode`（窗口级忽略该字段，始终 copy）。旧属性名 `BgImage` 在 `<skinwnd>` 上仍兼容。控件内部背景继续用 `<Property Name="BgImage">`。

## 3. 布局语法

`Rect="left,top,width,height"`，每个分量支持公式：

- `w`、`h` = 父容器宽高；支持 `+ - * /`（如 `w/2-25`、`w-10-60`）；留空 = 自动。
- 也可用分开的 `Left/Top/Width/Height` 属性。
- 容器类支持 `LayoutParams="match_parent_height"`、`Weight`、`MinWidth/MinHeight`。

控件相对父容器定位，父容器移动/缩放时按公式重算。窗口可调整大小时务必用
`w-x`、`h-x`、`w/2` 这类公式让布局自适应。

## 3.1 按操作系统切换节点（`os`）与标题栏 `<Caption>`

各平台 **Window 都不使用原生标题栏**，chrome 一律由皮肤自绘。C++ 主逻辑只有一套
（`ID_MINIMIZE` / `ID_MAXIMIZE` / `ID_CLOSE`、拖动空白处移动窗口、双击最大化）；
布局、图标、是否画 MenuBar 写在皮肤里，用 `os` 过滤：

```xml
<Caption os="win,linux" Rect="2,2,w-4,41">
  <MenuBar Name="Menu" .../>          <!-- 仅 Win/Linux 画窗口内菜单 -->
  <Text ID="ID_CAPTION" .../>
  <Toolbar ID="CID_TB_SYSBT" Image="caption_btn.png" ...>
    <button ID="ID_MINIMIZE" Left="1"/>
    <button ID="ID_MAXIMIZE" Left="2" CanCheck="TRUE" checked_left="3"/>
    <button ID="ID_CLOSE" Left="4"/>
  </Toolbar>
</Caption>
<Caption os="mac" Rect="2,2,w-4,41">
  <!-- macOS 菜单在系统菜单栏（AppDelegate 从 Menu="MainWndMenu" 同步），标题栏不画 MenuBar -->
  <Toolbar ID="CID_TB_SYSBT" Image="caption_btn_mac.png" Rect="12,12,64,16"
           units_x="16" ButtonSpacesCX="8" ...>
    <button ID="ID_CLOSE" Left="4"/>
    <button ID="ID_MINIMIZE" Left="1"/>
    <button ID="ID_MAXIMIZE" Left="2" CanCheck="TRUE" checked_left="3"/>
  </Toolbar>
  <Text ID="ID_CAPTION" AlignText="AT_CENTER | AT_VCENTER" .../>
</Caption>
```

- `os`：`win` / `mac` / `linux`，逗号分隔；省略则全平台加载。别名 `windows`、`macos`/`osx`。
- 任意 XML 子节点都可带 `os`（含 `<Property>`、Toolbar 的 `<button>`、顶层 style）。
- `<Caption>` 是容器：空白处拖动窗口，双击最大化（`EnableDblClick` 默认 true）。
- 系统按钮列语义不变：1=最小化 2=最大化 3=还原 4=关闭。mac 红绿灯图是
  `skins/assets/caption_btn_mac.png`（皮肤目录同名文件可覆盖）；XML 里用按钮顺序
  改成关闭-最小化-最大化即可。
- `skinwnd` 的 `Menu=` 仍要写：mac 系统菜单和 Win/Linux 的 MenuBar 共用同一份菜单定义。

## 4. 常用控件与图片精灵图约定

XML 标签名 = C++ 类名（注册表见 `Skin/SkinFactory.cpp` 与
`MPlayerUI/MPSkinFactory.cpp`）。样式派生：`<NeonPlaylist Extends="NormalPlaylist" ...>`。

### 4.1 Button（普通图片按钮）

竖排 3 态精灵图（normal/hover/pressed）：

```xml
<Button ID="ID_PREVIOUS" Image="prev_next.png" ImageSize="40,40"
    ImagePos="0,0" ImageFocusPos="0,40" ImageSelPos="0,80"
    ImageMask="prev_next.png" ImageMaskRect="0,0,40,40"/>
```

`ImageMask` 既是点击热区（按像素 alpha 命中测试），绘制时也会用它做 `maskBlt`
裁剪。引擎绘制蒙版时**用按钮图自己的精灵坐标去采样蒙版**
（`GfxRaw/RawImage.cpp::maskBltRawImage` 的 xMask/yMask 传的就是 xSrc/ySrc），
所以独立蒙版文件必须与按钮图**保持相同的精灵图布局**（参考
`Glass/prev_next-mask.png`，与 `prev_next.png` 同为 2 列 × 3 行）——否则
hover/pressed 行采样越界，按钮在悬停时直接消失。

双状态按钮（如播放/暂停）用 `S0_*` / `S1_*` 两组属性，图片约定 **2 列 × 3 行**
（列=状态，行=normal/hover/pressed），参考 `Neon/playpause.png`（96x144，48px/格）：

```xml
<Button ID="ID_PLAYPAUSE" S0_Image="playpause.png" S0_ImageSize="48,48"
    S0_ImagePos="0,0" S0_ImageFocusPos="0,48" S0_ImageSelPos="0,96"
    S1_Image="playpause.png" S1_ImageSize="48,48"
    S1_ImagePos="48,0" S1_ImageFocusPos="48,48" S1_ImageSelPos="48,96"/>
```

### 4.2 Slider / SeekCtrl（进度、音量）

样式定义于 `StyleBase.xml`：`<Slider Extends="SeekCtrl" EndWidth="8" ImageThumb="thumb.png" ImageTrack="progress.png"/>`。

- `ImageTrack`：**上半 = 未播放轨道，下半 = 已播放轨道**（引擎自动对半切，
  `Skin/SkinSeekCtrl.cpp`）。两端 `EndWidth` 像素不拉伸，中间 x 方向缩放。
  参考 `Neon/progress.png`（60x8，各半 60x4）。
- `ImageThumb`：竖排 3 态（normal/hover/pressed），参考 `Neon/thumb.png`（15x45）。

内置行为：`ID_SEEK`（播放进度）、`ID_VOLUME`（音量）由 C++ 自动绑定。

### 4.3 Toolbar（图标工具条 / 标题栏按钮 / 视图 Tab）

精灵图按 **列 = 按钮/状态，行 = normal/hover/pressed** 排布；
`units_x` = 每格宽度，`<button Left="n">` 以 units_x 为单位取列。
`FullStatusImage="TRUE"` 表示整格（含背景）都取自图片。

```xml
<Toolbar Image="caption_btn.png" units_x="16" blank_x="-1" blank_cx="16"
    seperator_x="0" seperator_cx="10" ButtonSpacesCX="5" FullStatusImage="TRUE">
  <button ID="ID_MINIMIZE" Left="1" />
  <button ID="ID_MAXIMIZE" Left="2" CanCheck="TRUE" checked_left="3"/>
  <button ID="ID_CLOSE" Left="4" />
</Toolbar>
```

- `CanCheck="TRUE"` + `checked_left="m"`：可选中按钮，选中时取第 m 列。
- `RadioGroup="1"`：同组按钮互斥（radio），选中一个自动取消其他
  （`CSkinToolbar::groupButtonUncheckOld`）。Neon 右下角的「列表/歌词」
  视图 Tab 即此用法（`view_tabs.png`：4 列 = 列表/歌词 × 未选/选中，3 行状态）。
- 标题栏按钮列语义（沿用 Classic/Metal 约定）：1=最小化 2=最大化 3=还原 4=关闭。

### 4.4 Frame（边框，可选）

在已有 `WindowImage` 时，圆角透明由底座 copy 完成，**一般不再需要 Frame**。
仅当边框必须独立拉伸（例如边条渐变、与填充分离的描边）时才用本控件。

九宫格边框：4 个 `RoundWidth` 方角 + 4 条 `ThickWidth` 边，**不填充中心**
（`Skin/SkinFrameCtrl.cpp`）。边条在图片边缘取样后平铺，**平铺方向上的渐变会
出现接缝**——把渐变放在圆角内、边条用单色（见 `Neon/raw` 的 frame 生成思路）。

```xml
<Frame Image="frame.png" ImageRect="0,0,64,64" RoundWidthTop="10"
    RoundWidthBottom="10" ThickWidth="3" BlendPixMode="copy"/>
```

无 `WindowImage` 的旧皮肤仍可靠 `BlendPixMode="copy"` 的 Frame 把圆角外打穿。
新皮肤请把圆角底座（含描边）画进 `bg.png`，用 `WindowImage`。

### 4.5 任意控件的 9-slice 背景（BgImage）

所有 CUIObject 支持 `<Property Name="BgImage">`，带双向拉伸点（真正的九宫格）：

```xml
<Container Rect="...">
  <Property Name="BgImage" Image="panel.png"
      HorzExtendPos="11,21" VertExtendPos="11,21" BlendPixMode="alpha_blend"/>
</Container>
```

`HorzExtendPos="a,b"`：0~a 与 b~图宽 不拉伸，中间水平拉伸；`VertExtendPos` 同理。
圆角面板/气泡背景都用这个（`Neon/panel.png`、`search_bg.png`、`caption_bg.png`）。

### 4.6 Playlist（播放列表）

直接用样式覆盖配色即可（`StyleBase.xml` 的 `NormalPlaylist` 定义了全部默认值）：

```xml
<NeonPlaylist Extends="NormalPlaylist"
    StripeColor="#1B1B36,#17172E" SelBgColor="#5B3BD6"
    NowPlayingBgColor="#12786B" NowPlayingTextColor="#FFFFFF"
    TextColor="#C9C9E0" SelTextColor="#FFFFFF" LineHeight="30"/>
```

主窗口中：`KeywordEdit="CID_E_SEARCH_MUSIC"` 关联搜索框；右键菜单由样式里的
`ContextMenu="PlaylistMenu"` 提供。

### 4.7 其他主窗口控件

- `AlbumArt`（封面）：`Image` 默认封面 + `FrameMask` 圆角蒙版。蒙版约定：
  白色 RGB + alpha 圆角矩形，**比封面图小一圈并居中**（引擎把封面缩放到蒙版
  大小后按 alpha 相乘，`MPlayerUI/MediaAlbumArtCtrl.cpp`）。
- `MediaInfoText`：`MediaInfoType="Title|Artist|Album|Duration|..."`，
  `CombineWith=" - "` 连接多项。
- `PlayingTimeText`：已播时间（点击切换为剩余时间）。
- `LyricsShow`（歌词）：背景由父容器绘制（`SetSkinBg="true"`，默认值）；
  `SetSkinBg="false"` 时歌词对象用 profile 的 BgColor **方形**填充自身区域，
  会盖住圆角面板的角——圆角面板上的歌词区应保持默认/true 并靠父容器背景透出。
  文字配色由 theme.xml 写入 profile（`LyrDispaly`/`FloatingLyr` 节）控制。
- `RateButton ID="ID_RATE"`：评分星。
- `EditCtrl` 编辑框**内容背景固定白色**（`CSkinEditCtrl` 无换色属性），
  深色皮肤里用白色圆角容器底 + 内嵌编辑框解决（见 Neon 搜索条）。

## 5. 命令 ID：C++ 自动绑定 vs JS 处理

- **内置 ID**（`MPlayerUI/MLCmd.h`）：`ID_PLAYPAUSE`、`ID_PREVIOUS`、`ID_NEXT`、
  `ID_SEEK`、`ID_VOLUME`、`ID_RATE`、`ID_SHUFFLE`、`ID_LOOP`、`ID_MINIMIZE`、
  `ID_MAXIMIZE`、`ID_CLOSE`、`ID_OPEN_LRC`、`ID_BACKWARD_LYRICS` 等，
  写上 ID 即自动获得行为（`MPlayerUI/MPSkinWnd.cpp::onAddUIObj`）。
- **自定义 ID**：命名 `CID_*` / `UID_*`，引擎自动分配数值（`ID_ID_USER_BASE` 起），
  在 JS 里用 `document.getCommandID('CID_XXX')` 解析后自行处理。
  `Toolbar` 对自定义 ID 的命令会自动同步按钮选中态
  （`Skin/SkinToolBar.cpp::onCommand`），所以脚本可用
  `document.postCommand(CID_XXX)` 在启动时恢复 tab 选中状态，
  与按钮点击走同一条路径（参考 `Glass/main.js`）。

## 6. TinyJS 脚本（main.js）

API 注册于 `Skin/api-js/SkinJsAPI.cpp`，可用面很小：

```js
// document（每个 skinwnd 一个）
var id  = document.getCommandID('CID_SHOW_LYRICS');   // 自定义名 -> 数值
var obj = document.getElementById('CID_LYRICS');      // 按 Name 或 ID 取控件
document.oncommand = function(cmd) { /* 按钮/命令回调 */ };
document.onsize = function(w, h) {};
document.height = 400;                // 改窗口高度
document.postCommand(id);             // 向窗口投递一条命令(走正常派发路径)
document.startAnimation(id); document.stopAnimation(id);

// UIObject
obj.visible = false;                  // 显隐
obj.onmouseenter = function() {}; obj.onmouseleave = function() {};

// profile（ini 的 [SkinJs] 节持久化）
var v = profile.getInt('Neon-ShowLyrics', 0);
profile.writeInt('Neon-ShowLyrics', 1);

// 动画：XML 里 <Animation ID=".." FadeIn/FadeOut="目标ID"/>
```

`console.log` 输出到应用日志；VM 由 4ms 定时器驱动，`setTimeout/setInterval` 可用。

## 7. 图片与 @2x

- 皮肤 PNG 由 Skin Compiler 从 `Skins-Design/skin-for-ai/<name>.skin.json` 生成：
  `cd Skins-Design/skin-for-ai/compiler && pnpm glass`（或 `pnpm neon` / `pnpm crystal`）。
  格式见 `docs/skin-compiler.md`。不要再写 `Skins-Design/raw/*_gen_assets.py`。

## 8. 调试技巧

1. `./build.sh Debug -b` 编译；皮肤文件（XML/JS/PNG）**改动不需要重新编译**，
   重启 App 或在偏好设置里切换一次皮肤即可生效。
2. 启动日志（含皮肤属性告警、JS `console.log`）写在
   `~/Library/Application Support/com.crintsoft.MusicPlayer/MusicPlayer.log`；
   从终端直接运行 `build/Debug/MusicPlayer.app/Contents/MacOS/MusicPlayer`
   可在 stdout 看到。
3. `Unknown property: xxx` 告警 = XML 属性名写错或该控件不支持。
4. 快速切换默认皮肤：改 `build/Debug/MusicPlayer.ini` 的
   `[main.xml] DefaultSkin=<文件夹名>`。
5. **`SkinRootDir` 必须写在 `[MusicPlayer]` 节**（profile 默认节名，见
   `MPEventsDispatcher.h` 的 `SZ_SECT_UI`）。旧机器上的 ini 可能把它放在遗留的
   `[MP3Player]` 节下——那样会被静默忽略，皮肤从 app bundle 的
   `Resources/skins` 加载（日志表现为新皮肤 "main.xml does NOT exist" 并回退
   Metal；旧皮肤能"正常"加载只是因为 bundle 里有构建时的旧拷贝）。
   另外注意：`DefaultSkin` 指向的皮肤是**直接加载、不应用 theme.xml** 的
   （theme 只在用户在偏好设置里手动切换皮肤时写入 profile）；调试歌词配色时
   要么手动切换一次皮肤，要么直接改 ini 的 `[LyrDispaly]`/`[FloatingLyr]` 节。
6. 窗口拖动/缩放异常时先检查 `<skinwnd>` 的 `MinWidth/MinHeight` 与 Rect 公式
   在小尺寸下是否为负。
7. **窗口内部透明（只有底座和显式背景图可见）**：`<skinwnd>`/Window 样式上的
   `BgColor` 会被转发给根容器（日志可见 "Property is set to Root Container"），
   但根容器自身的背景填充在 mac 上不显示。主窗口请用窗口级
   `<Property Name="WindowImage" ... />`（copy 铺满整窗，圆角外透明即打孔），
   对话框同样挂 `WindowImage` 即可，不必再叠 Frame。
   参考 Glass/Crystal/Neon 的 `bg.png`（主窗口）与 `bg-dialog.png`（对话框）。
8. **浮动歌词窗口**：固定加载 `floatinglyr.xml`（`MPFloatingLyrWnd.cpp`），
   皮肤目录放同名文件即可覆盖 assets 版本（assets 版 `BgColor` 写死蓝色）。
9. **专辑封面 FrameMask**：蒙版是相对于 `Image`（默认封面图）居中的内缩区域，
   封面按蒙版大小缩放后相乘——蒙版尺寸跟随默认封面图，而不是控件 Rect。
   蒙版 alpha 用**圆形**即可得到圆形封面（Glass 皮肤用它把封面做成唱片标签）。
10. **半透明装饰必须叠层，不要原地覆盖 alpha**：旧 PIL 脚本在 RGBA 上
    `ImageDraw` 会把像素 alpha 一并替换。Compiler 走 SVG composite，没有这个坑。
    手写位图时仍须独立图层再合成。
