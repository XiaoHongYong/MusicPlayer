#!/bin/bash
# macOS：Xcode 工程 + xcodebuild，产物为 MusicPlayer.app，打包为 dmg。
# Windows：在 Git Bash 中运行。CMake 生成 Visual Studio 工程（x64），
# cmake --build 编译，产物为 build/<Debug|Release>/MusicPlayer.exe。

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${CUR_DIR}"

uname_s="$(uname -s 2>/dev/null || echo unknown)"
case "${uname_s}" in
    Darwin*)
        HOST_OS=mac
        ;;
    MINGW*|MSYS*|CYGWIN*)
        HOST_OS=windows
        ;;
    *)
        HOST_OS=unsupported
        ;;
esac

function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

function require_supported_host() {
    if [ "${HOST_OS}" = "unsupported" ]; then
        echo "Unsupported host '${uname_s}'. build.sh supports macOS, and Windows via Git Bash."
        exit 1
    fi
}

# 跳过 Microsoft Store 的 python 占位程序（WindowsApps），它会排在真正的 Python 前面。
function python_is_usable() {
    case "$1" in
        *WindowsApps*) return 1 ;;
    esac
    "$@" -c 'import sys; raise SystemExit(0 if sys.version_info[0] >= 3 else 1)' >/dev/null 2>&1
}

# macOS 用 python3；Git Bash 上常见的是 python 或 py -3。
function find_python() {
    local path
    local -a found=()
    while IFS= read -r path; do
        [ -n "${path}" ] && found+=("${path}")
    done < <(which -a python3 python 2>/dev/null || true)

    for path in "${found[@]}"; do
        if python_is_usable "${path}"; then
            PYTHON=("${path}")
            return 0
        fi
    done

    local py_path
    py_path="$(command -v py 2>/dev/null || true)"
    if [ -n "${py_path}" ] && python_is_usable "${py_path}" -3; then
        PYTHON=(py -3)
        return 0
    fi

    echo "ERROR: Python 3 not found (tried python3, python, py)."
    exit 1
}

function find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        CMAKE=(cmake)
        return
    fi
    if [ "${HOST_OS}" != "windows" ]; then
        echo "ERROR: cmake not found."
        exit 1
    fi

    local candidates=(
        "/c/Program Files/CMake/bin/cmake.exe"
        "/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        "/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        "/c/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        "/c/Program Files/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    )
    local candidate
    for candidate in "${candidates[@]}"; do
        if [ -f "${candidate}" ]; then
            CMAKE=("${candidate}")
            return
        fi
    done
    echo "ERROR: cmake not found. Install CMake and add it to PATH."
    exit 1
}

# Git Bash 里 pnpm 有时只有 pnpm.cmd，bash 不会直接执行 .cmd。
function run_pnpm() {
    if command -v pnpm >/dev/null 2>&1; then
        pnpm "$@"
        return $?
    fi
    if [ "${HOST_OS}" = "windows" ] && command -v pnpm.cmd >/dev/null 2>&1; then
        cmd //c pnpm "$@"
        return $?
    fi
    echo "ERROR: pnpm not found; cannot build media center (LocalServer/www)."
    exit 1
}

# 语言包、媒体中心要装到的目录。
# macOS: MusicPlayer.app/Contents/Resources
# Windows: 与 MusicPlayer.exe 同目录（getAppResourceDir）
function payload_dir() {
    if [ "${HOST_OS}" = "mac" ]; then
        printf '%s\n' "${CUR_DIR}/build/${BUILD_TYPE}/MusicPlayer.app/Contents/Resources"
        return
    fi

    local expected="${CUR_DIR}/build/${BUILD_TYPE}"
    if [ -f "${expected}/MusicPlayer.exe" ]; then
        printf '%s\n' "${expected}"
        return
    fi

    local exe
    # 用 Git Bash / macOS 的 find，避开 Windows 自带的 find.exe。
    exe="$(/usr/bin/find "${CUR_DIR}/build" -type f -name 'MusicPlayer.exe' -path "*${BUILD_TYPE}*" ! -path '*/CMakeFiles/*' 2>/dev/null | head -n 1 || true)"
    if [ -n "${exe}" ]; then
        dirname "${exe}"
        return
    fi
    printf '%s\n' "${expected}"
}

