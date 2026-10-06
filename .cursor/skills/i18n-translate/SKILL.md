---
name: i18n-translate
description: >-
  Extract, translate, and sync MusicPlayer i18n strings shared by C++ (_TL/_TLT/_TLM)
  and the LocalServer/www web console. Use when the user asks to translate UI copy,
  add a locale, update i18n, run string extraction, or fill missing translations.
---

# i18n Translate

English source strings are the **shared keys**. C++ and Web must use the same key to share a translation.

## Layout

| Path | Role |
|---|---|
| `i18n/catalog.json` | Extracted keys + source locations (generated) |
| `i18n/locales/<locale>.json` | Translations (source of truth) |
| `i18n/lang/<locale>.ini` | C++ language pack (generated) |
| `LocalServer/www/src/i18n/messages.generated.ts` | Web message tables (generated) |

Do not hand-edit generated files. Edit `i18n/locales/*.json` only.

## Workflow

```
Task Progress:
- [ ] Extract
- [ ] Translate missing keys
- [ ] Re-extract / generate
- [ ] Verify
```

**1. Extract**

```bash
python3 tools/i18n_extract.py
```

C++: `_TL("...")` / `_TLT("...")` / `_TLM("...")` string literals (concatenated literals included).
Web: `t("...")` / `t('...')` first argument. Dynamic `t(item.label)` is **not** extracted — use `t('Home')` literals (or `(t) => t('Home')`).
Skin: `Skins-Design/skins/assets/menu.json` item titles; XML `Text` / `ToolTip` / `PlaceHolder`. Write English in those attributes (the same keys `_TL` uses at runtime).

**2. Translate missing keys** in `i18n/locales/zh-CN.json` (and any new locale).

Rules:

- Same English key → **same** translation. Never fork wording for C++ vs Web.
- Keep existing translations unless the user asked to revise them.
- Preserve `{name}` / `{n}` placeholders and C++ `%s` / `%d` / `$Product$`.
- Preserve `&` accelerators (Win32). Prefer `复制(&C)` for `&Copy`.
- Empty value means “use English”. Do not invent keys that are not in the catalog.
- Tone: desktop music player, concise UI Chinese. Match nearby strings.
- Do not translate product name `Music Center` unless the user asks.

**3. Generate again**

```bash
python3 tools/i18n_extract.py
```

Exit 0 when nothing is missing. `--check` scans without writing.

**4. Verify**

```bash
python3 tools/test_i18n_extract.py
cd LocalServer/www && npm test -- src/i18n/index.test.ts
```

New UI copy: wrap with `_TL`/`_TLT`/`_TLM` (C++) or `t()` / `useT()` (Web), then extract + translate.

## Build packaging

`i18n/lang/*.ini` and `i18n/catalog.json` are gitignored. `./build.sh` runs `tools/i18n_extract.py` then copies `i18n/lang/` to `MusicPlayer.app/Contents/Resources/lang/`.

Desktop loads `getAppResourceFile("lang")`. Empty profile `Language` follows the OS (macOS preferred language / Windows LANGID / Linux `LANG`): `zh-Hans*`/`zh-CN` → `zh-CN.ini`. User picks a language in Preferences → that filename is saved.

## Add a locale

1. Add metadata in `tools/i18n_extract.py` `LOCALE_META`.
2. Run extract (creates empty `i18n/locales/<locale>.json`).
3. Fill translations.
4. Run extract again.
5. Web: add the locale id to `Locale` / `LOCALES` in `LocalServer/www/src/i18n/index.ts`.
