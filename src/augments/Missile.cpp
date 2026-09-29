// missile (공습): every missileInterval() seconds of play a missile drops on
// a random hazard that is on screen and ahead of the player; when it lands
// (MissileNode::FallSeconds later) every hazard whose collision shape touches
// the blast circle (missileRadius()) is removed for the rest of the attempt.
// Aiming at a hazard rather than a random point means a strike always hits
// something; while nothing is in view the strike stays armed and fires as
// soon as a target scrolls in. Removal and put-back are hazard::
// (HazardRemoval.hpp), the same path as the cat.

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

// Targets closer than this (object units ahead of the player) are skipped.
// 150 was enough to land before the player arrived, but the user found it
// pointless ("gone as I pass it"), so it is ~1 s of travel at 1x speed:
// the strike clears what is coming up, not what is at the player's feet.
constexpr float kLead = 300.f;
// Objects further than this from the blast centre in x are not even tested
// (big hazards can still reach in from beyond the radius).
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
            log::info("Missile: node added to the object layer");
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
        if (this->launch(s)) {
            m_timer = 0.f;
            m_waitingLogged = false;
        }
        else if (!m_waitingLogged) {
            log::info("Missile: armed, no hazard in view at {:.1f}% - waiting", s.percent());
            m_waitingLogged = true;
        }
    }

    // Put back everything the strikes took this attempt; drop a missile
    // still in the air. Runs before GD's own reset so any per-object reset
    // GD does still gets the last word.
    bool onBeforeReset(LevelSession&) override {
        m_timer = 0.f;
        m_waitingLogged = false;
        if (m_strike.active) {
            m_strike = {};
            if (m_node) m_node->cancel();
            log::info("Missile: strike in flight cancelled by the reset");
        }
        else if (m_node) m_node->cancel();
        if (m_removed.empty()) return false;
        auto count = m_removed.size();
        int stillDisabled = m_removed.restore();
        log::info("Missile: restored {} hazards ({} were still disabled)", count, stillDisabled);
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
    // Pick the target and start the drop. False when nothing is in view.
    bool launch(LevelSession& s) {
        auto layer = s.layer();
        if (!layer->m_objectLayer || !layer->m_player1) return false;

        auto view = hazard::viewAhead(layer, kLead);
        auto candidates = hazard::hazardsInView(s, view);
        if (candidates.empty()) return false;

        static std::mt19937 rng{ std::random_device{}() };
        auto target = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];

        // Impact = the target's position in object-layer space; the drop
        // starts above the top edge of the screen at that x.
        CCNode* parent = target->getParent() ? target->getParent() : layer->m_objectLayer;
        CCPoint world = parent->convertToWorldSpace({ target->getPositionX(), target->getPositionY() });
        CCPoint impact = layer->m_objectLayer->convertToNodeSpace(world);
        auto win = CCDirector::get()->getWinSize();
        CCPoint from = layer->m_objectLayer->convertToNodeSpace({ world.x, win.height + 40.f });

        m_strike = { true, impact, s.mgr().missileRadius(), 0.f };
        if (m_node) m_node->launch(from, impact, m_strike.radius);
        log::info(
            "Missile: launched at ({:.0f}, {:.0f}) r{:.0f}, 1 of {} hazards in view at {:.1f}% (scan x {:.0f}..{:.0f})",
            impact.x, impact.y, m_strike.radius, candidates.size(), s.percent(), view.lo, view.hi
        );
        return true;
    }

    // The missile landed: remove every live hazard touching the blast.
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
        log::info(
            "Missile: impact at ({:.0f}, {:.0f}) r{:.0f} removed {} hazards at {:.1f}% ({} removed this attempt)",
            centre.x, centre.y, radius, removed, s.percent(), m_removed.size()
        );
        s.onHazardsDestroyed(removed);
    }

    // Seconds since the last launch (not counted while a missile is in the
    // air), the strike in flight, and what this attempt's strikes removed.
    float m_timer = 0.f;
    bool m_waitingLogged = false;
    struct Strike {
        bool active = false;
        CCPoint impact;
        float radius = 0.f;
        float t = 0.f;
    };
    Strike m_strike;
    hazard::Removed m_removed;
    // Child of m_objectLayer; dies with the level.
    MissileNode* m_node = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeMissile() { return std::make_unique<Missile>(); }

} // namespace augment