function create_music_player_ini() {
    # macOS 开发时优先读 .app 旁边的 ini（见 Utils/mac/App.mm）。
    # Windows 配置在 %APPDATA%\MusicPlayer，皮肤走 exe 相对路径 ../../SkinDesign/skins。
    if [ "${HOST_OS}" != "mac" ]; then
        return 0
    fi

    mkdir -p ${CUR_DIR}/build/Debug
    file_ini="${CUR_DIR}/build/Debug/MusicPlayer.ini"
    if ! test -f ${file_ini} ; then
        # lyrics-server.ini
        echo "Creating ${file_ini} ..."

        echo "[MusicPlayer]
SkinRootDir=${CUR_DIR}/SkinDesign/skins
LocalWWW=${CUR_DIR}/LocalServer/www/dist" > ${file_ini}
        exit_if_err "Failed to create ${file_ini}."
        echo "OK"
    elif ! grep -q '^LocalWWW=' "${file_ini}" ; then
        # 已有 ini：补上 Debug 开发用的媒体中心静态目录
        echo "LocalWWW=${CUR_DIR}/LocalServer/www/dist" >> "${file_ini}"
        echo "Added LocalWWW to ${file_ini}"
    fi
}

# 从 i18n/locales 生成 C++ i18n/lang/*.ini 与 Web messages.generated.ts
function generate_i18n() {
    echo "Generate i18n language packs..."
    "${PYTHON[@]}" "${CUR_DIR}/tools/i18n_extract.py"
    exit_if_err "Failed to generate i18n (tools/i18n_extract.py)."
}

# player.ico / MusicPlayer.icns 等在 .gitignore，需从母版生成。
function ensure_app_icons() {
    local need=0
    if [ "${HOST_OS}" = "windows" ] && [ ! -f "${CUR_DIR}/MusicPlayer/win32/player.ico" ]; then
        need=1
    fi
    if [ "${HOST_OS}" = "mac" ] && [ ! -f "${CUR_DIR}/MusicPlayer/MusicPlayer.icns" ]; then
        need=1
    fi
    if [ "${need}" -eq 0 ]; then
        return 0
    fi

    echo "Generate app icons (tools/generate_app_icons.py)..."
    if ! "${PYTHON[@]}" -c 'import PIL' >/dev/null 2>&1; then
        echo "ERROR: Pillow 未安装，无法生成 player.ico / 应用图标。"
        echo "请执行: ${PYTHON[*]} -m pip install Pillow"
        exit 1
    fi
    "${PYTHON[@]}" "${CUR_DIR}/tools/generate_app_icons.py"
    exit_if_err "Failed to generate app icons."
}

# macOS 装入 App Bundle 的 Contents/Resources/lang；Windows 装到 exe 同目录的 lang/。
function install_lang_packs() {
    local dest_root
    dest_root="$(payload_dir)"
    local dest_dir="${dest_root}/lang"

    if [ ! -d "${dest_root}" ] ; then
        echo "Skip installing language packs: output directory not found at ${dest_root}"
        return 0
    fi

    echo "Install language packs → ${dest_dir}"
    rm -rf "${dest_dir}"
    mkdir -p "${dest_dir}"
    if ls "${CUR_DIR}/i18n/lang/"*.ini >/dev/null 2>&1 ; then
        cp -R "${CUR_DIR}/i18n/lang/"*.ini "${dest_dir}/"
        exit_if_err "Failed to copy language packs."
    else
        echo "WARNING: no i18n/lang/*.ini generated; desktop UI will stay English."
    fi
    echo "OK"
}

# 编译媒体中心前端。macOS 拷进 Resources/local-server；Windows 拷到 exe 同目录的 local-server/。
function build_and_install_media_center() {
    local www_dir="${CUR_DIR}/LocalServer/www"
    local dist_dir="${www_dir}/dist"
    local dest_root
    dest_root="$(payload_dir)"
    local dest_dir="${dest_root}/local-server"

    echo "Build media center web (LocalServer/www)..."
    if [ ! -d "${www_dir}/node_modules" ] ; then
        (cd "${www_dir}" && run_pnpm install)
        exit_if_err "Failed to pnpm install LocalServer/www."
    fi
    (cd "${www_dir}" && run_pnpm build)
    exit_if_err "Failed to build LocalServer/www."

    if [ ! -d "${dest_root}" ] ; then
        echo "Skip installing media center: output directory not found at ${dest_root}"
        return 0
    fi

    echo "Install media center → ${dest_dir}"
    rm -rf "${dest_dir}"
    mkdir -p "${dest_dir}"
    # dist 内容直接落在 local-server/ 下（需有 index.html）
    cp -R "${dist_dir}/." "${dest_dir}/"
    exit_if_err "Failed to copy media center dist."
    echo "OK"
}

