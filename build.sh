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
SkinRootDir=${CUR_DIR}/Skins-Design/skins" > ${file_ini}
        exit_if_err "Failed to create ${file_ini}."
        echo "OK"
    fi
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
    echo "build.sh [Release|Debug] [-g|--generate] [-b|--build] [-p|--pack] [-s|--symbols] [-h|--help]"
    echo "    -h|--help                 显示帮助消息"
    echo "    Release|Debug             使用 Release 或者 Debug 配置，缺省为 Release"
    echo "    -g|--generate             生成项目工程文件"
    echo "    -b|--build                执行编译"
    echo "    -p|--pack                 进行打包（仅支持 Release）"
    echo "    -s|--symbols              构建时保留符号并生成 dSYM（主要用于 Release 性能分析）"
    exit
}

BUILD_TYPE=Release
ACTION_BUILD=
ACTION_PACK=
ACTION_GENERATE=
ACTION_SYMBOLS=

while (($# > 0)); do
    case "$1" in
        "-h"|"--help")
            print_help
        ;;

        "Release")
            BUILD_TYPE=Release
        ;;

        "Debug")
            BUILD_TYPE=Debug
        ;;

        "-g"|"--generate")
            ACTION_GENERATE=1
        ;;

        "-b"|"--build")
            ACTION_BUILD=1
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

if [[ ! $ACTION_BUILD ]] && [[ ! $ACTION_PACK ]] && [[ ! $ACTION_GENERATE ]] ; then
    ACTION_BUILD=1
    ACTION_PACK=1
    ACTION_GENERATE=1
fi

python3 TinyJS/build-script/build.py
VERSION="$(python3 build.py update_version_header_file)"
RELEASE_DIR="../Release/$VERSION"

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