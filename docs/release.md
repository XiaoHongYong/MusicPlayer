# 多平台发布流程（macOS + Windows）

本机打包后，将同一版本同步发布到 **GitHub** 与 **Gitee** Release。不使用 CI。

相关实现：

- 发布脚本：[`tools/release.py`](../tools/release.py)
- Agent skill：[`.cursor/skills/release-prepare/SKILL.md`](../.cursor/skills/release-prepare/SKILL.md)

## 范围

| 项 | 约定 |
|---|---|
| 平台 | macOS（dmg）、Windows（NSIS exe） |
| 托管 | GitHub + Gitee（默认两边都创建/上传） |
| 构建位置 | 各平台本机 `./build.sh` |
| 站点 scp | 仍用仓库旁 `Release/push.sh`，**不**并入本流程 |

## 版本号与目录

- 从 [`version.h.in`](../version.h.in) 读 `MAJOR_VERSION` / `MINOR_VERSION`
- `BUILD` = 当前 commit 短 hash（7 位，与 `tools/build.py` 一致）
- `VERSION` = `{MAJOR}.{MINOR}.{BUILD}`，例如 `1.0.a1be871`
- Git tag = `v{VERSION}`，例如 `v1.0.a1be871`
- 本地产物目录：仓库旁 `../Release/{VERSION}/`（即 `ProjectsPrivate/Release/{VERSION}/`）

`./build.sh` 打包后通常写入：

- `MusicPlayer.dmg`（macOS）
- `MusicPlayer.exe`（Windows）
- `music-player-update.json`（站点更新用，本流程不上传到 Git 托管）

上传到 Release 时脚本会重命名为：

- `MusicPlayer-{VERSION}-macOS.dmg`
- `MusicPlayer-{VERSION}-windows.exe`

## 四步流程

```text
prepare (skill + release.py)
    → 人工确认 Release Notes
    → 各平台 ./build.sh 打包
    → release.py upload
```

### 1. Prepare

由 Agent 按 skill「准备发布」执行，或手动：

```bash
python3 tools/release.py prepare
```

脚本会：

1. 计算 `VERSION` / `TAG`
2. 创建 `../Release/{VERSION}/`
3. 若本地无该 tag 则创建 annotated tag，并推送到 remote `gh`（GitHub）与 `origin`（Gitee）
4. 若不存在则写入 notes 占位文件：
   - `RELEASE_NOTES.en.md`
   - `RELEASE_NOTES.zh-CN.md`

Agent 应根据「上一 tag → HEAD」的 `git log` 填好中英文说明，然后**停下等你确认**。

### 2. 人工确认 Release Notes

编辑上述两个文件（可只改中文或两边都改）。确认前不要 `upload`。

### 3. 各平台打包

在**同一 commit**（建议 `git checkout v{VERSION}`）上：

```bash
# macOS / Windows（Git Bash）完整 Release 流水线
./build.sh
```

产物落入 `../Release/{VERSION}/`。可先打一边再上传，另一边打完后再跑一次 `upload`（同名资产会覆盖）。

### 4. Upload

```bash
# 默认 GitHub + Gitee；远端无 Release 则创建
python3 tools/release.py upload

# 只传一边
python3 tools/release.py upload --host github
python3 tools/release.py upload --host gitee

# 指定版本（目录名，可带或不带 v）
python3 tools/release.py upload 1.0.a1be871
```

Release 正文优先合并 `RELEASE_NOTES.en.md`，并附加中文节（来自 `RELEASE_NOTES.zh-CN.md`）。

## 发布脚本命令

```bash
python3 tools/release.py              # 无参数：交互模式（方向键选命令/参数）
python3 tools/release.py prepare
python3 tools/release.py upload [VERSION]
python3 tools/release.py list
python3 tools/release.py view [VERSION]
python3 tools/release.py delete [VERSION] [--delete-tag] [-y]
```

全局参数：

- `--host all|github|gitee`（默认 `all`）

交互模式：用 ↑/↓ 选择命令与版本/host，Enter 确认，Esc 退出。有实际动作的结果用颜色区分（成功绿 / 警告黄 / 失败红 / 信息青）。

## 远端约定

| Host | Git remote | 仓库 |
|---|---|---|
| GitHub | `gh` | `XiaoHongYong/MusicPlayer` |
| Gitee | `origin` | `xiaohongyong/Mp3Player` |

## 鉴权

- **GitHub**：本机已 `gh auth login`，对目标仓库有写权限。
- **Gitee**：环境变量 `GITEE_ACCESS_TOKEN`（或 `GITEE_TOKEN`），对目标仓库有写权限。

未配置对应凭据时，该 host 上的创建/上传/删除会失败。

## 与站点更新的关系

应用内检查更新读取的是网站上的 `music-player-update.json`（见 `MPlayerUI/VersionUpdate.cpp`），不是 GitHub/Gitee Release。

需要同步官网下载目录时，仍使用仓库旁的 `Release/push.sh`（scp）。本流程只负责 Git 托管上的 Release 资产。
