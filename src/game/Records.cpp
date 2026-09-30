#include "Records.hpp"
#include "AugmentManager.hpp"
#include "LevelSession.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

namespace augment::records {

namespace {
    int s_hideDepth = 0;
    bool s_ownNewBest = false;

    // main and online levels share low ids, so the type goes in the key.
    // editor levels are all id 0, use m_M_ID. TODO: check m_M_ID survives restarts
    std::string keyFor(GJGameLevel* level) {
        switch (level->m_levelType) {
            case GJLevelType::Main: return fmt::format("record.main.{}", level->m_levelID.value());
            case GJLevelType::Editor: return fmt::format("record.editor.{}", level->m_M_ID);
            default: return fmt::format("record.online.{}", level->m_levelID.value());
        }
    }
}

int best(GJGameLevel* level) {
    if (!level) return 0;
    return Mod::get()->getSavedValue<int>(keyFor(level), 0);
}

bool submit(GJGameLevel* level, int percent) {
    if (!level) return false;
    auto key = keyFor(level);
    int old = Mod::get()->getSavedValue<int>(key, 0);
    if (percent <= old) return false;
    Mod::get()->setSavedValue<int>(key, percent);
    // save now, not at exit, so a crash doesn't lose it
    if (auto res = Mod::get()->saveData(); res.isErr()) {
        log::warn("Record save failed: {}", res.unwrapErr());
    }
    return true;
}

void showNewBest(PlayLayer* layer, int percent) {
    if (!layer || !layer->m_level) return;
    auto level = layer->m_level;
    // not sure if the popup reads the live percent or the saved one, so fake
    // the saved one for the call (GD saves before it shows) and put it back after
    int normal = level->m_normalPercent.value();
    level->m_normalPercent = percent;
    s_ownNewBest = true;
    layer->showNewBest(false, 0, 0, false, false, false);
    s_ownNewBest = false;
    level->m_normalPercent = normal;
}

bool showingOwnNewBest() {
    return s_ownNewBest;
}

bool hiddenFromGd() {
    if (s_hideDepth > 0) return true;
    auto s = AugmentManager::get().session();
    return s && s->runAttempt();
}

HideFromGd::HideFromGd() { s_hideDepth++; }
HideFromGd::~HideFromGd() { s_hideDepth--; }

} // namespace augment::records
