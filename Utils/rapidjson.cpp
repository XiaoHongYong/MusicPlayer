//
//  rapidjson.cpp
//  Mp3Player
//
//  Created by henry_xiao on 2023/1/27.
//

#include "rapidjson.h"


std::string getMemberString(const rapidjson::Value &message, const char *key, const char *defVal) {
    assert(message.IsObject());

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

int getMemberInt(const rapidjson::Value &message, const char *key, int defVal) {
    assert(message.IsObject());

    auto it = message.FindMember(key);
    if (it == message.MemberEnd()) {
        return defVal;
    }

    auto &val = (*it).value;
    if (val.IsInt()) {
        return val.GetInt();
    } else if (val.IsString()) {
        return atoi(val.GetString());
    } else if (val.IsBool()) {
        return val.GetBool();
    }

    return defVal;
}

std::vector<int> getMemberIntArray(const rapidjson::Value &message, const char *key) {
    assert(message.IsObject());

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
