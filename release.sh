#!/bin/bash
# 启动多平台发布工具，详见 docs/release.md
CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${CUR_DIR}"
exec python3 "${CUR_DIR}/tools/release.py" "$@"
