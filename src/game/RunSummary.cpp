#include "RunSummary.hpp"
#include "AugmentManager.hpp"
#include "LevelSession.hpp"
#include "Records.hpp"
#include "../ui/RunInfoPopup.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace augment::summary {

CCNode* open() {
    auto s = AugmentManager::get().session();
    if (!s) return nullptr;
    auto& mgr = AugmentManager::get();
    RunInfoPopup::Info info;
    info.deaths = mgr.deaths();
    // the HUD's best minus the live attempt: the record only moves on deaths,
    // this must never read above it
    info.sessionBest = static_cast<int>(std::max(mgr.bestPercent(), mgr.lifeBest()));
    info.record = records::best(s->layer()->m_level);
    for (auto const& def : allAugments()) {
        if (int lvl = mgr.levelOf(def.id); lvl > 0) info.augments.push_back({ &def, lvl });
    }
    auto popup = RunInfoPopup::create(std::move(info));
    if (!popup) return nullptr;
    // over a draft the director is paused and the pop-in action would never run
    popup->m_noElasticity = CCDirector::get()->isPaused();
    popup->show();
    return popup;
}

} // namespace augment::summary
