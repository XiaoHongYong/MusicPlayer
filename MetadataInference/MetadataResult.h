#pragma once

#include <string>
#include "Utils/rapidjson.h"

// 元数据推断的类型定义（docs/web-console/metadata.md §3）。
// 本模块只推断展示/统计需要的字段，不自动改写音频文件。

enum class MetadataStatus {
    RAW = 0,          // 未推断 / 原始标签基本不可用
    INFERRED = 1,     // 系统从路径/文件名推出来了
    NORMALIZED = 2,   // 已清洗
    VERIFIED = 3,     // 用户确认或可靠外部数据确认
};

// 单个字段值来自哪里。
enum class MetadataSourceType {
    Tag = 0,          // 嵌入标签
    Path,             // 路径层级
    Filename,         // 文件名
    Corpus,           // 整库词元统计（后续分片）
    External,         // 外部数据库匹配（后续分片）
    Manual,           // 用户/人工修正
};

struct MetadataField {
    std::string value;
    MetadataSourceType source = MetadataSourceType::Path;
    float confidence = 0.0f;   // 0.0 ~ 1.0

    bool empty() const { return value.empty(); }
};

struct MetadataResult {
    MetadataField title;
    MetadataField artist;
    MetadataField album;
    MetadataField year;
    MetadataField trackNumber;
    MetadataField discNumber;
    MetadataField version;
    MetadataField genre;
    MetadataStatus status = MetadataStatus::RAW;

    bool isEmpty() const {
        return title.empty() && artist.empty() && album.empty()
            && year.empty() && version.empty();
    }

    void writeToJson(RapidjsonWriterEx &w) const;
    std::string toJson() const;
};

const char *metadataStatusToString(MetadataStatus s);
int metadataStatusToInt(MetadataStatus s);
const char *sourceTypeToString(MetadataSourceType s);