function create_music_player_update_json() {
    file="$RELEASE_DIR/music-player-update.json"
    echo "Creating ${file} ..."

    echo "{
\"version\": \"$VERSION\",
\"release-date\": \"$(date '+%Y-%m-%d')\"
}" > ${file}
    exit_if_err "Failed to create ${file}."
    echo "OK"
}

# 选出带 C++ 工具集的最新 Visual Studio。
# 通过 stdout 输出两行：生成器名、安装路径（Windows 反斜杠路径）。
# 不能省略 -G：CMake 在找不到可用 VS 时默认生成器是 NMake Makefiles，而 NMake 不接受 -A x64。
function windows_vs_info() {
    local vswhere=""
    local candidate
    for candidate in \
        "/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" \
        "/c/Program Files/Microsoft Visual Studio/Installer/vswhere.exe"
    do
        if [ -f "${candidate}" ]; then
            vswhere="${candidate}"
            break
        fi
    done
    [ -n "${vswhere}" ] || return 1

    local line path
    line="$("${vswhere}" -latest -products '*' \
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
        -property catalog_productLineVersion 2>/dev/null | tr -d '\r' | head -n 1)"
    path="$("${vswhere}" -latest -products '*' \
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
        -property installationPath 2>/dev/null | tr -d '\r' | head -n 1)"
    if [ -z "${line}" ] || [ -z "${path}" ]; then
        # VS 2017 的 C++ 组件有时登记为 VC.CoreIde，而不是 VC.Tools.x86.x64。
        line="$("${vswhere}" -latest -products '*' \
            -requires Microsoft.VisualStudio.Component.VC.CoreIde \
            -property catalog_productLineVersion 2>/dev/null | tr -d '\r' | head -n 1)"
        path="$("${vswhere}" -latest -products '*' \
            -requires Microsoft.VisualStudio.Component.VC.CoreIde \
            -property installationPath 2>/dev/null | tr -d '\r' | head -n 1)"
    fi
    [ -n "${line}" ] && [ -n "${path}" ] || return 1

    local gen=""
    case "${line}" in
        15|2017) gen="Visual Studio 15 2017" ;;
        16|2019) gen="Visual Studio 16 2019" ;;
        17|2022) gen="Visual Studio 17 2022" ;;
        18|2026) gen="Visual Studio 18 2026" ;;
        *) return 1 ;;
    esac
    printf '%s\n%s\n' "${gen}" "${path}"
}

function clear_cmake_cache() {
    echo "$1"
    rm -rf "${CUR_DIR}/build/CMakeCache.txt" "${CUR_DIR}/build/CMakeFiles"
}

