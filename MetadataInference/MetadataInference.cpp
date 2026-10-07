#include "MetadataInference.h"
#include "Normalizer.h"
#include "Utils/Utils.h"

namespace MetadataInference {

InferenceEngine::InferenceEngine() : _field(_garbage, _numbers) {
    // 尝试用 Resources/metadata_words.json 覆盖内置垃圾词表；失败则用内置表。
    string cfg = getAppResourceFile("metadata_words.json");
    string text;
    if (!cfg.empty() && readFile(cfg.c_str(), text)) {
        _garbage.loadConfig(text);
    }
}

MetadataResult MetadataInference::InferenceEngine::infer(const InferInput &in) const {
    return _field.infer(in);
}

MetadataResult MetadataInference::InferenceEngine::inferForMedia(const Media *media) const {
    InferInput in;
    in.filePath = media->url;
    in.fileNameNoExt = fileGetTitle(media->url.c_str());
    in.tagArtist = media->artist;
    in.tagTitle = media->title;
    in.tagAlbum = media->album;
    in.tagGenre = media->genre;
    in.tagYear = media->year;
    in.tagTrack = media->trackNumb;
    return _field.infer(in);
}

bool MetadataInference::InferenceEngine::applyToMedia(const MetadataResult &r, Media *media) const {
    bool changed = false;

    auto setStr = [&](std::string &dst, const MetadataField &f) {
        if (f.empty()) {
            return;
        }
        if (dst != f.value) {
            dst = f.value;
            changed = true;
        }
    };

    // 只应用置信度达标的字段，避免覆盖良好的标签。
    if (r.title.confidence >= 0.8f)      setStr(media->title, r.title);
    if (r.artist.confidence >= 0.8f)     setStr(media->artist, r.artist);
    if (r.album.confidence >= 0.8f)      setStr(media->album, r.album);
    if (r.year.confidence >= 0.8f && !r.year.value.empty()) {
        int y = atoi(r.year.value.c_str());
        if (media->year != y) {
            media->year = y;
            changed = true;
        }
    }
    if (r.trackNumber.confidence >= 0.8f && !r.trackNumber.value.empty()) {
        int t = atoi(r.trackNumber.value.c_str());
        if (media->trackNumb != t) {
            media->trackNumb = t;
            changed = true;
        }
    }
    if (r.discNumber.confidence >= 0.8f && !r.discNumber.value.empty()) {
        int d = atoi(r.discNumber.value.c_str());
        if (media->discNumb != d) {
            media->discNumb = (int16_t)d;
            changed = true;
        }
    }
    if (r.version.confidence >= 0.8f)    setStr(media->version, r.version);

    if (media->metaStatus != r.status) {
        media->metaStatus = r.status;
        changed = true;
    }

    return changed;
}

} // namespace MetadataInference

MetadataInference::InferenceEngine g_metadataInference;