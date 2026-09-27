// cat (고양이): every catInterval() seconds of play, remove catCount()
// random hazards that are on screen and ahead of the player. The scan and
// the removal / put-back bookkeeping are hazard:: (HazardRemoval.hpp), shared
// with the missile. A CatNode in the UI layer fires a laser at each removed
// hazard.

#include "Augments.hpp"
#include "HazardRemoval.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../ui/CatNode.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {

class Cat : public Augment {
public:
    Cat() : Augment({ ids::Cat }) {}

    void onFrame(LevelSession& s, float dt) override {
        if (!s.owns(ids::Cat)) return;
        auto layer = s.layer();
        if (!m_node && layer->m_uiLayer) {
            m_node = CatNode::create();
            layer->m_uiLayer->addChild(m_node, 999);
            log::info("Cat: node added to the UI layer");
        }
        if (!s.runAttempt() || layer->m_isPaused || !layer->m_player1 || layer->m_player1->m_isDead) return;

        float interval = s.mgr().catInterval();
        if (interval <= 0.f) return;
        m_timer += dt;
        if (m_timer < interval) return;
        m_timer = 0.f;
        this->sweep(s);
    }

    // Put back everything the cat took this attempt. Runs before GD's own
    // reset so any per-object reset GD does still gets the last word.
    bool onBeforeReset(LevelSession&) override {
        m_timer = 0.f;
        if (m_removed.empty()) return false;
        auto count = m_removed.size();
        int stillDisabled = m_removed.restore();
        log::info("Cat: restored {} hazards ({} were still disabled)", count, stillDisabled);
        return false;
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        auto& mgr = s.mgr();
        return fmt::format(
            "Lv{}  {} per {:.1f}s, next in {:.1f}s, removed {}",
            s.levelOf(ids::Cat), mgr.catCount(), mgr.catInterval(),
            std::max(0.f, mgr.catInterval() - m_timer), m_removed.size()
        );
    }

private:
    void sweep(LevelSession& s) {
        auto layer = s.layer();
        int want = s.mgr().catCount();
        if (want <= 0 || !layer->m_objectLayer || !layer->m_player1) return;

        auto view = hazard::viewAhead(layer);
        auto candidates = hazard::hazardsInView(s, view);

        static std::mt19937 rng{ std::random_device{}() };
        std::shuffle(candidates.begin(), candidates.end(), rng);
        int removed = 0;
        std::vector<GameObject*> hit;
        for (auto obj : candidates) {
            if (removed >= want) break;
            m_removed.take(obj);
            hit.push_back(obj);
            removed++;
        }
        if (m_node) m_node->fireAt(hit);
        log::info(
            "Cat: removed {}/{} of {} hazards in view at {:.1f}% (scan x {:.0f}..{:.0f}, {} removed this attempt)",
            removed, want, candidates.size(), s.percent(), view.lo, view.hi, m_removed.size()
        );
        if (removed > 0) {
            s.notice(fmt::format("CAT  -{}", removed), { 255, 180, 230 });
            s.onHazardsDestroyed(removed);
        }
    }

    // Seconds since the last sweep, and what this attempt's sweeps removed.
    float m_timer = 0.f;
    hazard::Removed m_removed;
    // Child of m_uiLayer; dies with the level.
    CatNode* m_node = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeCat() { return std::make_unique<Cat>(); }

} // namespace augment
