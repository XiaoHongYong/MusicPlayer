#pragma once

#include <string>
#include <vector>

// 垃圾词过滤（docs/web-console/metadata.md §7）。
// 分级：hard（技术/来源）直接删；soft（宣传）删；semantic（版本修饰词）不删、进 version。
namespace MetadataInference {

enum class TokenClass {
    Normal,
    HardTechnical,   // 320k flash 24bit 44.1khz ...
    HardSource,      // web-dl itunes qobuz rip ...
    SoftPromo,       // 无损 高品质 免费下载 ...
    SemanticVersion, // live remix acoustic instrumental ...
};

class GarbageFilter {
public:
    GarbageFilter();
    // 从 JSON 配置覆盖内置表；解析失败则保留内置表。
    bool loadConfig(const std::string &jsonText);

    // 按词元的 normalizeKey 分类（精确/整词匹配）。
    TokenClass classifyToken(const std::string &key) const;

    // 技术类数字词元，如 320k / 192kbps / 24bit / 44.1khz。
    bool matchTechnicalNumber(const std::string &key) const;

    bool isHard(const std::string &key) const {
        TokenClass k = classifyToken(key);
        return k == TokenClass::HardTechnical || k == TokenClass::HardSource || matchTechnicalNumber(key);
    }
    bool isSoft(const std::string &key) const { return classifyToken(key) == TokenClass::SoftPromo; }
    bool isSemanticVersion(const std::string &key) const { return classifyToken(key) == TokenClass::SemanticVersion; }

private:
    std::vector<std::string> _hardTechnical, _hardSource, _softPromo, _semanticVersion;
};

// 括号内容（词元 key）是否为可丢弃的"噪音"（Official / Audio / MV / OST ...）。
bool isBracketNoise(const std::string &key);

} // namespace MetadataInference