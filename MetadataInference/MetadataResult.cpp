#include "MetadataResult.h"
#include "Utils/Utils.h"

const char *metadataStatusToString(MetadataStatus s) {
    switch (s) {
        case MetadataStatus::RAW:        return "raw";
        case MetadataStatus::INFERRED:   return "inferred";
        case MetadataStatus::NORMALIZED: return "normalized";
        case MetadataStatus::VERIFIED:   return "verified";
    }
    return "raw";
}

int metadataStatusToInt(MetadataStatus s) {
    return (int)s;
}

void MetadataResult::writeToJson(RapidjsonWriterEx &w) const {
    w.startObject();
    {
        // 序列化单个字段为 { value, source, confidence }。
        struct FieldVisitor {
            RapidjsonWriterEx &w;
            const char *name;
            void operator()(const MetadataField &f) const {
                w.writeKey(name);
                if (f.empty()) {
                    w.writeNull();
                    return;
                }
                w.startObject();
                w.writeKey("value");
                w.writeString(f.value.c_str());
                w.writeKey("source");
                w.writeString(sourceTypeToString(f.source));
                w.writeKey("confidence");
                w.writeDouble(f.confidence);
                w.endObject();
            }
        };
        FieldVisitor v{ w, nullptr };
        v.name = "title";   v(this->title);
        v.name = "artist";  v(this->artist);
        v.name = "album";   v(this->album);
        v.name = "year";    v(this->year);
        v.name = "track";   v(this->trackNumber);
        v.name = "disc";    v(this->discNumber);
        v.name = "version"; v(this->version);
        v.name = "genre";   v(this->genre);
    }
    w.writeKey("status");
    w.writeString(metadataStatusToString(status));
    w.endObject();
}

std::string MetadataResult::toJson() const {
    RapidjsonWriterEx w;
    writeToJson(w);
    StringView v = w.getStringView();
    return std::string(v.data, v.len);
}

// 仅供 writeToJson 使用。
const char *sourceTypeToString(MetadataSourceType s) {
    switch (s) {
        case MetadataSourceType::Tag:      return "tag";
        case MetadataSourceType::Path:    return "path";
        case MetadataSourceType::Filename: return "filename";
        case MetadataSourceType::Corpus:   return "corpus";
        case MetadataSourceType::External: return "external";
        case MetadataSourceType::Manual:   return "manual";
    }
    return "path";
}