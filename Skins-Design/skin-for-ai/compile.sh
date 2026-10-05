#!/usr/bin/env bash
# 调用 Skin Compiler，按指定 skin.json 生成或更新 Skins-Design/skins/<Name>/ 资源。
#
# 用法:
#   ./compile.sh glass
#   ./compile.sh glass.skin.json
#   ./compile.sh neon --xml
#   ./compile.sh --all
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$ROOT/compiler"
SKINS_OUT="$(cd "$ROOT/.." && pwd)/skins"

usage() {
    cat <<'EOF'
用法: ./compile.sh <文件名> [选项]
      ./compile.sh --all [选项]
      ./compile.sh --list

根据 skin-for-ai 下的 <name>.skin.json 调用 Skin Compiler，
输出到 Skins-Design/skins/<meta.name>/。

文件名可以是 stem（glass）、完整文件名（glass.skin.json）或路径。

默认行为:
  - 目标皮肤目录已有 Styles.xml → 只更新 PNG（--png-only --force），保留手写 XML
  - 新皮肤（尚无 Styles.xml）    → 写出 PNG + XML

选项:
  --png-only   强制只写 PNG
  --xml        同时写出/覆盖 Styles.xml、main.xml、theme.xml
  --no-force   使用编译缓存，不强制全量重渲（默认 --force）
  --all        编译本目录全部 *.skin.json
  --list       列出可编译的源文件
  -h, --help
EOF
}

die() {
    echo "错误: $*" >&2
    exit 1
}

list_sources() {
    local f
    for f in "$ROOT"/*.skin.json; do
        [[ -f "$f" ]] || continue
        basename "$f"
    done
}

resolve_source() {
    local spec="$1"
    local candidate f stem want

    if [[ -f "$spec" ]]; then
        (cd "$(dirname "$spec")" && echo "$(pwd)/$(basename "$spec")")
        return 0
    fi

    candidate="$ROOT/$spec"
    if [[ -f "$candidate" ]]; then
        echo "$candidate"
        return 0
    fi

    case "$spec" in
        *.skin.json) ;;
        *)
            candidate="$ROOT/${spec}.skin.json"
            if [[ -f "$candidate" ]]; then
                echo "$candidate"
                return 0
            fi
            ;;
    esac

    want="$(printf '%s' "$spec" | sed 's/\.skin\.json$//' | tr '[:upper:]' '[:lower:]')"
    for f in "$ROOT"/*.skin.json; do
        [[ -f "$f" ]] || continue
        stem="$(basename "$f" .skin.json | tr '[:upper:]' '[:lower:]')"
        if [[ "$stem" == "$want" ]]; then
            echo "$f"
            return 0
        fi
    done
    return 1
}

skin_meta_name() {
    local json="$1"
    node --input-type=module -e "
import { readFileSync } from 'node:fs';
const j = JSON.parse(readFileSync(process.argv[1], 'utf8'));
const n = j?.meta?.name;
if (typeof n !== 'string' || !n.trim()) {
  process.stderr.write('skin.json 缺少 meta.name\\n');
  process.exit(1);
}
process.stdout.write(n.trim());
" "$json"
}

ensure_compiler() {
    command -v node >/dev/null 2>&1 || die "需要 Node.js ≥ 18"
    command -v pnpm >/dev/null 2>&1 || die "需要 pnpm ≥ 9（见 compiler/README.md）"
    if [[ ! -d "$COMPILER/node_modules" ]]; then
        echo "首次运行，正在 pnpm install…"
        (cd "$COMPILER" && pnpm install)
    fi
}

compile_one() {
    local json="$1"
    local png_only_flag="$2"
    local force_flag="$3"
    local xml_flag="$4"
    local name extra

    [[ -f "$json" ]] || die "找不到源文件: $json"

    name="$(skin_meta_name "$json")"
    extra=""
    if [[ "$force_flag" == "1" ]]; then
        extra="$extra --force"
    fi
    if [[ "$xml_flag" == "1" ]]; then
        :
    elif [[ "$png_only_flag" == "1" ]]; then
        extra="$extra --png-only"
    elif [[ -f "$SKINS_OUT/$name/Styles.xml" ]]; then
        extra="$extra --png-only"
        echo "检测到已有 $name/Styles.xml，仅更新 PNG（需要覆盖 XML 时加 --xml）"
    fi

    echo "编译: $(basename "$json")  →  $SKINS_OUT/$name/"
    # extra 故意不引号，以便展开为独立参数
    # shellcheck disable=SC2086
    (cd "$COMPILER" && pnpm exec tsx src/cli.ts compile "$json" --out "$SKINS_OUT" $extra)
}

PNG_ONLY=0
XML=0
FORCE=1
DO_ALL=0
DO_LIST=0
SPECS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --list)
            DO_LIST=1
            shift
            ;;
        --all)
            DO_ALL=1
            shift
            ;;
        --png-only)
            PNG_ONLY=1
            shift
            ;;
        --xml)
            XML=1
            shift
            ;;
        --no-force)
            FORCE=0
            shift
            ;;
        --force)
            FORCE=1
            shift
            ;;
        -*)
            die "未知选项: $1"
            ;;
        *)
            SPECS="${SPECS}${SPECS:+$'\n'}$1"
            shift
            ;;
    esac
done

if [[ "$DO_LIST" == "1" ]]; then
    list_sources
    exit 0
fi

if [[ "$PNG_ONLY" == "1" && "$XML" == "1" ]]; then
    die "--png-only 与 --xml 不能同时使用"
fi

if [[ "$DO_ALL" == "1" ]]; then
    SPECS="$(list_sources)"
    if [[ -z "$SPECS" ]]; then
        die "本目录没有 *.skin.json"
    fi
elif [[ -z "$SPECS" ]]; then
    usage >&2
    echo >&2
    echo "可编译:" >&2
    list_sources >&2 || true
    exit 1
fi

ensure_compiler

failed=0
OLDIFS="$IFS"
IFS=$'\n'
for spec in $SPECS; do
    IFS="$OLDIFS"
    json=""
    if ! json="$(resolve_source "$spec")"; then
        echo "错误: 找不到源文件 '$spec'（试 ./compile.sh --list）" >&2
        failed=1
        continue
    fi
    if ! compile_one "$json" "$PNG_ONLY" "$FORCE" "$XML"; then
        failed=1
    fi
    IFS=$'\n'
done
IFS="$OLDIFS"

exit "$failed"
