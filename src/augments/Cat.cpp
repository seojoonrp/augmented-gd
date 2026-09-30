// cat (고양이): every catInterval() seconds, removes catCount() random hazards
// that are on screen and ahead of the player. The cat itself sits in the
// bottom-right corner (CatNode).

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
        }
        if (!s.runAttempt() || layer->m_isPaused || !layer->m_player1 || layer->m_player1->m_isDead) return;

        float interval = s.mgr().catInterval();
        if (interval <= 0.f) return;
        m_timer += dt;
        if (m_timer < interval) return;
        m_timer = 0.f;
        this->sweep(s);
    }

    // before GD's reset, so GD's own per-object reset still gets the last word
    bool onBeforeReset(LevelSession&) override {
        m_timer = 0.f;
        m_removed.restore();
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
        if (m_node) m_node->castAt(hit);
        if (removed > 0) s.onHazardsDestroyed(removed);
    }

    float m_timer = 0.f;   // since the last sweep
    hazard::Removed m_removed;
    CatNode* m_node = nullptr;   // child of m_uiLayer
};

} // namespace

std::unique_ptr<Augment> makeCat() { return std::make_unique<Cat>(); }

} // namespace augment
