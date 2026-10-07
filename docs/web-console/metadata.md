# Personal Music Center — 音乐库元数据推断与整理模块设计

> 面向本地音乐库的**元数据推断 / 规范化**模块。目标：把原始标签混乱、文件名杂乱的音乐库逐步整理成干净的媒体库，供 Web 中心与播放器展示、搜索、统计使用。
> 相关文档：[data-model.md](data-model.md) · [api.md](api.md) · [architecture.md](architecture.md) · [plan.md](plan.md) · [album-art-download.md](../album-art-download.md)

## 1. 目标与边界

**核心策略**：

> **标签优先，但不盲信标签；路径和文件名作为第二来源；多来源推断后再做规范化、垃圾词过滤与置信度评分。**

本模块**不直接改写音频文件（不自动回写 ID3 Tag）**。推断结果只写入数据库与进程内 `Media`，用户看到干净的 `Artist / Album / Title`；原始文件始终不动。未来需要"写回标签"时，再作为显式操作单独实现（见 [§16](#16-不自动回写音频文件)）。

本模块只推断**展示与统计需要**的字段：`title / artist / album / year / track_number / disc_number / version / genre`。封面、音频技术参数（码率、采样率）、时长**不在本模块职责**（沿用 `MediaTags` 与扫描流程）。

## 2. 定位：一个独立 C++ 模块

新建目录 `MetadataInference/`（与 `MediaTags/`、`LyricsLib/` 同级），在现有 C++17 代码库上扩展，复用既有播放器与媒体库基础设施。

依赖关系：只向上依赖 `Utils`、`MediaTags`、`MPlayer`（`Media` 结构、`MediaLibrary`）、`LocalServer` 的数据来源；不反向依赖 UI。所有推断逻辑对三平台一致，不放平台子目录。

```text
MetadataInference/
├── MetadataInference.h/.cpp         // 流程编排入口（两遍扫描的调度）
├── MetadataResult.h                 // MetadataField / MetadataResult / MetadataStatus
├── TagReader.cpp                    // 包装复用 MediaTags::getTags
├── PathParser.cpp                   // 完整文件路径层级解析
├── FilenameParser.cpp               // 文件名解析
├── Normalizer.cpp                   // Unicode NFC / 大小写 / 分隔符统一
├── Tokenizer.cpp                    // 分词（含全角符号、多空格）
├── GarbageFilter.cpp                // 分级垃圾词表 + 正则规则
├── SemanticParser.cpp               // 提取 Live/Remix/Acoustic 等修饰词
├── NumberParser.cpp                 // track / disc 序号解析
├── YearParser.cpp                   // 年份解析
├── FieldInference.cpp               // Artist/Album/Title/Year 候选评分（分类而非位置切片）
├── CorporaCollector.cpp             // 第一遍整库词元统计
├── ConfidenceScorer.cpp             // 置信度评分与状态判定
├── MetadataResolver.cpp             // 外部匹配（复用 MusicBrainz 请求路径）
├── MetadataMatcher.h                // 外部匹配抽象接口（接口形式参照 IAlbumArtSource）
└── config/metadata_words.json       // 垃圾词配置（见 §7）
```

> 说明：配置用 rapidjson 解析 `Resources/metadata_words.json`（`getResourceFile`），以 `getAppDataDir()` 下的同名文件覆盖；项目无 YAML 加载器，rapidjson 已是既有依赖。

## 3. 核心数据模型

单个字段同时保留三份信息——**原始值、来源、置信度**——不直接拿一个裸字符串当结论。注意这些是**推断过程中的内存结果**，并非全部落盘；最终只持久化展示需要的东西（见 [§3.2](#32-对-medialib-表的改动只持久化必要的)）。

```cpp
enum MetadataStatus { RAW, INFERRED, NORMALIZED, VERIFIED };

enum MetadataSourceType {
  Tag,        // 嵌入标签
  Path,       // 路径层级
  Filename,   // 文件名
  Corpus,     // 整库词元统计得出
  External,   // 外部数据库匹配
  Manual      // 用户/人工修正
};

// 单字段：值 + 来源 + 置信度（对应参考的 MetadataField）
struct MetadataField {
  std::string        value;
  MetadataSourceType source;
  float              confidence = 0.0f;  // 0.0 ~ 1.0
};

// 一次推断的完整结果
struct MetadataResult {
  MetadataField title;
  MetadataField artist;
  MetadataField album;
  MetadataField year;
  MetadataField trackNumber;
  MetadataField discNumber;
  MetadataField version;
  MetadataField genre;
  MetadataStatus status = MetadataStatus::RAW;
};
```

### 3.1 对 `Media` 结构（`MPlayer/Media.h`）的改动

现有 `Media` 已有 `artist/album/title/genre/trackNumb/year`，缺少 `disc`、`version`、推断状态。增量新增：

```cpp
int16_t        discNumb;          // 缺省 -1（与 trackNumb 缺省一致）
MetadataStatus metaStatus = MetadataStatus::RAW;   // 0 RAW / 1 INFERRED / 2 NORMALIZED / 3 VERIFIED
```

`title/artist/album` 存**规范化后**的展示值；`version` 单独存储（见 §9）。

### 3.2 对 `medialib` 表的改动：只持久化必要的

现有 `SQL_CREATE_MEDIALIB`（`MediaLibrary.cpp:26`）是扁平单表。展示与统计需要的值落在既有列 + 两个新增列，**不新增 JSON 列**：

```sql
-- 在原有列基础上增量新增（须走 additive 迁移，见 §17）
ALTER TABLE medialib ADD COLUMN disc_number INTEGER;
ALTER TABLE medialib ADD COLUMN metadata_status INTEGER;   -- 0 RAW / 1 INFERRED / 2 NORMALIZED / 3 VERIFIED
```

写库的字段 = `artist/album/title/year/track_number/disc_number/version(标题版本)/genre` + `metadata_status`，即**扫描的输出**——这是产品真正要持久化的值（展示、搜索、统计都读它，`url` 已保留原始路径）。

**不持久化 `metadata_json`（每字段 source + confidence）与 `metadata_version`：**

- 每字段的 `source + confidence` 只在中途用两处：① 判阈值 0.8 决定能否自动入库（即时判定，不用存档）；② 渲染 Metadata Issues 审查页（只显示少数低置信度歌曲，按需对那几首**现场重推**，见 §15）。歌曲一旦变干净，这份 JSON 就没有消费场景，成了死数据。
- `metadata_version` 与现有 `file_size + modified_at` 变更检测冗余：文件一改就该重跑推断，不需要额外计数器。

原始标签值（raw_tag_json）同样**不持久化**：扫描时本就重读标签，需要时再读即可。

## 4. 总体流程（两遍扫描）

利用整库做语料级推断，而不是一首首独立处理。整体分两遍：

> **不是每次启动都全库扫两遍。** 第一遍统计只对内存中已加载的 `url` 字符串做词频聚合（不读音频、开销可忽略），且每次扫描现算、不存档；第二遍只对**新增 / 变化 / 仍未 VERIFIED** 的文件推断，已确认（VERIFIED）且文件未动的歌曲直接跳过。磁盘扫描本身是显式触发（`POST /library/scan`）或按计划自动触发，不是每次启动都跑。

```text
第一遍  建立整库词元统计（CorporaCollector）
         统计每个词元：出现次数、出现在多少个文件/目录、所在层级、
                   是否像数字、与其他词元的共现
               ↓
第二遍  逐文件推断（图见下）
```

扫描开始时（复用 `MediaScanner::startLibraryRescan` 编排，见 §5），先从当前库 `getAll()` 的 `url` 加上本次发现的新文件，构建第一遍统计；第二遍对每个文件走下方流程：

```text
         音乐文件
             │
             ▼
     读取嵌入标签（复用 MediaTags::getTags）
             │
   ┌─────────┴─────────┐
   ▼                   ▼
 嵌入标签值         完整路径 / 文件名
 （可靠性评分）           │
                 ┌──────┴──────┐
                 ▼             ▼
            路径层级解析     文件名解析
                 └─────┬──────┘
                       ▼
                  规范化（Unicode NFC / 大小写 / 分隔符）
                       ▼
                      分词（含全角符号）
                       ▼
                技术垃圾 / 来源垃圾 / 宣传词过滤
                       ▼
                提取语义修饰词 → version
                       ▼
                解析 track / disc / year
                       ▼
              Artist / Album / Title 候选评分
                （结合第一批整库统计，分类而非切片）
                       ▼
                合并标签值 + 推断值（按来源优先级）
                       ▼
                  置信度评分 → 状态判定
              ┌────────────┴────────────┐
              ▼                         ▼
      置信度 ≥ 0.8                置信度 < 0.8
       自动入库                   进入 Metadata Issues
    status=INFERRED→NORMALIZED    待外部匹配/人工确认
```

明确的处理顺序（固定为严格分步，顺序影响结果）：

```text
1  读嵌入标签
2  解析完整路径
3  解析文件名
4  Unicode 规范化
5  分词
6  检测技术垃圾（320K / FLAC / MP3 …）
7  检测来源/宣传垃圾（WEB-DL / iTunes / 无损 …）
8  提取语义修饰词（Live / Remix / Acoustic …→ version）
9  解析 track / disc 序号
10 解析 year
11 推断 artist / album / title
12 整库一致性分析（复用第一遍统计）
13 合并标签值 + 推断值
14 计算置信度
15 写库
```

#### 第一遍统计是否持久化？

**不建议默认落盘。** 第一遍是整个 `url` 列表的词频聚合——纯内存字符串处理，上万首歌也就毫秒级，每次扫描现算即可，也不用持久化，省去文件增删改时维护统计增减量的同步问题。仅当实际性能测试证明大库下这步成了瓶颈，再把它落到 `settings` 或独立 `metadata_corpus` 表做增量更新（此时要处理文件删除时的减法）。

## 5. 既有接口的复用与改动

| 现状 | 本模块的做法 | 类型 |
|---|---|---|
| `MediaTags::getTags(fileName, BasicMediaTags&, ExtendedMediaInfo&)`（`MediaTags.h:11`） | `TagReader.cpp` 包装；`BasicMediaTags.trackNo/year` 是字符串，解析成数值 | 复用+包装 |
| `CPlayer::loadMediaTagInfo/getArtistTitleFromFileName`（`Player.cpp:812/34`）——用"拆 `" - "` 再拆 `-`"的朴素启发式 | 改为把文件名/路径交给本模块推断；`getArtistTitleFromFileName` 被替换 | **改动** |
| `MediaScanner::startLibraryRescan/scanMedia` + `CPlayer::updateMediaInfo` 写库（`MediaScanner.cpp`、`MediaLibrary.cpp:523`） | 本模块挂进 `updateMediaInfo`/`loadMediaTagInfo`，在写库前完成推断 | **改动（接入点）** |
| `CMediaLibrary::addFast`（`MediaLibrary.cpp:468`）——先在文件名解析 artist/title 再排后台补全 | 快速路径也走本模块（先做文件名推断，标签就绪后升级） | 复用+接入 |
| `CMusicBrainzAlbumArtSource`（`MPlayerUI/`）——已按 artist/album/title 查 MusicBrainz 并评分 | 提取共享 `MetadataMatcher` 接口（查询三元组 + 打分），供元数据解析与封面对接 | 复用+重构 |
| `MediaIdentity`（`MPlayerUI/IAlbumArtSource.h`，字段 artist/album/title） | 作为外部匹配的查询入参，**上移到跨模块共用** | 复用+上移 |
| `HttpServer::ApiHandler`（`LocalServer/Http/ApiHandler.cpp`） | 新增元数据审查端点并在 `writeMediaJson` 暴露 status/confidence（见 §15） | 改动 |

### 5.1 关键接入点：`updateMediaInfo` 里完成推断

替换 `getArtistTitleFromFileName` 后，写库前：

```cpp
if (needMetadataInference(media)) {
    MetadataResult r = g_metadataInference.infer(media);
    applyResultToMedia(media, r);      // 仅改进程内 + 待写库字段，不改音频文件
}
```

触发条件：`metadata_status == RAW`、字段为空、或文件已被改动（`file_size / modified_at` 变化）。已 `VERIFIED` 且文件未动的歌曲直接跳过、不重跑。推断失败或低置信度时**保留原标签值**、`metadata_status` 保持 `RAW`，该曲进入 Metadata Issues 待人工确认——确认页的建议值按需现场重推（见 §15）。

## 6. 分词（Tokenizer）

无论目录还是文件名，先全部拆成词元，再做后续过滤；支持全角与多空格分隔符：

```text
- _ . ( ) [ ] { }      以及全角  － ＿ ． （ ） 【 】
以及连续多个空格
```

例：`01 - 周杰伦 - 七里香 [320K] (Official Audio).mp3`

```text
01
周杰伦
七里香
320K
Official Audio
```

## 7. 垃圾词过滤（分级 + 配置化）

做成**分级表**，而不是 `if contains("mp3")` 的散写：

| 级别 | 语义 | 例 | 处理 |
|---|---|---|---|
| hard（技术） | 音质/格式/参数 | `320k` `flac` `24bit` `44.1khz` | **直接删** |
| hard（来源） | 发布组/平台/压制组 | `web-dl` `itunes` `qobuz` `cdda` `rip` | **直接删** |
| soft（宣传） | 中文诱惑词 | `无损` `高品质` `免费下载` `收藏版` `正式版` | 删（除非组成标题语义） |
| semantic（修饰词） | 版本语义 | `live` `remix` `acoustic` `instrumental` | **不删**，进 `version`（见 §9） |

正则规则（示例）：`\d{2,4}kbps`、`\d+K`、`\d+bit`、`\d+(\.\d+)?kHz`、`\[[A-Z0-9_-]{2,20}\]`。

配置来源 `config/metadata_words.json`：

```json
{
  "hard": {
    "technical": ["320k", "320kbps", "flac", "mp3", "wav", "24bit", "44.1khz"],
    "source": ["web-dl", "itunes", "qobuz", "deezer", "cdda", "cdrip", "dvdrip"]
  },
  "soft": {
    "promotional": ["无损", "高品质", "免费下载", "收藏版", "正式版", "试听版"]
  },
  "semantic": {
    "version": ["live", "remix", "acoustic", "instrumental", "unplugged",
               "radio edit", "extended mix", "club mix", "demo", "mono", "stereo"]
  }
}
```

规则持续积累时**只改配置，不改代码**。

### 7.1 方括号内容分类

`[320K]` 删、`[WEB-DL]` 删、`[Live]` → version、`[Remix]` → version、`[Official]` → 删。本质是**括号内容分类**：先查 semantic 表，是则进 version；否则按 hard/soft 决定是否删，未知内容保守保留到值里并在置信度上扣分。

## 8. 规范化（StringNormalizer）

所有字符串先统一：

- **Unicode NFC**：`覚醒` 与组合写法归一。
- 大小写：比较类字段生成 normalized key，`The Beatles` / `the beatles` 归一为 `the beatles`，**展示仍保留原始规范形式**。
- 统一全角/半角分隔符、压缩多空格、去首尾空白。

（`medialib` 现有 `COLLATE NOCASE`，只处理 ASCII 大小写；中文需走本模块的 NFC + 全角规范化。）

## 9. 语义修饰词 → `version` 字段

`version` 不能当作标题垃圾删掉，它是歌曲语义的一部分：

```text
Coldplay - Yellow (Live).mp3
   title   = Yellow
   version = Live
```

展示时拼 `Yellow (Live)`，库里仍分存，便于去重、统计、搜索。缺失新增字段见 §3.1。

## 10. 序号与年份解析

### 10.1 track / disc

`NumberParser` 支持：`01`、`01.`、`1-`、`A01`、`01-02`、`Disc 1 Track 2`、`1-01`。`Disc 1/01` 或 `1-01` → `disc_number=1, track_number=1`。

### 10.2 year

`YearParser` 支持 `2004`、`[1999]`、`(1999)`、`1999-Album`、`Album (1999)`，并约束 `1900 <= year <= 当前年 + 1`，超出不算年份（`1980` 也可能是别的，交给评分）。

## 11. Artist / Album / Title 推断：分类，不是切片

不要写死 `parts[len-3]=artist`，因为目录深度不固定、中间还有 Genre 层：

```text
Music/Chinese/Pop/周杰伦/七里香/01 - 我的地盘.mp3
   Chinese  → Genre/分类
   Pop      → Genre
   周杰伦    → Artist
   七里香    → Album
```

字段识别是**分类问题**：对每个路径词元做候选评分（positive/negative 分数相加）：

```text
Artist 候选：+40 目录位置合理
             +30 在多个歌曲路径中重复出现（借第一遍整库统计）
             +20 不含垃圾词
             +20 符合人名/乐队名形态
             +30 与嵌入标签 artist 一致
             -40 像 Genre（低频且是常见分类词）
             -30 像 year（数字，4 位 19xx/20xx）
Album 候选：类似，权重换位
```

结果落成：`周杰伦=93 / 七里香=21 / Pop=8 / 2004=0` → artist=`周杰伦`、album=`七里香`。

**整库重复是强信号**：`周杰伦` 出现在几十个文件路径 → 很可能是 Artist；`01 02 03` → 明显 track。这正是走两遍扫描的原因。

## 12. 标签可靠性评分与来源优先级

"可靠"不等于"存在"。标签值本身也要打分：

```text
artist = "Unknown Artist" / "Various Artists" → 0
title  = "Track 01" / "01 - xxx"（模板化）  → 0
```

来源优先级：

```text
可靠的嵌入标签
      >
高置信度的路径推断
      >
文件名推断
      >
弱启发式
```

冲突处理例：标签 `Unknown/Track 01` + 路径 `周杰伦/七里香/01 - 我的地盘.mp3` → 听路径；标签 `Jay Chou/七里香/我的地盘` + 文件名 `01 - My Place.mp3` → 听标签的 artist/album，title 依置信度判断（标签更可信时用标签）。

## 13. 置信度评分与状态机

`ConfidenceScorer` 汇总各字段置信度（0.0~1.0）。阈值：`>= 0.8` 自动入库并置 `NORMALIZED`；`< 0.8` 进 Metadata Issues（§15）。

状态变迁：

```text
RAW -----> INFERRED -----> NORMALIZED -----> VERIFIED
  扫描推出     已清洗（自动）     用户确认 / 可靠外部数据
```

## 14. 外部匹配（MetadataResolver）

`confidence < 0.8` 的歌曲不直接放弃，进入外部匹配。做文本匹配，**音频指纹最后做**：

```text
Artist + Title
Artist + Album
Album + Track Number
```

方向复用现有 MusicBrainz 请求路径（`buildReleaseQuery/buildRecordingQuery/luceneEscape` 与 `httpGetUrl`），抽象成共享 `MetadataMatcher` 接口（接口形式参照 `IAlbumArtSource`），后续可加 AcoustID、国内源。匹配成功返回 `source=External` 的高置信度结果。

## 15. API 与 UI：自动整理 + 可人工修正

本模块让"规则到 100% 准确"不必要——低置信度进人工确认，比硬凑规则更现实。

### 15.1 API 改动（`api.md`）

- `writeMediaJson` 的 song 对象新增：`version`、`metadata_status`、`metadata_confidence`（数值或 null）。
- `GET /api/v1/metadata/review` → 列出 `metadata_status` 不为 `NORMALIZED/VERIFIED` 且置信度不足的歌曲。每一项的 `{ current, suggested, confidence }` 为该曲**按需现场重算**（重走一遍推断，读路径 + 必要时读标签），不依赖落盘字段。
- `PUT /api/v1/songs/{id}/metadata` → 手动/外部匹配结果写回，`status=VERIFIED`。
- 拉起扫描后自动跑推断（挂在现有 `POST /library/scan`），无需独立端点。

### 15.2 审查交互

```text
┌─────────────────────────────────────────────┐
│ 01 - Unknown - Track 01.mp3                 │
│                                             │
│ Artist   [周杰伦       ] ✓ 98%             │
│ Album    [七里香       ] ✓ 94%             │
│ Title    [我的地盘     ] ✓ 96%             │
│                                             │
│             [ Apply ] [ Ignore ]            │
└─────────────────────────────────────────────┘
```

## 16. 不自动回写音频文件

第一版绝不回写 Tag。流程止于：

```text
File → Inference → Database → UI
```

用户看到干净 `Artist/Album/Title`，原文件不动。只有当用户显式执行"写回标签"时才写出（且是重写整个 `BasicMediaTags`，不是只改推断字段）。这样算法出问题可随时重扫，不会污染源文件。

## 17. 迁移：优先 additive，不炸库

现状 `upgradeCheck`（`MediaLibrary.cpp:1303`）在版本不匹配时 `DROP_MEDIALIB_TABLE` 重置，**会丢库**。新增列不能再走这条路。

**改动**：把 `upgradeCheck` 从"版本不符就重建"改成**追加式迁移**——按 `settings.version` 逐级 `ALTER TABLE ADD COLUMN`（列已存在则跳过），`play_history` 这类新表继续 `IF NOT EXISTS`。`data-model.md` 建议的 `normalized_name` 独立列在此也不做，展示与统计直接用既有 `artist/album/title` 列；本模块只新增 `disc_number` 与 `metadata_status` 两列（见 §3.2）。

## 18. 实施阶段与验证

按垂直切片推进（每阶段可运行）：

1. **数据模型**：`Media` 加字段 + `medialib` 追加式迁移 + `MetadataResult` 类型。验证：`./build.sh Release -b` 通过。
2. **单文件推断核心**：分词 / 规范 / 垃圾过滤 / 序号 / 年份 / version / 评分。gtest 覆盖各种文件名样例（`MediaTags/LrcParser.cpp` 底部同款 `TEST`）。
3. **两遍扫描与接入**：`CorporaCollector` + 挂进 `updateMediaInfo`。验证：`curl /library/scan` 后 `GET /library/snapshot` 看干净字段与 `metadata_status`。
4. **外部匹配**：提取 `MetadataMatcher`、接 MusicBrainz。验证：乱标签经典曲目能补全到 VERIFIED。
5. **API + UI（Metadata Issues）**：审查端点 + 前端列表。

测试注意：单文件推断用 `-DUT=ON` 的 gtest；纯语法检查可用 `clang++ -std=c++17 -fsyntax-only -D_MAC_OS -I. -I./TinyJS MetadataInference/FilenameParser.cpp`。

## 19. 风险与注意

- **不误删语义**：`Live/Remix/Acoustic` 走 version，绝不进 title 垃圾（§9）。
- **展示与审查一致**：展示始终用落库的 `Media.title/version` 拼；审查页的建议值是现场重推的，二者可能因文件后续变化而不一致——审查页需标注"按当前文件重算"，避免误导。
- **Release NDEBUG**：改动不得触碰根 CMake 的 `NDEBUG` 约定（CLAUDE.md）。
- **新增 `MetadataInference/*.cpp` 后**须 `./build.sh -g` 再编译（CMake `aux_source_directory`）。
- **不把推断逻辑塞进平台无关层之外**：平台差异只出现在需要的地方，本模块对三平台保持一致。