function configure_project() {
    mkdir -p build

    if [ "${HOST_OS}" = "mac" ]; then
        echo "Generate Xcode project MusicPlayer..."
        cd build
        # 显式指定 Xcode 工具链编译器，避免 CMake 在全新 configure 时找不到编译器。
        # CODE_SIGNING_ALLOWED=NO：新版本 Xcode/cmake 的编译器识别测试会因空签名身份失败，
        # 需禁用签名才能通过（打包时发布自带签名）。
        "${CMAKE[@]}" -D CMAKE_C_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" \
              -D CMAKE_CXX_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++" \
              -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
              -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="-" \
              -G Xcode -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
        exit_if_err "Failed to generate project files."
        cd "${CUR_DIR}"
    else
        local gen="${CMAKE_GENERATOR:-}"
        local instance="${CMAKE_GENERATOR_INSTANCE:-}"
        if [ -z "${gen}" ] || [ -z "${instance}" ]; then
            local info
            info="$(windows_vs_info || true)"
            if [ -n "${info}" ]; then
                [ -n "${gen}" ] || gen="$(printf '%s\n' "${info}" | sed -n '1p')"
                [ -n "${instance}" ] || instance="$(printf '%s\n' "${info}" | sed -n '2p')"
            fi
        fi
        if [ -z "${gen}" ]; then
            echo "ERROR: 未找到带 C++ 工具集的 Visual Studio，无法生成工程。"
            echo "请安装 Visual Studio 2022 或 2026，并勾选工作负载“使用 C++ 的桌面开发”，然后重新运行 ./build.sh。"
            echo "也可以设置 CMAKE_GENERATOR / CMAKE_GENERATOR_INSTANCE。"
            exit 1
        fi

        local cache="${CUR_DIR}/build/CMakeCache.txt"
        if [ -f "${cache}" ]; then
            local cached_gen cached_instance
            cached_gen="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${cache}" | tr -d '\r' | head -n 1)"
            # 兼容 INTERNAL / UNINITIALIZED / STRING 等类型；旧路径残留会导致 VS 实例解析失败。
            cached_instance="$(sed -n 's/^CMAKE_GENERATOR_INSTANCE:[^=]*=//p' "${cache}" | tr -d '\r' | head -n 1)"
            if [ -n "${cached_gen}" ] && [ "${cached_gen}" != "${gen}" ]; then
                clear_cmake_cache "Existing CMake cache uses generator '${cached_gen}', switching to '${gen}'."
            elif [ -n "${instance}" ] && [ -n "${cached_instance}" ] && [ "${cached_instance}" != "${instance}" ]; then
                clear_cmake_cache "Existing CMake cache points to '${cached_instance}', switching to '${instance}'."
            fi
        fi

        echo "Generate project MusicPlayer with ${gen} ..."
        if [ -n "${instance}" ]; then
            echo "Visual Studio instance: ${instance}"
        fi
        cd build
        local -a cmake_args=(-G "${gen}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
        # 只有 Visual Studio 生成器接受 -A。NMake / Ninja 传了会在 PROJECT() 直接失败。
        case "${gen}" in
            "Visual Studio"*)
                cmake_args+=(-A "${CMAKE_GENERATOR_PLATFORM:-x64}")
                if [ -n "${instance}" ]; then
                    cmake_args+=(-DCMAKE_GENERATOR_INSTANCE="${instance}")
                fi
                ;;
        esac
        "${CMAKE[@]}" "${cmake_args[@]}" ..
        exit_if_err "Failed to generate project files."
        cd "${CUR_DIR}"
    fi

    create_music_player_ini
}

function build_project() {
    echo "Build project MusicPlayer..."

    if [ "${HOST_OS}" = "mac" ]; then
        # -s/--symbols：构建带符号的 Release（保留符号并生成 dSYM），供性能分析。
        # 常规 Release 会因 DEPLOYMENT_POSTPROCESSING=YES 被 strip（CMakeLists.txt）。
        XCODE_ATTRS=""
        if [ $ACTION_SYMBOLS ] ; then
            XCODE_ATTRS="DEPLOYMENT_POSTPROCESSING=NO DEBUG_INFORMATION_FORMAT=dwarf-with-dsym"
        fi

        xcodebuild -project build/MusicPlayer.xcodeproj -scheme MusicPlayer -configuration $BUILD_TYPE $XCODE_ATTRS
        exit_if_err "Failed to build MusicPlayer."

        # 带符号构建：显式生成 dSYM，便于 atos/Instruments 做行级符号化。
        APP_BIN="build/${BUILD_TYPE}/MusicPlayer.app/Contents/MacOS/MusicPlayer"
        if [ $ACTION_SYMBOLS ] && [ -f "${APP_BIN}" ] ; then
            dsymutil "${APP_BIN}" -o "build/${BUILD_TYPE}/MusicPlayer.app.dSYM" >/dev/null 2>&1 \
                && echo "dSYM written to build/${BUILD_TYPE}/MusicPlayer.app.dSYM"
        fi
        return
    fi

    if [ -n "${ACTION_SYMBOLS}" ]; then
        echo "Note: -s/--symbols generates a dSYM on macOS. Windows keeps the linker PDB when the toolchain emits one."
    fi

    "${CMAKE[@]}" --build "${CUR_DIR}/build" --config "${BUILD_TYPE}" --target MusicPlayer --parallel
    exit_if_err "Failed to build MusicPlayer."
}

