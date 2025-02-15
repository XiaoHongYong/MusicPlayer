﻿//
//  rapidjson.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/27.
//

#include "rapidjson.h"


const rapidjson::Value &getMember(const rapidjson::Value &message, const char *key) {
    static const rapidjson::Value defVal;

    assert(message.IsObject());
    if (!message.IsObject()) {
        return defVal;
    }

    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return defVal;
    }

    return (*it).value;
}

std::string getMemberString(const rapidjson::Value &message, const char *key, const char *defVal) {
    assert(message.IsObject());
    if (!message.IsObject()) {
        return defVal;
    }

    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return defVal;
    }

    auto &val = (*it).value;
    if (val.IsString()) {
        return std::string(val.GetString(),  val.GetStringLength());
    }

    return defVal;
}

int64_t getMemberInt64(const rapidjson::Value &message, const char *key, int64_t defVal) {
    assert(message.IsObject());
    if (!message.IsObject()) {
        return defVal;
    }

    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return defVal;
    }

    auto &val = (*it).value;
    if (val.IsNumber()) {
        return val.GetInt64();
    } else if (val.IsString()) {
        return atoi(val.GetString());
    } else if (val.IsBool()) {
        return val.GetBool();
    } else if (val.IsNull()) {
        return 0;
    }

    return defVal;
}

bool getMemberBool(const rapidjson::Value &message, const char *key, bool defVal) {
    assert(message.IsObject());
    if (!message.IsObject()) {
        return defVal;
    }

    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return defVal;
    }

    auto &val = (*it).value;
    if (val.IsInt()) {
        return val.GetInt() != 0;
    } else if (val.IsBool()) {
        return val.GetBool();
    } else if (val.IsString()) {
        return val.GetString()[0] != '\0';
    } else {
        return false;
    }

    return defVal;
}

std::vector<int> getMemberIntArray(const rapidjson::Value &message, const char *key) {
    assert(message.IsObject());
    if (!message.IsObject()) {
        return {};
    }

    std::vector<int> values;
    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return values;
    }

    auto &arr = (*it).value;
    if (arr.IsArray()) {
        auto count = arr.Size();
        for (int i = 0; i < count; i++) {
            auto &item = arr[i];
            if (item.IsInt()) {
                values.push_back(item.GetInt());
            }
        }
    }

    return values;
}
