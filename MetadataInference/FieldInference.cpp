#include "FieldInference.h"
#include "Normalizer.h"
#include "Utils/Utils.h"
#include <algorithm>

namespace MetadataInference {

float FieldInference::scoreForMetaField(const std::string &value, float base) const {
    if (value.empty()) {
        return 0.0f;
    }
    std::string key = MetadataNormalizer::normalizeKey(value);
    if (MetadataNormalizer::looksLikeGenericTitle(key)) {
        return 0.0f;
    }
    if (_gf.matchTechnicalNumber(key) || _gf.isHard(key) || _gf.isSoft(key)) {
        return base * 0.3f;
    }
    return base;
}

MetadataResult FieldInference::infer(const InferInput &in) const {
    MetadataResult r;

    auto fin = _filename.parse(in.fileNameNoExt);
    auto pin = _path.parse(in.filePath.c_str());

    bool albumReinterpretedAsArtist = false;

    // ---- title ----
    std::string tTitle = MetadataNormalizer::normalizeDisplay(fin.title);
    std::string tTagTitle = MetadataNormalizer::normalizeDisplay(in.tagTitle);
    float sTitle = scoreForMetaField(tTitle, 0.92f);
    float sTagTitle = scoreForMetaField(tTagTitle, 0.85f);
    if (sTitle >= sTagTitle && !tTitle.empty()) {
        r.title = { tTitle, MetadataSourceType::Filename, sTitle };
    } else if (sTagTitle > 0.0f) {
        r.title = { tTagTitle, MetadataSourceType::Tag, sTagTitle };
    } else if (!tTitle.empty()) {
        r.title = { tTitle, MetadataSourceType::Filename, sTitle };
    }

    // ---- artist ----
    float sPathArtist = scoreForMetaField(pin.artist, 0.9f);
    float sFileArtist = scoreForMetaField(fin.artist, 0.72f);
    float sTagArtist = 0.0f;
    std::string tTagArtist = MetadataNormalizer::normalizeDisplay(in.tagArtist);
    sTagArtist = scoreForMetaField(tTagArtist, 0.8f);

    if (sPathArtist >= sFileArtist && sPathArtist >= sTagArtist && sPathArtist > 0.0f) {
        r.artist = { MetadataNormalizer::normalizeDisplay(pin.artist), MetadataSourceType::Path, sPathArtist };
    } else if (sFileArtist >= sTagArtist && sFileArtist > 0.0f) {
        r.artist = { MetadataNormalizer::normalizeDisplay(fin.artist), MetadataSourceType::Filename, sFileArtist };
    } else if (sTagArtist > 0.0f) {
        r.artist = { tTagArtist, MetadataSourceType::Tag, sTagArtist };
    } else if (sFileArtist > 0.0f) {
        r.artist = { MetadataNormalizer::normalizeDisplay(fin.artist), MetadataSourceType::Filename, sFileArtist };
    }

    // 扁平艺人目录：路径只给了一个目录、文件名没有艺人、标签弱 → 解释为艺人。
    if (r.artist.empty() && !pin.album.empty() && sFileArtist == 0.0f && sTagArtist == 0.0f && !r.title.empty()) {
        std::string k = MetadataNormalizer::normalizeKey(pin.album);
        if (!looksLikeGenreDir(k)) {
            r.artist = { MetadataNormalizer::normalizeDisplay(pin.album), MetadataSourceType::Path, 0.55f };
            albumReinterpretedAsArtist = true;
        }
    }

    // ---- album ----
    std::string tAlbum = MetadataNormalizer::normalizeDisplay(fin.album);
    float sPathAlbum = scoreForMetaField(pin.album, 0.88f);
    float sFileAlbum = scoreForMetaField(tAlbum, 0.7f);
    float sTagAlbum = scoreForMetaField(MetadataNormalizer::normalizeDisplay(in.tagAlbum), 0.8f);

    if (!albumReinterpretedAsArtist) {
        if (sPathAlbum >= sTagAlbum && sPathAlbum >= sFileAlbum && sPathAlbum > 0.0f) {
            r.album = { MetadataNormalizer::normalizeDisplay(pin.album), MetadataSourceType::Path, sPathAlbum };
        } else if (sTagAlbum >= sFileAlbum && sTagAlbum > 0.0f) {
            r.album = { MetadataNormalizer::normalizeDisplay(in.tagAlbum), MetadataSourceType::Tag, sTagAlbum };
        } else if (sFileAlbum > 0.0f) {
            r.album = { tAlbum, MetadataSourceType::Filename, sFileAlbum };
        }
    }

    // ---- year / track / disc / version / genre ----
    int year = fin.year ? fin.year : pin.year;
    if (!year) {
        year = in.tagYear;
    }
    if (year) {
        r.year = { itos(year), MetadataSourceType::Path, 0.85f };
    }
    if (fin.track > 0) {
        r.trackNumber = { itos(fin.track), MetadataSourceType::Filename, 0.9f };
    } else if (in.tagTrack > 0) {
        r.trackNumber = { itos(in.tagTrack), MetadataSourceType::Tag, 0.8f };
    }
    if (fin.disc > 0) {
        r.discNumber = { itos(fin.disc), MetadataSourceType::Filename, 0.9f };
    }
    if (!fin.version.empty()) {
        r.version = { fin.version, MetadataSourceType::Filename, 0.9f };
    }
    if (!MetadataNormalizer::normalizeDisplay(in.tagGenre).empty()) {
        r.genre = { in.tagGenre, MetadataSourceType::Tag, 0.7f };
    }

    // 状态：有任一来源可靠地填出 title/artist/album 即视为已清洗，否则 RAW。
    float maxCore = scoreForMetaField(r.title.value, 1.0f);
    maxCore = std::max(maxCore, scoreForMetaField(r.artist.value, 1.0f));
    maxCore = std::max(maxCore, scoreForMetaField(r.album.value, 1.0f));
    if (r.isEmpty()) {
        r.status = MetadataStatus::RAW;
    } else if (maxCore >= 0.8f) {
        r.status = MetadataStatus::NORMALIZED;
    } else {
        r.status = MetadataStatus::INFERRED;
    }

    return r;
}

} // namespace MetadataInference