function pack_macos() {
    echo "Make package: MusicPlayer.dmg ..."

    rm -f build/MusicPlayer.dmg
    rm -f build/Release/Applications
    ln -s /Applications build/Release/Applications
    hdiutil create -volname MusicPlayer -srcfolder build/Release -format UDZO build/MusicPlayer.dmg
    exit_if_err "Failed to create MusicPlayer.dmg."

    mkdir -p "$RELEASE_DIR"
    rm -f $RELEASE_DIR/*.dmg
    cp build/MusicPlayer.dmg $RELEASE_DIR
    exit_if_err "Failed to copy dmg to ${RELEASE_DIR}."

    create_music_player_update_json
}

function pack_windows() {
    local nsis="${CUR_DIR}/third-parties/NSIS/makensis.exe"
    if [ ! -f "${nsis}" ]; then
        if [ -n "${ACTION_PACK_USER}" ]; then
            echo "ERROR: NSIS not found at ${nsis}"
            exit 1
        fi
        echo "Skip packaging: NSIS not found at third-parties/NSIS/makensis.exe"
        return 0
    fi

    local nsh="${CUR_DIR}/MusicPlayer/win32/version.nsh"
    echo "Creating ${nsh} ..."
    {
        printf '%s\r\n' "; This file was generated by build.sh, don't modify it manually."
        printf '!define VERSION\t"%s"\r\n' "${VERSION}"
    } > "${nsh}"
    exit_if_err "Failed to write ${nsh}."

    echo "Make package: MusicPlayer setup ..."
    # Git Bash 会把 /X、/FINAL 当成路径改写。关掉转换后，nsi 路径要用 Windows 形式。
    local nsi_path
    nsi_path="$(cygpath -w "${CUR_DIR}/MusicPlayer/win32/install.nsi")"
    MSYS_NO_PATHCONV=1 "${nsis}" /X"SetCompressor /FINAL /SOLID lzma" "${nsi_path}"
    exit_if_err "Failed to run makensis."

    local installer="${CUR_DIR}/MusicPlayer/win32/MusicPlayer.exe"
    if [ ! -f "${installer}" ]; then
        echo "ERROR: installer not found at ${installer}"
        exit 1
    fi

    mkdir -p "$RELEASE_DIR"
    rm -f "$RELEASE_DIR"/*.exe
    cp "${installer}" "$RELEASE_DIR/"
    exit_if_err "Failed to copy installer to ${RELEASE_DIR}."

    create_music_player_update_json
}

function print_help() {
    echo "build.sh [Release|Debug] [-g|--generate] [-b|--build] [-l|--lang] [-m|--media] [-p|--pack] [-s|--symbols] [-h|--help]"
    echo "    -h|--help                 显示帮助消息"
    echo "    Release|Debug             使用 Release 或者 Debug 配置，缺省为 Release"
    echo "    -g|--generate             生成项目工程文件（macOS: Xcode；Windows: Visual Studio x64）"
    echo "    -b|--build                执行编译（macOS: xcodebuild；Windows: cmake --build）"
    echo "    -l|--lang                 装入语言包（macOS: Resources/lang；Windows: exe 同目录 lang/）"
    echo "    -m|--media                编译并装入媒体中心（macOS: Resources/local-server；Windows: exe 同目录 local-server/）"
    echo "    -p|--pack                 打包（macOS: dmg；Windows: NSIS，仅 Release）"
    echo "    -s|--symbols              macOS 上保留符号并生成 dSYM（主要用于 Release 性能分析）"
    echo ""
    echo "Windows 请在 Git Bash 中运行。可用环境变量 CMAKE_GENERATOR / CMAKE_GENERATOR_INSTANCE 指定生成器与 VS 安装路径。"
    echo ""
    echo "参数缺省行为："
    echo "    无任何参数            ：Release 编译 + install_lang_packs + build_and_install_media_center + pack"
    echo "    仅传 Debug 或 Release ：只编译对应配置"
    exit
}

BUILD_TYPE=Release
ACTION_BUILD=
ACTION_PACK=
ACTION_PACK_USER=
ACTION_GENERATE=
ACTION_LANG=
ACTION_MEDIA=
ACTION_SYMBOLS=

# TOTAL_ARGS：本次传入的参数个数；CONFIG_ARGS：其中 Release/Debug 配置参数的个数。
# 用于区分“无参数默认完整流水线”与“仅传 Debug/Release 只编译”。
TOTAL_ARGS=0
CONFIG_ARGS=0

while (($# > 0)); do
    TOTAL_ARGS=$((TOTAL_ARGS + 1))
    case "$1" in
        "-h"|"--help")
            print_help
        ;;

        "Release")
            BUILD_TYPE=Release
            CONFIG_ARGS=$((CONFIG_ARGS + 1))
        ;;

        "Debug")
            BUILD_TYPE=Debug
            CONFIG_ARGS=$((CONFIG_ARGS + 1))
        ;;

        "-g"|"--generate")
            ACTION_GENERATE=1
        ;;

        "-b"|"--build")
            ACTION_BUILD=1
        ;;

        "-l"|"--lang")
            ACTION_LANG=1
        ;;

        "-m"|"--media")
            ACTION_MEDIA=1
        ;;

        "-p"|"--pack")
            ACTION_PACK=1
            ACTION_PACK_USER=1
        ;;

        "-s"|"--symbols")
            ACTION_SYMBOLS=1
        ;;

        *)
            echo "Invalid parameters: ($1)"
            exit 1
        ;;
    esac
    shift
done

require_supported_host

if [[ ! $ACTION_BUILD ]] && [[ ! $ACTION_PACK ]] && [[ ! $ACTION_GENERATE ]] \
   && [[ ! $ACTION_LANG ]] && [[ ! $ACTION_MEDIA ]] ; then
    # 未指定任何主操作
    if [[ $TOTAL_ARGS == 0 ]] ; then
        # 无参数：默认完整 Release 流水线
        ACTION_BUILD=1
        ACTION_LANG=1
        ACTION_MEDIA=1
        ACTION_PACK=1
        ACTION_GENERATE=1
    elif [[ $TOTAL_ARGS == $CONFIG_ARGS ]] ; then
        # 仅传 Debug/Release：只编译对应配置
        ACTION_BUILD=1
    else
        # 其它情形（如仅 -s）：回退为只编译
        ACTION_BUILD=1
    fi
fi

find_python
if [[ $ACTION_GENERATE ]] || [[ $ACTION_BUILD ]] ; then
    find_cmake
fi

if [ ! -f "${CUR_DIR}/TinyJS/build-script/build.py" ]; then
    echo "ERROR: TinyJS/build-script/build.py not found."
    exit 1
fi
"${PYTHON[@]}" TinyJS/build-script/build.py
exit_if_err "Failed to run TinyJS/build-script/build.py."

VERSION="$("${PYTHON[@]}" tools/build.py update_version_header_file)"
exit_if_err "Failed to update version.h."
RELEASE_DIR="${CUR_DIR}/../Release/${VERSION}"

echo "Host: ${HOST_OS}, configuration: ${BUILD_TYPE}, version: ${VERSION}"

if [[ $ACTION_GENERATE ]] || [[ $ACTION_PACK ]] || [[ $ACTION_LANG ]] ; then
    generate_i18n
fi

if [[ $ACTION_PACK ]] ; then
    ensure_app_icons
fi

if [ $ACTION_GENERATE ] ; then
    configure_project
fi

if [ $ACTION_BUILD ] ; then
    build_project
fi

# 语言包：编译后装入 Bundle（CMake Copy Resources 可能把目录拍平，这里覆盖为 lang/）。
# Windows 上 CMake POST_BUILD 也会拷一份；这里按 -l 再覆盖一次。
# 独立参数 -l/--lang 控制；输出目录不存在时函数内自动跳过。
if [ $ACTION_LANG ] ; then
    install_lang_packs
fi

# 媒体中心网页：编译并装入，供 LocalServer 静态托管（默认 127.0.0.1:12120）。
# 独立参数 -m/--media 控制；输出目录不存在时函数内自动跳过。
if [ $ACTION_MEDIA ] ; then
    build_and_install_media_center
fi

if [ $ACTION_PACK ] ; then
    if [ "$BUILD_TYPE" != "Release" ] ; then
        echo "Skip packaging: only supported for Release (got $BUILD_TYPE)."
    elif [ "${HOST_OS}" = "mac" ]; then
        pack_macos
    else
        pack_windows
    fi
fi

echo "== build successfully =="

# 编译失败需要安装 mbedtls:
# python3 -m pip install jsonschema
