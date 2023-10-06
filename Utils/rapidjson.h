//
//  rapidjson.h
//  Mp3Player
//
//  Created by henry_xiao on 2023/1/27.
//

#ifndef rapidjson_h
#define rapidjson_h

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>
#include <string>
#include <vector>


using RapidjsonWriter = rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag>;
using RapidjsonPrettyWriter = rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag>;

/** rapidjson writer 的用法:
 rapidjson::StringBuffer buf;
 RapidjsonWriter writer(buf);

 writer.StartObject();
 writer.Key("type");
 writer.String(m_type.c_str());

 writer.Key("result");
 writer.String("OK");
 writer.EndObject();

 buf.GetString();
 buf.GetSize();
 */

std::string getMemberString(const rapidjson::Value &message, const char *key, const char *defVal = "");
int getMemberInt(const rapidjson::Value &message, const char *key, int defVal = 0);
bool getMemberBool(const rapidjson::Value &message, const char *key, bool defVal = false);
std::vector<int> getMemberIntArray(const rapidjson::Value &message, const char *key);

#endif /* rapidjson_h */
