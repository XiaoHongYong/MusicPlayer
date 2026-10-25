#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MusicPlayer multi-host release helper (GitHub + Gitee).

Subcommands: prepare | upload | list | view | delete
No args → interactive menu (arrow keys).
"""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import re
import shutil
import ssl
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import uuid
from datetime import date
from pathlib import Path
from typing import List, Optional, Sequence, Tuple


REPO_ROOT = Path(__file__).resolve().parent.parent
VERSION_H_IN = REPO_ROOT / "version.h.in"
RELEASE_ROOT = REPO_ROOT.parent / "Release"

GITHUB_REMOTE = "gh"
GITEE_REMOTE = "origin"
GITEE_API = "https://gitee.com/api/v5"
DEFAULT_UPDATE_URL = "http://www.crintsoft.com"

ASSET_RULES = (
    ("MusicPlayer.dmg", "macOS", ".dmg"),
    ("MusicPlayer.exe", "windows", ".exe"),
)


class C:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    CYAN = "\033[36m"
    _enabled: Optional[bool] = None

    @classmethod
    def enabled(cls) -> bool:
        if cls._enabled is None:
            cls._enabled = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None
        return cls._enabled

    @classmethod
    def wrap(cls, color: str, text: str) -> str:
        if not cls.enabled():
            return text
        return f"{color}{text}{cls.RESET}"


def ok(msg: str) -> None:
    print(C.wrap(C.GREEN, f"✓ {msg}"))


def warn(msg: str) -> None:
    print(C.wrap(C.YELLOW, f"! {msg}"))


def err(msg: str) -> None:
    # 与 stdout 信息交错时保持顺序可读
    sys.stdout.flush()
    print(C.wrap(C.RED, f"✗ {msg}"), file=sys.stderr)
    sys.stderr.flush()


def info(msg: str) -> None:
    print(C.wrap(C.CYAN, f"· {msg}"))


def die(msg: str, code: int = 1) -> None:
    err(msg)
    sys.exit(code)


def run(
    cmd: Sequence[str],
    *,
    cwd: Optional[Path] = None,
    check: bool = True,
    capture: bool = True,
) -> subprocess.CompletedProcess:
    return subprocess.run(
        list(cmd),
        cwd=str(cwd or REPO_ROOT),
        check=check,
        text=True,
        capture_output=capture,
    )


def run_out(cmd: Sequence[str], **kwargs) -> str:
    return run(cmd, **kwargs).stdout.strip()


def git(*args: str, check: bool = True) -> str:
    return run_out(["git", *args], check=check)


def field_int(path: Path, name: str) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(rf"#define\s+{name}\s+(\d+)", text)
    if not m:
        die(f"Cannot find {name} in {path}")
    return int(m.group(1))


def current_commit_short() -> str:
    return git("log", "-1", "--format=%H")[:7]


def resolve_version(explicit: Optional[str] = None) -> Tuple[str, str, str]:
    """Return (version, tag, commit_short)."""
    if explicit:
        version = explicit.lstrip("v")
        commit = version.rsplit(".", 1)[-1] if version.count(".") >= 2 else current_commit_short()
        return version, f"v{version}", commit
    major = field_int(VERSION_H_IN, "MAJOR_VERSION")
    minor = field_int(VERSION_H_IN, "MINOR_VERSION")
    commit = current_commit_short()
    version = f"{major}.{minor}.{commit}"
    return version, f"v{version}", commit


def release_dir(version: str) -> Path:
    return RELEASE_ROOT / version


def parse_remote_owner_repo(remote: str) -> Tuple[str, str]:
    url = git("remote", "get-url", remote)
    m = re.search(r"[:/]([^/]+)/([^/]+?)(?:\.git)?$", url.strip())
    if not m:
        die(f"Cannot parse owner/repo from remote '{remote}': {url}")
    return m.group(1), m.group(2)


def hosts_from_arg(host: str) -> List[str]:
    host = (host or "all").lower()
    if host == "all":
        return ["github", "gitee"]
    if host in ("github", "gitee"):
        return [host]
    die(f"Unknown host: {host} (use all|github|gitee)")
    return []


def previous_tag(current_tag: str) -> Optional[str]:
    tags = [t for t in git("tag", "--sort=-creatordate").splitlines() if t]
    for t in tags:
        if t != current_tag:
            return t
    return None


def notes_body(version: str) -> str:
    d = release_dir(version)
    en = d / "RELEASE_NOTES.en.md"
    zh = d / "RELEASE_NOTES.zh-CN.md"
    parts: List[str] = []
    if en.is_file():
        parts.append(en.read_text(encoding="utf-8").strip())
    if zh.is_file():
        zh_text = zh.read_text(encoding="utf-8").strip()
        if parts:
            parts.append("\n---\n\n## 中文\n\n" + zh_text)
        else:
            parts.append(zh_text)
    if not parts:
        parts.append(f"MusicPlayer {version}")
    return "\n".join(parts).strip() + "\n"


def notes_text_from_md(path: Path) -> str:
    """从 RELEASE_NOTES.*.md 提取纯文本（去掉标题行，保留条目）。"""
    if not path.is_file():
        return ""
    lines: List[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        lines.append(s)
    return "\n".join(lines)


def write_music_player_update_json(
    version: str, *, url: str = DEFAULT_UPDATE_URL
) -> Path:
    """生成站点用的紧凑 JSON（无缩进），供应用内检查更新下载。

    格式:
    {"version":"1.1.abc","date":"2026-10-19","url":"http://...","notes":{"en":"...","zh-CN":"..."}}
    """
    d = release_dir(version)
    d.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": version,
        "date": date.today().isoformat(),
        "url": url or DEFAULT_UPDATE_URL,
        "notes": {
            "en": notes_text_from_md(d / "RELEASE_NOTES.en.md"),
            "zh-CN": notes_text_from_md(d / "RELEASE_NOTES.zh-CN.md"),
        },
    }
    out_path = d / "music-player-update.json"
    text = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    with out_path.open("w", encoding="utf-8", newline="\n") as fp:
        fp.write(text)
        fp.write("\n")
    return out_path


def discover_local_assets(version: str) -> List[Tuple[Path, str]]:
    d = release_dir(version)
    found: List[Tuple[Path, str]] = []
    for local_name, platform, ext in ASSET_RULES:
        src = d / local_name
        if src.is_file():
            upload_name = f"MusicPlayer-{version}-{platform}{ext}"
            found.append((src, upload_name))
        renamed = d / f"MusicPlayer-{version}-{platform}{ext}"
        if renamed.is_file() and (not src.is_file() or renamed.resolve() != src.resolve()):
            found.append((renamed, renamed.name))
    seen = set()
    unique: List[Tuple[Path, str]] = []
    for path, name in found:
        if name in seen:
            continue
        seen.add(name)
        unique.append((path, name))
    return unique


def _read_key() -> str:
    if os.name == "nt":
        import msvcrt

        ch = msvcrt.getwch()
        if ch in ("\x00", "\xe0"):
            ch2 = msvcrt.getwch()
            return {"H": "up", "P": "down", "K": "left", "M": "right"}.get(ch2, ch2)
        if ch == "\r":
            return "enter"
        if ch == "\x1b":
            return "esc"
        return ch

    import termios
    import tty

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == "\x1b":
            ch2 = sys.stdin.read(1)
            if ch2 == "[":
                ch3 = sys.stdin.read(1)
                return {"A": "up", "B": "down", "C": "right", "D": "left"}.get(ch3, "esc")
            return "esc"
        if ch in ("\r", "\n"):
            return "enter"
        if ch == "\x03":
            raise KeyboardInterrupt
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def select_menu(title: str, options: Sequence[str], *, default: int = 0) -> int:
    if not sys.stdin.isatty():
        print(title)
        for i, opt in enumerate(options):
            print(f"  {i + 1}. {opt}")
        while True:
            raw = input("Select number: ").strip()
            if raw.isdigit() and 1 <= int(raw) <= len(options):
                return int(raw) - 1
            warn("Invalid selection")

    idx = max(0, min(default, len(options) - 1))
    while True:
        sys.stdout.write("\033[2J\033[H")
        print(C.wrap(C.BOLD, title))
        print(C.wrap(C.DIM, "↑/↓ select · Enter confirm · Esc cancel\n"))
        for i, opt in enumerate(options):
            if i == idx:
                print(C.wrap(C.CYAN + C.BOLD, f" › {opt}"))
            else:
                print(f"   {opt}")
        sys.stdout.flush()
        key = _read_key()
        if key == "up":
            idx = (idx - 1) % len(options)
        elif key == "down":
            idx = (idx + 1) % len(options)
        elif key == "enter":
            sys.stdout.write("\033[2J\033[H")
            return idx
        elif key in ("esc", "q"):
            sys.stdout.write("\033[2J\033[H")
            raise SystemExit(0)


def prompt_text(label: str, default: str = "") -> str:
    suffix = f" [{default}]" if default else ""
    raw = input(C.wrap(C.CYAN, f"{label}{suffix}: ")).strip()
    return raw or default


def confirm(label: str, *, default: bool = False) -> bool:
    hint = "Y/n" if default else "y/N"
    raw = input(C.wrap(C.YELLOW, f"{label} ({hint}): ")).strip().lower()
    if not raw:
        return default
    return raw in ("y", "yes")


def gh_repo_slug() -> str:
    owner, repo = parse_remote_owner_repo(GITHUB_REMOTE)
    return f"{owner}/{repo}"


def ensure_gh() -> None:
    if shutil.which("gh") is None:
        raise FileNotFoundError(
            "GitHub CLI 'gh' not found in PATH. Install: https://cli.github.com/ then run: gh auth login"
        )


def _gh_auth_hint(stderr: str = "", stdout: str = "") -> Optional[str]:
    text = f"{stderr}\n{stdout}".lower()
    if any(
        s in text
        for s in (
            "not logged into",
            "gh auth login",
            "gh_token",
            "authentication required",
            "to get started with github cli",
        )
    ):
        return (
            "GitHub CLI 未登录。请先执行: gh auth login"
            "（或设置环境变量 GH_TOKEN）"
        )
    return None


def gh(*args: str, check: bool = True) -> str:
    ensure_gh()
    try:
        return run_out(["gh", *args], check=check)
    except subprocess.CalledProcessError as e:
        hint = _gh_auth_hint(e.stderr or "", e.stdout or "")
        if hint:
            raise RuntimeError(hint) from e
        detail = (e.stderr or e.stdout or "").strip()
        raise RuntimeError(detail or f"gh {' '.join(args)} failed (exit {e.returncode})") from e


def ssl_context() -> ssl.SSLContext:
    """macOS 官方 Python 常缺系统 CA；优先用 certifi。"""
    try:
        import certifi

        return ssl.create_default_context(cafile=certifi.where())
    except Exception:
        return ssl.create_default_context()


def github_release_exists(tag: str) -> bool:
    ensure_gh()
    cp = run(["gh", "release", "view", tag, "--repo", gh_repo_slug()], check=False)
    return cp.returncode == 0


def github_create_release(tag: str, version: str, body: str) -> None:
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".md", delete=False) as fp:
        fp.write(body)
        notes_path = fp.name
    try:
        gh(
            "release",
            "create",
            tag,
            "--repo",
            gh_repo_slug(),
            "--title",
            f"MusicPlayer {version}",
            "--notes-file",
            notes_path,
            "--target",
            git("rev-parse", "HEAD"),
        )
    finally:
        os.unlink(notes_path)


def github_upload_assets(tag: str, assets: Sequence[Tuple[Path, str]]) -> None:
    if not assets:
        return
    with tempfile.TemporaryDirectory(prefix="mp-release-") as tmp:
        paths: List[str] = []
        for src, name in assets:
            dest = Path(tmp) / name
            shutil.copy2(src, dest)
            paths.append(str(dest))
        gh("release", "upload", tag, *paths, "--repo", gh_repo_slug(), "--clobber")


def github_view(tag: Optional[str] = None) -> None:
    if tag:
        print(gh("release", "view", tag, "--repo", gh_repo_slug()))
    else:
        print(gh("release", "list", "--repo", gh_repo_slug(), "--limit", "20"))


def github_delete(tag: str) -> None:
    gh("release", "delete", tag, "--repo", gh_repo_slug(), "--yes")


def gitee_token() -> str:
    token = os.environ.get("GITEE_ACCESS_TOKEN") or os.environ.get("GITEE_TOKEN") or ""
    if not token:
        raise RuntimeError(
            "GITEE_ACCESS_TOKEN (or GITEE_TOKEN) is not set"
        )
    return token


def gitee_owner_repo() -> Tuple[str, str]:
    return parse_remote_owner_repo(GITEE_REMOTE)


def gitee_request(
    method: str,
    path: str,
    *,
    data: Optional[dict] = None,
    form_fields: Optional[List[Tuple[str, object]]] = None,
) -> object:
    token = gitee_token()
    owner, repo = gitee_owner_repo()
    url = f"{GITEE_API}/repos/{owner}/{repo}{path}"
    headers = {"User-Agent": "MusicPlayer-release.py"}

    if form_fields is not None:
        boundary = f"----mp-release-{uuid.uuid4().hex}"
        body = b""
        for key, value in form_fields:
            if isinstance(value, tuple):
                filename, file_bytes, content_type = value
                body += (
                    f"--{boundary}\r\n"
                    f'Content-Disposition: form-data; name="{key}"; filename="{filename}"\r\n'
                    f"Content-Type: {content_type}\r\n\r\n"
                ).encode("utf-8") + file_bytes + b"\r\n"
            else:
                body += (
                    f"--{boundary}\r\n"
                    f'Content-Disposition: form-data; name="{key}"\r\n\r\n'
                    f"{value}\r\n"
                ).encode("utf-8")
        body += f"--{boundary}--\r\n".encode("utf-8")
        headers["Content-Type"] = f"multipart/form-data; boundary={boundary}"
        req = urllib.request.Request(url, data=body, method=method, headers=headers)
    elif data is not None:
        payload = dict(data)
        payload["access_token"] = token
        encoded = urllib.parse.urlencode(payload).encode("utf-8")
        headers["Content-Type"] = "application/x-www-form-urlencoded"
        req = urllib.request.Request(url, data=encoded, method=method, headers=headers)
    else:
        sep = "&" if "?" in url else "?"
        url = f"{url}{sep}access_token={urllib.parse.quote(token)}"
        req = urllib.request.Request(url, method=method, headers=headers)

    try:
        with urllib.request.urlopen(req, timeout=300, context=ssl_context()) as resp:
            raw = resp.read().decode("utf-8")
            if not raw:
                return None
            return json.loads(raw)
    except urllib.error.HTTPError as e:
        detail = e.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Gitee API {method} {path} → HTTP {e.code}: {detail}") from e
    except urllib.error.URLError as e:
        reason = str(e.reason if getattr(e, "reason", None) else e)
        if "CERTIFICATE_VERIFY_FAILED" in reason or "SSL" in reason:
            raise RuntimeError(
                f"Gitee SSL 校验失败: {reason}. "
                "请安装/修复证书: python3 -m pip install --upgrade certifi"
                "，或运行 Python 安装目录下的 Install Certificates.command"
            ) from e
        raise RuntimeError(f"Gitee 网络错误: {reason}") from e


def gitee_list_releases() -> List[dict]:
    result = gitee_request("GET", "/releases?per_page=50")
    return result if isinstance(result, list) else []


def gitee_find_release(tag: str) -> Optional[dict]:
    for rel in gitee_list_releases():
        if rel.get("tag_name") == tag:
            return rel
    return None


def gitee_create_release(tag: str, version: str, body: str) -> dict:
    target = git("rev-parse", "HEAD")
    result = gitee_request(
        "POST",
        "/releases",
        data={
            "tag_name": tag,
            "name": f"MusicPlayer {version}",
            "body": body,
            "target_commitish": target,
        },
    )
    if not isinstance(result, dict) or "id" not in result:
        raise RuntimeError(f"Unexpected Gitee create response: {result}")
    return result


def gitee_attach_files(release_id: int) -> List[dict]:
    result = gitee_request("GET", f"/releases/{release_id}/attach_files")
    return result if isinstance(result, list) else []


def gitee_delete_attach(release_id: int, attach_id: int) -> None:
    gitee_request("DELETE", f"/releases/{release_id}/attach_files/{attach_id}")


def gitee_upload_assets(tag: str, assets: Sequence[Tuple[Path, str]]) -> None:
    rel = gitee_find_release(tag)
    if not rel:
        raise RuntimeError(f"Gitee release {tag} not found")
    release_id = int(rel["id"])
    existing = {a.get("name") or a.get("filename"): a for a in gitee_attach_files(release_id)}
    for src, name in assets:
        if name in existing:
            attach_id = existing[name].get("id")
            if attach_id is not None:
                info(f"Gitee: replace existing asset {name}")
                gitee_delete_attach(release_id, int(attach_id))
        file_bytes = src.read_bytes()
        content_type = mimetypes.guess_type(name)[0] or "application/octet-stream"
        gitee_request(
            "POST",
            f"/releases/{release_id}/attach_files",
            form_fields=[
                ("access_token", gitee_token()),
                ("file", (name, file_bytes, content_type)),
            ],
        )
        ok(f"Gitee uploaded {name}")


def gitee_delete_release(tag: str) -> None:
    rel = gitee_find_release(tag)
    if not rel:
        warn(f"Gitee release {tag} not found")
        return
    gitee_request("DELETE", f"/releases/{rel['id']}")


def ensure_tag_and_push(version: str, tag: str, commit: str, hosts: Sequence[str]) -> None:
    """本地无 tag 则创建 annotated tag，并推送到所选 host 对应 remote。"""
    existing = run(["git", "rev-parse", "-q", "--verify", f"refs/tags/{tag}"], check=False)
    if existing.returncode == 0:
        tagged = git("rev-list", "-n", "1", tag)
        if tagged[:7] != commit:
            die(f"Tag {tag} already points to {tagged[:7]}, expected {commit}")
        warn(f"Tag {tag} already exists (commit {tagged[:7]})")
    else:
        target_cp = run(
            ["git", "rev-parse", "--verify", f"{commit}^{{commit}}"], check=False
        )
        if target_cp.returncode == 0:
            target = target_cp.stdout.strip()
        elif current_commit_short() == commit:
            target = "HEAD"
        else:
            die(f"Cannot create tag {tag}: commit {commit} not found in local repo")
        git("tag", "-a", tag, "-m", f"MusicPlayer {version}", target)
        ok(f"Created local tag {tag} → {commit}")

    for host in hosts:
        remote = GITHUB_REMOTE if host == "github" else GITEE_REMOTE
        # 已在远端则 push 会报 already exists；用 --force-with-lease 太危险，改为探测后跳过
        remote_has = run(
            ["git", "ls-remote", "--tags", remote, f"refs/tags/{tag}"], check=False
        )
        remote_lines = [
            ln for ln in (remote_has.stdout or "").splitlines() if ln.strip()
        ]
        if remote_lines:
            remote_sha = remote_lines[0].split()[0]
            local_sha = git("rev-list", "-n", "1", tag)
            if remote_sha[:7] != local_sha[:7] and remote_sha != local_sha:
                die(
                    f"Remote {remote} already has {tag} at {remote_sha[:7]}, "
                    f"local is {local_sha[:7]}"
                )
            info(f"Tag {tag} already on {remote} ({host}), skip push")
            continue
        info(f"Push tag {tag} → {remote}")
        run(["git", "push", remote, tag], check=True, capture=False)
        ok(f"Pushed {tag} to {remote} ({host})")


def cmd_prepare(args: argparse.Namespace) -> None:
    version, tag, commit = resolve_version(getattr(args, "version", None))
    d = release_dir(version)
    d.mkdir(parents=True, exist_ok=True)
    ok(f"Release directory: {d}")

    hosts = hosts_from_arg(args.host)
    ensure_tag_and_push(version, tag, commit, hosts)

    prev = previous_tag(tag)
    range_hint = f"{prev}..HEAD" if prev else "HEAD~30..HEAD"
    info(f"Suggested git log range for notes: {range_hint}")
    log = run(["git", "log", range_hint, "--oneline"], check=False)
    print(log.stdout.strip() or "(no commits)")

    en = d / "RELEASE_NOTES.en.md"
    zh = d / "RELEASE_NOTES.zh-CN.md"
    if not en.exists():
        en.write_text(
            f"# MusicPlayer {version}\n\n"
            f"<!-- Fill release notes (English). Range: {range_hint} -->\n\n"
            f"## Changes\n\n- \n",
            encoding="utf-8",
        )
        ok(f"Stub created: {en}")
    else:
        info(f"Keep existing: {en}")
    if not zh.exists():
        zh.write_text(
            f"# MusicPlayer {version}\n\n"
            f"<!-- 填写中文更新说明。范围: {range_hint} -->\n\n"
            f"## 更新内容\n\n- \n",
            encoding="utf-8",
        )
        ok(f"Stub created: {zh}")
    else:
        info(f"Keep existing: {zh}")

    print()
    ok(f"prepare done · VERSION={version} TAG={tag}")
    warn("Edit RELEASE_NOTES.*.md, then: write-update-json → build → upload")


def cmd_write_update_json(args: argparse.Namespace) -> None:
    version, _, _ = resolve_version(getattr(args, "version", None))
    url = getattr(args, "url", None) or DEFAULT_UPDATE_URL
    out = write_music_player_update_json(version, url=url)
    ok(f"Wrote {out}")


def cmd_upload(args: argparse.Namespace) -> None:
    version, tag, commit = resolve_version(getattr(args, "version", None))
    d = release_dir(version)
    if not d.is_dir():
        die(f"Release directory not found: {d} (run prepare / build.sh first)")

    # 确认 notes 后若有改动，上传前再写一次，保证站点 JSON 与 notes 一致
    out = write_music_player_update_json(version)
    info(f"Refreshed {out.name}")

    assets = discover_local_assets(version)
    if not assets:
        die(
            f"No packages in {d}. Expected MusicPlayer.dmg and/or MusicPlayer.exe "
            f"(from ./build.sh -p)."
        )

    body = notes_body(version)
    hosts = hosts_from_arg(args.host)

    # 发布前确保 tag 存在并已推到所选远端（未 prepare 过也能 upload）
    ensure_tag_and_push(version, tag, commit, hosts)

    for host in hosts:
        info(f"=== {host} · {tag} ===")
        try:
            if host == "github":
                if not github_release_exists(tag):
                    info("Creating GitHub release…")
                    github_create_release(tag, version, body)
                    ok(f"GitHub release {tag} created")
                else:
                    info(f"GitHub release {tag} already exists")
                github_upload_assets(tag, assets)
                for _, name in assets:
                    ok(f"GitHub uploaded {name}")
            else:
                gitee_token()
                rel = gitee_find_release(tag)
                if not rel:
                    info("Creating Gitee release…")
                    gitee_create_release(tag, version, body)
                    ok(f"Gitee release {tag} created")
                else:
                    info(f"Gitee release {tag} already exists")
                gitee_upload_assets(tag, assets)
        except SystemExit:
            raise
        except Exception as e:
            err(f"{host}: {e}")
            raise SystemExit(1) from e

    ok(f"upload done · {version} · {len(assets)} asset(s) → {', '.join(hosts)}")


def cmd_list(args: argparse.Namespace) -> None:
    info(f"Local releases under {RELEASE_ROOT}")
    if RELEASE_ROOT.is_dir():
        versions = sorted(
            [p.name for p in RELEASE_ROOT.iterdir() if p.is_dir() and not p.name.startswith(".")],
            reverse=True,
        )
        for v in versions[:30]:
            assets = discover_local_assets(v)
            names = ", ".join(n for _, n in assets) or "(no packages)"
            print(f"  {C.wrap(C.BOLD, v)}  {names}")
    else:
        warn("No local Release directory")

    hosts = hosts_from_arg(args.host)
    if "github" in hosts:
        print()
        info("GitHub releases")
        try:
            github_view(None)
        except Exception as e:
            err(f"GitHub: {e}")
    if "gitee" in hosts:
        print()
        info("Gitee releases")
        try:
            for rel in gitee_list_releases()[:20]:
                tag = rel.get("tag_name", "?")
                name = rel.get("name", "")
                print(f"  {tag}\t{name}")
        except Exception as e:
            err(f"Gitee: {e}")


def cmd_view(args: argparse.Namespace) -> None:
    version, tag, _ = resolve_version(getattr(args, "version", None))
    hosts = hosts_from_arg(args.host)
    d = release_dir(version)
    info(f"Local: {d}")
    if d.is_dir():
        for p in sorted(d.iterdir()):
            if p.is_file():
                print(f"  {p.name}\t{p.stat().st_size} bytes")
    else:
        warn("Local directory missing")

    for host in hosts:
        print()
        info(f"{host} · {tag}")
        try:
            if host == "github":
                if github_release_exists(tag):
                    github_view(tag)
                else:
                    warn("GitHub release not found")
            else:
                rel = gitee_find_release(tag)
                if not rel:
                    warn("Gitee release not found")
                else:
                    print(json.dumps(rel, ensure_ascii=False, indent=2))
                    files = gitee_attach_files(int(rel["id"]))
                    for f in files:
                        print(f"  asset: {f.get('name') or f.get('filename')}")
        except Exception as e:
            err(f"{host}: {e}")


def cmd_delete(args: argparse.Namespace) -> None:
    version, tag, _ = resolve_version(getattr(args, "version", None))
    hosts = hosts_from_arg(args.host)
    delete_tag = bool(getattr(args, "delete_tag", False))

    if not getattr(args, "yes", False) and not confirm(
        f"Delete remote release(s) for {tag} on {hosts}?", default=False
    ):
        warn("Cancelled")
        return

    for host in hosts:
        try:
            if host == "github":
                if github_release_exists(tag):
                    github_delete(tag)
                    ok(f"Deleted GitHub release {tag}")
                else:
                    warn(f"GitHub release {tag} not found")
            else:
                gitee_delete_release(tag)
                ok(f"Deleted Gitee release {tag}")
        except Exception as e:
            err(f"{host}: {e}")
            raise SystemExit(1) from e

    if delete_tag:
        for remote, host in ((GITHUB_REMOTE, "github"), (GITEE_REMOTE, "gitee")):
            if host not in hosts:
                continue
            run(["git", "push", remote, f":refs/tags/{tag}"], check=False, capture=False)
            info(f"Tried delete remote tag on {remote}")
        local = run(["git", "tag", "-d", tag], check=False)
        if local.returncode == 0:
            ok(f"Deleted local tag {tag}")
        else:
            warn(f"Local tag {tag} not deleted (may not exist)")


def list_local_versions() -> List[str]:
    if not RELEASE_ROOT.is_dir():
        return []
    return sorted(
        [p.name for p in RELEASE_ROOT.iterdir() if p.is_dir() and re.match(r"^\d+\.\d+\.", p.name)],
        reverse=True,
    )


def interactive() -> None:
    actions = [
        ("prepare", "prepare — 创建版本目录并打 tag"),
        ("write-update-json", "write-update-json — 生成 music-player-update.json"),
        ("upload", "upload — 上传本地安装包到 GitHub/Gitee"),
        ("list", "list — 查看本地与远端 Release"),
        ("view", "view — 查看指定版本详情"),
        ("delete", "delete — 删除远端 Release"),
        ("quit", "quit — 退出"),
    ]
    idx = select_menu("MusicPlayer release", [label for _, label in actions])
    action = actions[idx][0]
    if action == "quit":
        return

    host_options = ["all", "github", "gitee"]
    host = host_options[select_menu("选择发布目标 host", host_options)]

    ns = argparse.Namespace(
        host=host, version=None, yes=False, delete_tag=False, url=DEFAULT_UPDATE_URL
    )

    if action == "prepare":
        cmd_prepare(ns)
        return
    if action == "list":
        cmd_list(ns)
        return

    versions = list_local_versions()
    cur_v, _, _ = resolve_version(None)
    choices = [f"{cur_v} (current HEAD)"] + [v for v in versions if v != cur_v] + ["手动输入…"]
    vi = select_menu("选择版本", choices)
    if choices[vi].startswith("手动"):
        ns.version = prompt_text("VERSION", cur_v)
    elif vi == 0:
        ns.version = cur_v
    else:
        ns.version = choices[vi].split()[0]

    if action == "write-update-json":
        cmd_write_update_json(ns)
    elif action == "upload":
        cmd_upload(ns)
    elif action == "view":
        cmd_view(ns)
    elif action == "delete":
        ns.delete_tag = confirm("同时删除 git tag？", default=False)
        cmd_delete(ns)


def build_parser() -> argparse.ArgumentParser:
    host_parent = argparse.ArgumentParser(add_help=False)
    host_parent.add_argument(
        "--host",
        default="all",
        choices=["all", "github", "gitee"],
        help="发布目标（默认 all）",
    )

    p = argparse.ArgumentParser(
        description="MusicPlayer release helper (GitHub + Gitee)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="无子命令时进入交互模式。详见 docs/release.md",
        parents=[host_parent],
    )
    sub = p.add_subparsers(dest="command")

    sp = sub.add_parser("prepare", help="创建版本目录并打/推 tag", parents=[host_parent])
    sp.add_argument("version", nargs="?", help="可选，默认从 version.h.in + HEAD 计算")
    sp.set_defaults(func=cmd_prepare)

    sp = sub.add_parser(
        "write-update-json",
        help="根据 RELEASE_NOTES 生成 music-player-update.json",
        parents=[host_parent],
    )
    sp.add_argument("version", nargs="?", help="可选版本号")
    sp.add_argument(
        "--url",
        default=DEFAULT_UPDATE_URL,
        help=f"检查更新打开的页面（默认 {DEFAULT_UPDATE_URL}）",
    )
    sp.set_defaults(func=cmd_write_update_json)

    sp = sub.add_parser("upload", help="上传本地包；Release 不存在则创建", parents=[host_parent])
    sp.add_argument("version", nargs="?", help="可选版本号")
    sp.set_defaults(func=cmd_upload)

    sp = sub.add_parser("list", help="列出本地与远端 Release", parents=[host_parent])
    sp.set_defaults(func=cmd_list)

    sp = sub.add_parser("view", help="查看指定版本", parents=[host_parent])
    sp.add_argument("version", nargs="?", help="可选版本号")
    sp.set_defaults(func=cmd_view)

    sp = sub.add_parser("delete", help="删除远端 Release", parents=[host_parent])
    sp.add_argument("version", nargs="?", help="可选版本号")
    sp.add_argument("--yes", "-y", action="store_true", help="跳过确认")
    sp.add_argument("--delete-tag", action="store_true", help="同时删除远端/本地 tag")
    sp.set_defaults(func=cmd_delete)

    return p


def main(argv: Optional[Sequence[str]] = None) -> None:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        try:
            interactive()
        except KeyboardInterrupt:
            print()
            warn("Interrupted")
        return

    parser = build_parser()
    args = parser.parse_args(argv)
    if not getattr(args, "command", None):
        interactive()
        return
    args.func(args)


if __name__ == "__main__":
    main()
