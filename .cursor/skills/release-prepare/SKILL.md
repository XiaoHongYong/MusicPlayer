---
name: release-prepare
description: >-
  Prepare a MusicPlayer GitHub/Gitee release: create version dir, tag from
  version.h.in + commit, and draft bilingual release notes. Use when the user
  asks to prepare a release, 准备发布, 打 release, or generate release notes.
---

# Release Prepare

权威流程见 [`docs/release.md`](../../../docs/release.md)。本 skill 只覆盖 **第 1 步 prepare + 起草 notes**；确认与上传由用户后续完成。

## Task Progress

```
Task Progress:
- [ ] Prepare (tools/release.py prepare)
- [ ] Draft RELEASE_NOTES.en.md + RELEASE_NOTES.zh-CN.md
- [ ] Stop for human confirmation
```

## Steps

### 1. Preconditions

- Working tree should be clean enough to tag (warn if dirty; do not force-clean).
- Confirm the intended commit is checked out (`git log -1 --oneline`).
- Remotes: GitHub = `gh`, Gitee = `origin`.

### 2. Run prepare

```bash
python3 tools/release.py prepare
```

Script will:

- Read `MAJOR_VERSION` / `MINOR_VERSION` from `version.h.in`
- Set build = current commit short hash (7 chars)
- Create `../Release/{VERSION}/`
- Create annotated tag `v{VERSION}` if missing and push to `gh` + `origin`
- Create stub note files if missing

Capture printed `VERSION` / `TAG` / directory path.

Optional: `python3 tools/release.py prepare --host github` (or `gitee`) if only one host should receive the tag push.

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

### 4. Stop for human confirmation

Show the two note paths and **stop**. Tell the user:

1. Review / edit the notes.
2. On each platform at the same commit: `./build.sh`
3. Then: `python3 tools/release.py upload`

Do **not** run `upload` or `delete` in this skill unless the user explicitly asks after confirming notes.

## Out of scope

- Building installers (`build.sh`)
- Uploading assets (`release.py upload`)
- Website scp (`Release/push.sh`)
