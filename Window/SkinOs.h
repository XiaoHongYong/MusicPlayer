#pragma once

#include "../Utils/Utils.h"

class SXNode;

// 皮肤 XML 使用的操作系统名：win / mac / linux。
// 各平台 Window 都不使用原生标题栏；标题栏由皮肤样式自绘。
cstr_t getSkinOsName();

// os 属性：空 = 全平台；逗号分隔列表（如 "win,linux"）。
// 亦接受别名 windows / macos / osx。
bool isSkinOsMatch(cstr_t osList);

// 节点无 os 属性，或 os 匹配当前平台时返回 true。
bool isSkinXmlNodeForCurrentOs(SXNode *node);

// 样式名平台后缀：Caption.mac / Caption.win / Caption.linux（及 windows/macos/osx 别名）。
enum class SkinStyleOsKind {
    Generic,  // 无平台后缀，全平台可用
    Current,  // 后缀匹配当前 OS，注册时去掉后缀
    Other,    // 后缀是其他 OS，应忽略
};

SkinStyleOsKind parseSkinStyleName(cstr_t rawName, string &baseName);
