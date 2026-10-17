//
//  Profile.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2022/12/20.
//

#include "interpreter/VirtualMachineTypes.hpp"
#include "objects/JsLibObject.hpp"
#include "../SkinTypes.h"


// getInt(key, def) → [SkinJs]
// getInt(section, key, def) → 任意节（如 Orbit.PlaylistWnd）
void profile_getInt(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    if (args.count < 1) {
        ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.getInt");
        return;
    }

    string section = "SkinJs";
    string key;
    int defVal = 0;

    if (args.count >= 3) {
        auto sec = args.getStringAt(ctx, 0);
        auto k = args.getStringAt(ctx, 1);
        if (sec.empty() || k.empty()) {
            ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.getInt");
            return;
        }
        section = sec.toString();
        key = k.toString();
        defVal = args.getIntAt(ctx, 2, 0);
    } else {
        auto k = args.getStringAt(ctx, 0);
        if (k.empty()) {
            ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.getInt");
            return;
        }
        key = k.toString();
        defVal = args.getIntAt(ctx, 1, 0);
    }

    ctx->retValue = makeJsValueInt32(g_profile.getInt(section.c_str(), key.c_str(), defVal));
}

// writeInt(key, val) → [SkinJs]
// writeInt(section, key, val) → 任意节
void profile_writeInt(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    if (args.count < 2) {
        ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.writeInt");
        return;
    }

    string section = "SkinJs";
    string key;
    int val = 0;

    if (args.count >= 3) {
        auto sec = args.getStringAt(ctx, 0);
        auto k = args.getStringAt(ctx, 1);
        if (sec.empty() || k.empty()) {
            ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.writeInt");
            return;
        }
        section = sec.toString();
        key = k.toString();
        val = args.getIntAt(ctx, 2);
    } else {
        auto k = args.getStringAt(ctx, 0);
        if (k.empty()) {
            ctx->throwException(JE_TYPE_ERROR, "Invalid parameters for profile.writeInt");
            return;
        }
        key = k.toString();
        val = args.getIntAt(ctx, 1);
    }

    g_profile.writeInt(section.c_str(), key.c_str(), val);
    ctx->retValue = jsValueUndefined;
}

static JsLibProperty profileFunctions[] = {
    { "name", nullptr, "Profile" },
    { "length", nullptr, nullptr, jsValueLength1Property },
    { "getInt", profile_getInt },
    { "writeInt", profile_writeInt },
};

void registerProfile(VMRuntimeCommon *rt) {
    setGlobalLibObject("profile", rt, profileFunctions, CountOf(profileFunctions));
}
