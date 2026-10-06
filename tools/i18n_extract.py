#!/usr/bin/env python3
"""Extract translatable strings and generate shared locale artifacts.

C++ macros: _TL / _TLT / _TLM (see Utils/LocalizeTool.h)
Web calls:  t("...") / t('...')  (first argument only)
Skin menus: Skins-Design/skins/assets/menu.json item titles
Skin XML:  Text / ToolTip / PlaceHolder attributes

Source of truth for translations: i18n/locales/<locale>.json
Generated:
  i18n/lang/<locale>.ini                     — C++ language packs
  LocalServer/www/src/i18n/messages.generated.ts
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import OrderedDict
from html import unescape as html_unescape
from pathlib import Path
from typing import Iterable, Iterator

ROOT = Path(__file__).resolve().parents[1]
I18N_DIR = ROOT / "i18n"
CATALOG_PATH = I18N_DIR / "catalog.json"
LOCALES_DIR = I18N_DIR / "locales"
LANG_DIR = I18N_DIR / "lang"
WEB_MESSAGES = ROOT / "LocalServer" / "www" / "src" / "i18n" / "messages.generated.ts"

CPP_EXTS = {".cpp", ".h", ".hpp", ".c", ".cc", ".mm", ".m"}
WEB_EXTS = {".ts", ".tsx", ".js", ".jsx"}
SKIP_DIR_NAMES = {
    "third-parties",
    "build",
    "node_modules",
    "dist",
    ".git",
    "googletest",
}

CPP_MACROS = ("_TLT", "_TLM", "_TL")  # longer first
WEB_T_NAME = "t"

MENU_JSON = ROOT / "Skins-Design" / "skins" / "assets" / "menu.json"
SKIN_ROOT = ROOT / "Skins-Design" / "skins"
SKIN_ATTR_RE = re.compile(r'\b(Text|ToolTip|PlaceHolder)="([^"]*)"', re.IGNORECASE)
SKIP_SKIN_VALUE_RE = re.compile(
    r"^(TRUE|FALSE|#?[0-9A-Fa-f]{3,8}|[\d.,\s/%+-]*)$",
    re.IGNORECASE,
)
MENU_SKIP_TITLES = {"separator"}
CPP_OCC_SOURCES = {"cpp", "menu", "skin"}

LOCALE_META = {
    "zh-CN": {
        "Language": "简体中文",
        "LanguageCode": "Simplified Chinese",
        "LanguageCodeFull": "Chinese (PRC)",
        "ini_name": "zh-CN.ini",
    },
}


class Occurrence:
    __slots__ = ("key", "source", "file", "line")

    def __init__(self, key: str, source: str, file: str, line: int):
        self.key = key
        self.source = source  # "cpp" | "web"
        self.file = file
        self.line = line


def unescape_c_string(body: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ch != "\\":
            out.append(ch)
            i += 1
            continue
        if i + 1 >= len(body):
            out.append("\\")
            break
        nxt = body[i + 1]
        mapping = {
            "n": "\n",
            "r": "\r",
            "t": "\t",
            "0": "\0",
            '"': '"',
            "'": "'",
            "\\": "\\",
        }
        if nxt in mapping:
            out.append(mapping[nxt])
            i += 2
            continue
        if nxt == "x" and i + 3 < len(body):
            hexpart = body[i + 2 : i + 4]
            if re.fullmatch(r"[0-9a-fA-F]{2}", hexpart):
                out.append(chr(int(hexpart, 16)))
                i += 4
                continue
        out.append(nxt)
        i += 2
    return "".join(out)


def unescape_js_string(body: str, quote: str) -> str:
    if quote == "`":
        return body.replace("\\`", "`").replace("\\n", "\n").replace("\\t", "\t")
    return unescape_c_string(body)


def _skip_ws_and_comments(text: str, i: int) -> int:
    n = len(text)
    while i < n:
        ch = text[i]
        if ch in " \t\r\n":
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            i += 2
            while i < n and text[i] not in "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            if end < 0:
                return n
            i = end + 2
            continue
        break
    return i


def _parse_c_string_literal(text: str, i: int) -> tuple[str, int] | None:
    n = len(text)
    if i < n and text.startswith(('u8"', 'u"', 'U"', 'L"'), i):
        i = text.find('"', i)
    if i >= n or text[i] != '"':
        return None
    i += 1
    body: list[str] = []
    while i < n:
        ch = text[i]
        if ch == "\\":
            if i + 1 < n:
                body.append(ch)
                body.append(text[i + 1])
                i += 2
                continue
            return None
        if ch == '"':
            return unescape_c_string("".join(body)), i + 1
        if ch == "\n":
            return None
        body.append(ch)
        i += 1
    return None


def parse_concatenated_c_strings(text: str, i: int) -> tuple[str, int] | None:
    first = _parse_c_string_literal(text, i)
    if first is None:
        return None
    parts = [first[0]]
    i = first[1]
    while True:
        j = _skip_ws_and_comments(text, i)
        nxt = _parse_c_string_literal(text, j)
        if nxt is None:
            return "".join(parts), i
        parts.append(nxt[0])
        i = nxt[1]


def _is_ident_char(ch: str) -> bool:
    return ch.isalnum() or ch == "_"


def extract_cpp_macros(text: str, relpath: str) -> list[Occurrence]:
    found: list[Occurrence] = []
    n = len(text)
    i = 0
    in_line = False
    in_block = False
    in_str = False
    str_ch = ""
    while i < n:
        ch = text[i]
        if in_line:
            if ch == "\n":
                in_line = False
            i += 1
            continue
        if in_block:
            if ch == "*" and i + 1 < n and text[i + 1] == "/":
                in_block = False
                i += 2
            else:
                i += 1
            continue
        if in_str:
            if ch == "\\":
                i += 2
                continue
            if ch == str_ch:
                in_str = False
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            in_line = True
            i += 2
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            in_block = True
            i += 2
            continue
        if ch in "\"'":
            in_str = True
            str_ch = ch
            i += 1
            continue

        hit = None
        for name in CPP_MACROS:
            if text.startswith(name, i) and (i == 0 or not _is_ident_char(text[i - 1])):
                after = i + len(name)
                if after < n and _is_ident_char(text[after]):
                    continue
                hit = name
                break
        if not hit:
            i += 1
            continue

        j = _skip_ws_and_comments(text, i + len(hit))
        if j >= n or text[j] != "(":
            i += 1
            continue
        j = _skip_ws_and_comments(text, j + 1)
        parsed = parse_concatenated_c_strings(text, j)
        if parsed is None:
            i += 1
            continue
        key, end = parsed
        k = _skip_ws_and_comments(text, end)
        if k < n and text[k] == ")":
            line = text.count("\n", 0, i) + 1
            if key:
                found.append(Occurrence(key, "cpp", relpath, line))
            i = k + 1
            continue
        i += 1
    return found


_JS_STRING = re.compile(
    r"""t\s*\(\s*(?P<q>['"`])(?P<body>(?:\\.|(?!(?P=q)|\$\{).)*)(?P=q)""",
    re.DOTALL,
)


def extract_web_t_calls(text: str, relpath: str) -> list[Occurrence]:
    found: list[Occurrence] = []
    for m in _JS_STRING.finditer(text):
        # skip import { t } and function t(
        start = m.start()
        if start > 0 and _is_ident_char(text[start - 1]):
            continue
        prefix = text[max(0, start - 20) : start]
        if re.search(r"\bfunction\s+$", prefix) or re.search(r"\bexport\s+function\s+$", prefix):
            continue
        quote = m.group("q")
        body = m.group("body")
        if quote == "`" and "${" in m.group(0):
            continue
        key = unescape_js_string(body, quote)
        if not key:
            continue
        line = text.count("\n", 0, start) + 1
        found.append(Occurrence(key, "web", relpath, line))
    return found


def iter_source_files(root: Path, exts: set[str], extra_skip: Iterable[str] = ()) -> Iterator[Path]:
    skip = set(SKIP_DIR_NAMES)
    skip.update(extra_skip)
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in exts:
            continue
        if any(part in skip for part in path.parts):
            continue
        yield path


def _line_of(text: str, needle: str) -> int:
    idx = text.find(needle)
    if idx < 0:
        return 1
    return text.count("\n", 0, idx) + 1


def _walk_menu_items(node, acc: list[str]) -> None:
    if isinstance(node, list):
        if node and isinstance(node[0], str):
            title = node[0]
            if (
                title not in MENU_SKIP_TITLES
                and not title.startswith("INSERT_")
                and not title.isdigit()
            ):
                acc.append(title)
        for child in node:
            _walk_menu_items(child, acc)
    elif isinstance(node, dict):
        for child in node.values():
            _walk_menu_items(child, acc)


def extract_menu_json(path: Path | None = None) -> list[Occurrence]:
    menu_path = path or MENU_JSON
    if not menu_path.exists():
        return []
    text = menu_path.read_text(encoding="utf-8")
    data = json.loads(text)
    titles: list[str] = []
    _walk_menu_items(data, titles)
    try:
        rel = menu_path.relative_to(ROOT).as_posix()
    except ValueError:
        rel = menu_path.name
    found: list[Occurrence] = []
    seen: set[str] = set()
    for title in titles:
        if title in seen:
            continue
        seen.add(title)
        found.append(Occurrence(title, "menu", rel, _line_of(text, json.dumps(title, ensure_ascii=False))))
    return found


def is_skin_ui_string(value: str) -> bool:
    if not value or not value.strip():
        return False
    if SKIP_SKIN_VALUE_RE.match(value.strip()):
        return False
    if value.endswith((".png", ".jpg", ".bmp", ".xml", ".js")):
        return False
    return True


def extract_skin_xml(root: Path | None = None) -> list[Occurrence]:
    skin_root = root or SKIN_ROOT
    found: list[Occurrence] = []
    if not skin_root.exists():
        return found
    for path in sorted(skin_root.rglob("*.xml")):
        text = path.read_text(encoding="utf-8", errors="replace")
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            rel = path.name
        for m in SKIN_ATTR_RE.finditer(text):
            value = html_unescape(m.group(2))
            if not is_skin_ui_string(value):
                continue
            line = text.count("\n", 0, m.start()) + 1
            found.append(Occurrence(value, "skin", rel, line))
    return found


def collect_all() -> list[Occurrence]:
    occ: list[Occurrence] = []
    for path in iter_source_files(ROOT, CPP_EXTS):
        rel = path.relative_to(ROOT).as_posix()
        occ.extend(extract_cpp_macros(path.read_text(encoding="utf-8", errors="replace"), rel))
    web_root = ROOT / "LocalServer" / "www" / "src"
    for path in iter_source_files(web_root, WEB_EXTS):
        if path.name.endswith(".test.ts") or path.name.endswith(".test.tsx"):
            continue
        if path.name == "messages.generated.ts":
            continue
        rel = path.relative_to(ROOT).as_posix()
        occ.extend(extract_web_t_calls(path.read_text(encoding="utf-8", errors="replace"), rel))
    occ.extend(extract_menu_json())
    occ.extend(extract_skin_xml())
    return occ


def build_catalog(occ: list[Occurrence]) -> OrderedDict:
    catalog: OrderedDict[str, dict] = OrderedDict()
    for item in sorted(occ, key=lambda x: (x.key.lower(), x.source, x.file, x.line)):
        entry = catalog.get(item.key)
        if entry is None:
            entry = {"sources": []}
            catalog[item.key] = entry
        loc = f"{item.source}:{item.file}:{item.line}"
        if loc not in entry["sources"]:
            entry["sources"].append(loc)
    return catalog


def load_json(path: Path) -> dict:
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def dump_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def merge_locale(existing: dict, keys: Iterable[str]) -> tuple[dict, list[str]]:
    out = OrderedDict()
    missing: list[str] = []
    for key in keys:
        val = existing.get(key, "")
        if not isinstance(val, str):
            val = "" if val is None else str(val)
        out[key] = val
        if val.strip() == "":
            missing.append(key)
    return dict(out), missing


def ini_escape_value(s: str) -> str:
    return s.replace("\r\n", "\n").replace("\r", "\n")


def write_ini(path: Path, locale: str, translations: dict, cpp_keys: list[str]) -> None:
    meta = LOCALE_META[locale]
    lines = [
        "[Info]",
        f"Language={meta['Language']}",
        f"LanguageCode={meta['LanguageCode']}",
        f"LanguageCodeFull={meta['LanguageCodeFull']}",
        "",
        "[string]",
    ]
    skipped = 0
    for key in cpp_keys:
        val = translations.get(key, "")
        if not val:
            continue
        if "\n" in key or "\r" in key:
            skipped += 1
            continue
        val = ini_escape_value(val)
        if "\n" in val:
            val = val.replace("\n", " ")
        lines.append(f"{key}={val}")
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")
    if skipped:
        print(f"warning: skipped {skipped} C++ keys with newlines for {path.name}", file=sys.stderr)


def write_web_messages(path: Path, locales: dict[str, dict]) -> None:
    payload = json.dumps(locales, ensure_ascii=False, indent=2)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "/* eslint-disable */\n"
        "// Generated by tools/i18n_extract.py — do not edit.\n"
        f"export const messages: Record<string, Record<string, string>> = {payload};\n",
        encoding="utf-8",
    )


def run(write: bool) -> int:
    occ = collect_all()
    catalog = build_catalog(occ)
    keys = list(catalog.keys())
    cpp_keys = sorted(
        {item.key for item in occ if item.source in CPP_OCC_SOURCES},
        key=str.lower,
    )

    I18N_DIR.mkdir(parents=True, exist_ok=True)
    LOCALES_DIR.mkdir(parents=True, exist_ok=True)

    if write:
        dump_json(
            CATALOG_PATH,
            {
                "count": len(catalog),
                "strings": catalog,
            },
        )

    missing_report: dict[str, list[str]] = {}
    locale_maps: dict[str, dict] = {}
    for locale in LOCALE_META:
        loc_path = LOCALES_DIR / f"{locale}.json"
        existing = load_json(loc_path)
        merged, missing = merge_locale(existing, keys)
        locale_maps[locale] = merged
        missing_report[locale] = missing
        if write:
            dump_json(loc_path, merged)
            write_ini(LANG_DIR / LOCALE_META[locale]["ini_name"], locale, merged, cpp_keys)

    if write:
        write_web_messages(WEB_MESSAGES, locale_maps)

    print(f"extracted {len(occ)} occurrences, {len(keys)} unique keys")
    print(f"  cpp keys: {len(cpp_keys)}")
    print(f"  web keys: {len({item.key for item in occ if item.source == 'web'})}")
    print(f"  menu keys: {len({item.key for item in occ if item.source == 'menu'})}")
    print(f"  skin keys: {len({item.key for item in occ if item.source == 'skin'})}")
    for locale, missing in missing_report.items():
        print(f"  {locale} missing: {len(missing)}")
        for key in missing[:20]:
            print(f"    - {key}")
        if len(missing) > 20:
            print(f"    ... {len(missing) - 20} more")
    missing_any = any(len(v) > 0 for v in missing_report.values())
    if write:
        return 0
    return 0 if not missing_any else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="scan only; do not write files (exit 1 if translations missing)",
    )
    args = parser.parse_args(argv)
    return run(write=not args.check)


if __name__ == "__main__":
    raise SystemExit(main())
