#!/bin/bash

CUR_DIR=`dirname ${BASH_SOURCE-$0}`
cd ${CUR_DIR}
CUR_DIR="$(pwd)"

function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

function create_music_player_ini() {
    mkdir -p ${CUR_DIR}/build/Debug
    file_ini="${CUR_DIR}/build/Debug/MusicPlayer.ini"
    if ! test -f ${file_ini} ; then
        # lyrics-server.ini
        echo "Creating ${file_ini} ..."

        echo "[MusicPlayer]
SkinRootDir=${CUR_DIR}/Skins-Design/skins
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
    python3 "${CUR_DIR}/tools/i18n_extract.py"
    exit_if_err "Failed to generate i18n (tools/i18n_extract.py)."
}

# 把语言包装入 App Bundle：Contents/Resources/lang/*.ini
function install_lang_packs() {
    local app_resources="${CUR_DIR}/build/${BUILD_TYPE}/MusicPlayer.app/Contents/Resources"
    local dest_dir="${app_resources}/lang"

    if [ ! -d "${app_resources}" ] ; then
        echo "Skip installing language packs: app Resources not found at ${app_resources}"
        return 0
    fi

    echo "Install language packs → ${dest_dir}"
    rm -rf "${dest_dir}"
    mkdir -p "${dest_dir}"
    if ls "${CUR_DIR}/i18n/lang/"*.ini >/dev/null 2>&1 ; then
        cp -R "${CUR_DIR}/i18n/lang/"*.ini "${dest_dir}/"
        exit_if_err "Failed to copy language packs to app bundle."
    else
        echo "WARNING: no i18n/lang/*.ini generated; desktop UI will stay English."
    fi
    echo "OK"
}

