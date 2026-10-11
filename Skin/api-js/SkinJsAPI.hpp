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
const StringView SS_ON_SIZE = makeCommonString("onsize");
const StringView SS_ON_ACTIVATE = makeCommonString("onactivate");
const StringView SS_ON_DESTORY = makeCommonString("ondestory");
const StringView SS_ON_MOUSE_ACTIVATE = makeCommonString("onmouseactivate");
const StringView SS_HEIGHT = makeCommonString("height");
const StringView SS_WIDTH = makeCommonString("width");

enum SkinJsDataTypes {
    SJT_DOCUMENT,
    SJT_UIOBJECT,
    SJT_MOUSE_EVENT,
};

void initSkinJsAPIs();

#endif /* SkinJsAPI_hpp */
