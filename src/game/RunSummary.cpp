#include "RunSummary.hpp"
#include "AugmentManager.hpp"
#include "LevelSession.hpp"
#include "Records.hpp"
#include "../ui/RunInfoPopup.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace augment::summary {

void open() {
    auto s = AugmentManager::get().session();
    if (!s) {
        log::info("Summary: asked for without a session");
        return;
    }
    auto& mgr = AugmentManager::get();
    RunInfoPopup::Info info;
    info.deaths = mgr.deaths();
    // What the HUD's best shows, minus the attempt still in progress:
    // the record only moves on deaths, and this must never read above it.
    info.sessionBest = static_cast<int>(std::max(mgr.bestPercent(), mgr.lifeBest()));
    info.record = records::best(s->layer()->m_level);
    for (auto const& def : allAugments()) {
        if (int lvl = mgr.levelOf(def.id); lvl > 0) info.augments.push_back({ &def, lvl });
    }
    if (auto popup = RunInfoPopup::create(std::move(info))) popup->show();
}

} // namespace augment::summary
