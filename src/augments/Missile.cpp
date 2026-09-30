// missile (공습): every missileInterval() seconds a missile drops on a random
// hazard on screen ahead of the player; on impact (MissileNode::FallSeconds
// later) every hazard touching the missileRadius() blast is gone for the
// attempt. Aimed at a hazard so it always hits something; with nothing in
// view it stays armed and fires once something scrolls in.

#include "Augments.hpp"
#include "HazardRemoval.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Sfx.hpp"
#include "../hooks/HazardHitboxHook.hpp"
#include "../ui/MissileNode.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {

// skip targets closer than this ahead of the player (~1 s at 1x), so the
// strike clears what's coming, not what's at your feet
constexpr float kLead = 300.f;
// x range tested around the blast beyond its radius (big hazards reach in)
constexpr float kScanPad = 90.f;

class Missile : public Augment {
public:
    Missile() : Augment({ ids::Missile }) {}

    void onFrame(LevelSession& s, float dt) override {
        if (!s.owns(ids::Missile)) return;
        auto layer = s.layer();
        if (!m_node && layer->m_objectLayer) {
            m_node = MissileNode::create();
            layer->m_objectLayer->addChild(m_node, 999);
        }
        if (!s.runAttempt() || layer->m_isPaused || !layer->m_player1 || layer->m_player1->m_isDead) return;

        if (m_strike.active) {
            m_strike.t += dt;
            if (m_node) m_node->tick(dt);
            if (m_strike.t >= MissileNode::FallSeconds) this->detonate(s);
            return;
        }
        if (m_node) m_node->tick(dt);   // lets a blast fade out

        float interval = s.mgr().missileInterval();
        if (interval <= 0.f) return;
        m_timer += dt;
        if (m_timer < interval) return;
        if (this->launch(s)) m_timer = 0.f;
    }

    // before GD's reset, so GD's own per-object reset still gets the last word
    bool onBeforeReset(LevelSession&) override {
        m_timer = 0.f;
        if (m_strike.active) m_strike = {};
        if (m_node) m_node->cancel();
        m_removed.restore();
        return false;
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        auto& mgr = s.mgr();
        std::string next = m_strike.active ? "INCOMING" : fmt::format("next in {:.1f}s", std::max(0.f, mgr.missileInterval() - m_timer));
        return fmt::format(
            "Lv{}  r{:.1f} per {:.1f}s, {}, removed {}",
            s.levelOf(ids::Missile), mgr.missileRadius() / tune::BlockUnits, mgr.missileInterval(),
            next, m_removed.size()
        );
    }

private:
    // false when nothing is in view
    bool launch(LevelSession& s) {
        auto layer = s.layer();
        if (!layer->m_objectLayer || !layer->m_player1) return false;

        auto view = hazard::viewAhead(layer, kLead);
        auto candidates = hazard::hazardsInView(s, view);
        if (candidates.empty()) return false;

        static std::mt19937 rng{ std::random_device{}() };
        auto target = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];

        // impact at the target in object-layer space, drop starts above the top of the screen
        CCNode* parent = target->getParent() ? target->getParent() : layer->m_objectLayer;
        CCPoint world = parent->convertToWorldSpace({ target->getPositionX(), target->getPositionY() });
        CCPoint impact = layer->m_objectLayer->convertToNodeSpace(world);
        auto win = CCDirector::get()->getWinSize();
        CCPoint from = layer->m_objectLayer->convertToNodeSpace({ world.x, win.height + 40.f });

        m_strike = { true, impact, s.mgr().missileRadius(), 0.f };
        if (m_node) m_node->launch(from, impact, m_strike.radius);
        return true;
    }

    void detonate(LevelSession& s) {
        auto layer = s.layer();
        CCPoint centre = m_strike.impact;
        float radius = m_strike.radius;
        m_strike = {};
        if (m_node) m_node->detonate();
        sfx::play(sfx::Cue::MissileImpact);

        int removed = 0;
        s.forEachObjectInX(centre.x - radius - kScanPad, centre.x + radius + kScanPad, [&](GameObject* obj) {
            if (!hazard::isTarget(obj) || obj == layer->m_anticheatSpike) return;
            if (obj->m_isDisabled || obj->m_isDisabled2) return;
            if (!hazard::touchesCircle(obj, centre, radius)) return;
            m_removed.take(obj);
            removed++;
        });
        s.onHazardsDestroyed(removed);
    }

    float m_timer = 0.f;   // since the last launch, paused while one is in the air
    struct Strike {
        bool active = false;
        CCPoint impact;
        float radius = 0.f;
        float t = 0.f;
    };
    Strike m_strike;
    hazard::Removed m_removed;
    MissileNode* m_node = nullptr;   // child of m_objectLayer
};

} // namespace

std::unique_ptr<Augment> makeMissile() { return std::make_unique<Missile>(); }

} // namespace augment
