#pragma once

#include "IAlbumArtEffect.h"
#include "IAlbumArtEffectTransition.h"

#include <memory>
#include <vector>

std::unique_ptr<IAlbumArtEffect> createAlbumArtEffect(cstr_t id);
std::unique_ptr<IAlbumArtEffectTransition> createAlbumArtEffectTransition(cstr_t id);

/** 已注册特效 id，顺序用于点击循环切换。 */
const std::vector<cstr_t> &albumArtEffectIds();

cstr_t defaultAlbumArtEffectId();
cstr_t defaultAlbumArtTransitionId();