# 编译媒体中心前端，并把 dist 拷进 App Bundle 的 Resources/local-server/
function build_and_install_media_center() {
    local www_dir="${CUR_DIR}/LocalServer/www"
    local dist_dir="${www_dir}/dist"
    local app_resources="${CUR_DIR}/build/${BUILD_TYPE}/MusicPlayer.app/Contents/Resources"
    local dest_dir="${app_resources}/local-server"

    if ! command -v pnpm >/dev/null 2>&1 ; then
        echo "ERROR: pnpm not found; cannot build media center (LocalServer/www)."
        exit 1
    fi

    echo "Build media center web (LocalServer/www)..."
    if [ ! -d "${www_dir}/node_modules" ] ; then
        (cd "${www_dir}" && pnpm install)
        exit_if_err "Failed to pnpm install LocalServer/www."
    fi
    (cd "${www_dir}" && pnpm build)
    exit_if_err "Failed to build LocalServer/www."

    if [ ! -d "${app_resources}" ] ; then
        echo "Skip installing media center: app Resources not found at ${app_resources}"
        return 0
    fi

    echo "Install media center → ${dest_dir}"
    rm -rf "${dest_dir}"
    mkdir -p "${dest_dir}"
    # dist 内容直接落在 local-server/ 下（需有 index.html）
    cp -R "${dist_dir}/." "${dest_dir}/"
    exit_if_err "Failed to copy media center dist to app bundle."
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

function print_help() {
    echo "build.sh [Release|Debug] [-g|--generate] [-b|--build] [-l|--lang] [-m|--media] [-p|--pack] [-s|--symbols] [-h|--help]"
    echo "    -h|--help                 显示帮助消息"
    echo "    Release|Debug             使用 Release 或者 Debug 配置，缺省为 Release"
    echo "    -g|--generate             生成项目工程文件"
    echo "    -b|--build                执行编译"
    echo "    -l|--lang                 装入语言包（i18n -> Resources/lang）"
    echo "    -m|--media                编译并装入媒体中心（LocalServer -> Resources/local-server）"
    echo "    -p|--pack                 进行打包（仅支持 Release）"
    echo "    -s|--symbols              构建时保留符号并生成 dSYM（主要用于 Release 性能分析）"
    echo ""
    echo "参数缺省行为："
    echo "    无任何参数            ：Release 编译 + install_lang_packs + build_and_install_media_center + pack"
    echo "    仅传 Debug 或 Release ：只编译对应配置"
    exit
}

BUILD_TYPE=Release
ACTION_BUILD=
ACTION_PACK=
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

python3 TinyJS/build-script/build.py
VERSION="$(python3 tools/build.py update_version_header_file)"
RELEASE_DIR="../Release/$VERSION"

if [[ $ACTION_GENERATE ]] || [[ $ACTION_BUILD ]] || [[ $ACTION_PACK ]] || [[ $ACTION_LANG ]] ; then
    generate_i18n
fi

if [ $ACTION_GENERATE ] ; then
    echo "Generate XCode project MusicPlayer..."

    mkdir -p build
    cd build

    # 显式指定 Xcode 工具链编译器，避免 CMake 在全新 configure 时找不到编译器。
    # CODE_SIGNING_ALLOWED=NO：新版本 Xcode/cmake 的编译器识别测试会因空签名身份失败，
    # 需禁用签名才能通过（打包时发布自带签名）。
    cmake -D CMAKE_C_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" \
          -D CMAKE_CXX_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++" \
          -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
          -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="-" \
          -G Xcode -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
    exit_if_err
    cd ..

    create_music_player_ini
fi

if [ $ACTION_BUILD ] ; then
    echo "Build project MusicPlayer..."

    # -s/--symbols：构建带符号的 Release（保留符号并生成 dSYM），供性能分析。
    # 常规 Release 会因 DEPLOYMENT_POSTPROCESSING=YES 被 strip（CMakeLists.txt）。
    XCODE_ATTRS=""
    if [ $ACTION_SYMBOLS ] ; then
        XCODE_ATTRS="DEPLOYMENT_POSTPROCESSING=NO DEBUG_INFORMATION_FORMAT=dwarf-with-dsym"
    fi

    xcodebuild -project build/MusicPlayer.xcodeproj -scheme MusicPlayer -configuration $BUILD_TYPE $XCODE_ATTRS
    exit_if_err

    # 带符号构建：显式生成 dSYM，便于 atos/Instruments 做行级符号化。
    APP_BIN="build/${BUILD_TYPE}/MusicPlayer.app/Contents/MacOS/MusicPlayer"
    if [ $ACTION_SYMBOLS ] && [ -f "${APP_BIN}" ] ; then
        dsymutil "${APP_BIN}" -o "build/${BUILD_TYPE}/MusicPlayer.app.dSYM" >/dev/null 2>&1 \
            && echo "dSYM written to build/${BUILD_TYPE}/MusicPlayer.app.dSYM"
    fi
fi

# 语言包：编译后装入 Bundle（CMake Copy Resources 可能把目录拍平，这里覆盖为 lang/）。
# 独立参数 -l/--lang 控制；无 Bundle 时函数内自动跳过。
if [ $ACTION_LANG ] ; then
    install_lang_packs
fi

# 媒体中心网页：编译并装入 Bundle，供 LocalServer 静态托管（默认 127.0.0.1:12120）。
# 独立参数 -m/--media 控制；无 Bundle 时函数内自动跳过。
if [ $ACTION_MEDIA ] ; then
    build_and_install_media_center
fi

if [ $ACTION_PACK ] ; then
    if [ "$BUILD_TYPE" != "Release" ] ; then
        echo "Skip packaging: only supported for Release (got $BUILD_TYPE)."
    else
        echo "Make package: MusicPlayer.dmg ..."

        rm -f build/MusicPlayer.dmg
        rm -f build/Release/Applications
        ln -s /Applications build/Release/Applications
        hdiutil create -volname MusicPlayer -srcfolder build/Release -format UDZO build/MusicPlayer.dmg
        exit_if_err "Failed to create MusicPlayer.dmg."

        mkdir -p $RELEASE_DIR
        rm -f $RELEASE_DIR/*.dmg
        cp build/MusicPlayer.dmg $RELEASE_DIR
        exit_if_err "Failed to copy dmg to ${RELEASE_DIR}."

        create_music_player_update_json
    fi
fi

echo "== build successfully =="

# 编译失败需要安装 mbedtls:
# python3 -m pip install jsonschema