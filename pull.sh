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

function pull() {
    BRANCH=`git branch | sed -n 's/\* \(.*\)$/\1/p'`
    # echo $BRANCH
    # set -x
    git pull --autostash --rebase "origin" $BRANCH

    exit_if_err "Failed to pull $BRANCH, in $(pwd)"
}

function pull_or_clone() {
    git_url=$1
    targ_dir=$2

    if [ -d $2 ]; then
        cd $targ_dir
        echo "Updating $(pwd) ..."
        pull
        cd -
    else
        echo "Cloning $git_url into $targ_dir ..."
        git clone $git_url $targ_dir
        exit_if_err "Failed to clone $git_url $targ_dir"
    fi
}

pull
pull_or_clone git@gitee.com:xiaohongyong/LyricServer.git LyricServer
pull_or_clone git@gitee.com:xiaohongyong/music-player-third-parties.git third-parties
pull_or_clone git@gitee.com:xiaohongyong/TinyJS.git TinyJS

echo "== update successfully =="
