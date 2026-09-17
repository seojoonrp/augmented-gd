// draft-count (기회비용): four cards per draft instead of three. The rule
// itself is RunState::draftCardCount(); this only owns the HUD row.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"

#include <Geode/Geode.hpp>

namespace augment {

namespace {

class DraftCount : public Augment {
public:
    DraftCount() : Augment({ ids::DraftCount }) {}

    std::string hudState(LevelSession& s, std::string const&) override {
        return fmt::format("{} cards", s.mgr().draftCardCount());
    }
};

} // namespace

std::unique_ptr<Augment> makeDraftCount() { return std::make_unique<DraftCount>(); }

} // namespace augment
