#include "AlbumArtEffectFactory.h"
#include "effects/SonicHaloEffect.h"
#include "effects/PlainCoverEffect.h"
#include "effects/LivingCoverEffect.h"
#include "effects/AudioReactorEffect.h"
#include "effects/DebugMetersEffect.h"
#include "transitions/CrossfadeTransition.h"

#include <cstring>

namespace {

const std::vector<cstr_t> kEffectIds = {
    "sonic-halo",
    "plain-cover",
    "living-cover",
    "audio-reactor",
    "debug-meters",
};

} // namespace

cstr_t defaultAlbumArtEffectId() {
    return "sonic-halo";
}

cstr_t defaultAlbumArtTransitionId() {
    return "crossfade";
}

const std::vector<cstr_t> &albumArtEffectIds() {
    return kEffectIds;
}

std::unique_ptr<IAlbumArtEffect> createAlbumArtEffect(cstr_t id) {
    if (id && strcasecmp(id, "plain-cover") == 0) {
        return std::make_unique<PlainCoverEffect>();
    }
    if (id && strcasecmp(id, "living-cover") == 0) {
        return std::make_unique<LivingCoverEffect>();
    }
    if (id && strcasecmp(id, "audio-reactor") == 0) {
        return std::make_unique<AudioReactorEffect>();
    }
    if (id && strcasecmp(id, "debug-meters") == 0) {
        return std::make_unique<DebugMetersEffect>();
    }
    return std::make_unique<SonicHaloEffect>();
}

std::unique_ptr<IAlbumArtEffectTransition> createAlbumArtEffectTransition(cstr_t id) {
    (void)id;
    return std::make_unique<CrossfadeTransition>();
}
