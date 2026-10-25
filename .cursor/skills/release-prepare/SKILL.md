---
name: release-prepare
description: >-
  Prepare a MusicPlayer GitHub/Gitee release: create version dir, tag from
  version.h.in + commit, and draft bilingual release notes. Use when the user
  asks to prepare a release, 准备发布, 打 release, or generate release notes.
---

# Release Prepare

权威流程见 [`docs/release.md`](../../../docs/release.md)。本 skill 只覆盖 **prepare → 起草 notes → 生成 update json**；确认与上传由用户后续完成。

## Task Progress

```
Task Progress:
- [ ] Prepare (./release.sh prepare)
- [ ] Draft RELEASE_NOTES.en.md + RELEASE_NOTES.zh-CN.md
- [ ] Write music-player-update.json (./release.sh write-update-json)
- [ ] Stop for human confirmation
```

## Steps

### 1. Preconditions

- Working tree should be clean enough to tag (warn if dirty; do not force-clean).
- Confirm the intended commit is checked out (`git log -1 --oneline`).
- Remotes: GitHub = `gh`, Gitee = `origin`.

### 2. Run prepare

```bash
./release.sh prepare
```

Script will:

- Read `MAJOR_VERSION` / `MINOR_VERSION` from `version.h.in`
- Set build = current commit short hash (7 chars)
- Create `../Release/{VERSION}/`
- Create annotated tag `v{VERSION}` if missing and push to `gh` + `origin`
- Create stub note files if missing

Capture printed `VERSION` / `TAG` / directory path.

Optional: `./release.sh prepare --host github` (or `gitee`) if only one host should receive the tag push.

### 3. Draft release notes

Use the suggested git range from prepare output (previous tag → `HEAD`). If none:

```bash
git log -30 --oneline
```

Write user-facing notes (not raw commit dumps):

| File | Language |
|---|---|
| `../Release/{VERSION}/RELEASE_NOTES.en.md` | English |
| `../Release/{VERSION}/RELEASE_NOTES.zh-CN.md` | 中文 |

Rules:

- Same structure in both files (title + bullet changes).
- Concise; group related commits.
- Do not invent features not in the log.
- Keep product name `MusicPlayer` / `Music Center` as elsewhere.

Overwrite the stub placeholders created by `prepare`.

### 4. Write music-player-update.json

Notes 写好后立刻生成站点用检查更新文件（紧凑 JSON，含 `notes.en` / `notes.zh-CN`）：

```bash
./release.sh write-update-json
```

输出：`../Release/{VERSION}/music-player-update.json`。  
（`upload` 时会再写一次，以便吸收你确认阶段对 notes 的修改。）

### 5. Stop for human confirmation

Show the note paths、update json 路径，然后 **stop**。Tell the user:

1. Review / edit the notes（若改了 notes，可再跑一次 `./release.sh write-update-json`）。
2. On each platform at the same commit: `./build.sh`
3. Then: `./release.sh upload`（若本地尚无 tag，upload 会自动创建并推到所选 remote）

Do **not** run `upload` or `delete` in this skill unless the user explicitly asks after confirming notes.

## Out of scope

- Building installers (`build.sh`)
- Uploading assets (`release.py upload`)
- Website scp (`Release/push.sh`)
