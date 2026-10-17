//
//  SkinJsAPI.hpp
//  MusicPlayer
//
//  Created by henry_xiao on 2022/12/21.
//

#pragma once

#ifndef SkinJsAPI_hpp
#define SkinJsAPI_hpp

#include "JsSkinDocument.hpp"


const StringView SS_ON_COMMAND = makeCommonString("oncommand");
const StringView SS_ON_MOUSE_MOVE = makeCommonString("onmousemove");
const StringView SS_ON_MOUSE_ENTER = makeCommonString("onmouseenter");
const StringView SS_ON_MOUSE_LEAVE = makeCommonString("onmouseleave");
const StringView SS_ON_KILL_FOCUS = makeCommonString("onkillfocus");
const StringView SS_ON_POINTER_DOWN = makeCommonString("onpointerdown");
const StringView SS_ON_POINTER_UP = makeCommonString("onpointerup");
const StringView SS_ON_SIZE = makeCommonString("onsize");
const StringView SS_ON_ACTIVATE = makeCommonString("onactivate");
const StringView SS_ON_DESTORY = makeCommonString("ondestory");
const StringView SS_ON_MOUSE_ACTIVATE = makeCommonString("onmouseactivate");
const StringView SS_HEIGHT = makeCommonString("height");
const StringView SS_WIDTH = makeCommonString("width");
const StringView SS_LEFT = makeCommonString("left");
const StringView SS_TOP = makeCommonString("top");
const StringView SS_ID = makeCommonString("id");
// SS_NAME 已在 TinyJS ConstStrings 中定义

enum SkinJsDataTypes {
    SJT_DOCUMENT,
    SJT_UIOBJECT,
    SJT_MOUSE_EVENT,
};

void initSkinJsAPIs();

#endif /* SkinJsAPI_hpp */
