// berserker (버서커): every hazard an augment destroys rolls berserkChance()
// to open a berserkSeconds() window; while it is open, hitting a hazard
// destroys that hazard instead of killing the player. The rolls come from the
// session's onHazardsDestroyed fan-out, so the cat's sweeps and the missile's
// blasts feed it — and so does berserk's own smash, which rolls again and can
// refresh the window. Solids, slopes and a death GD names no object for are
// not hazards and still kill (hazard::isTarget, the same test the cat and the
// missile target by); removal and put-back are hazard:: (HazardRemoval.hpp),
// the same path as both of them.
//
// Note (design): with neither the cat nor the missile owned nothing destroys
// hazards, so nothing ever rolls and the augment is inert — the HUD row says
// so. Berserker is a synergy pick by construction.

#include "Augments.hpp"
#include "HazardRemoval.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../hooks/HazardHitboxHook.hpp"
#include "../ui/BerserkAura.hpp"
#include "../ui/BerserkNode.hpp"

#include <Geode/Geode.hpp>

#include <random>

using namespace geode::prelude;

namespace augment {

namespace {

class Berserk : public Augment {
public:
    Berserk() : Augment({ ids::Berserk }) {}

    // Asked before the shield, so an open window smashes the hazard for free
    // instead of the shield spending a charge on it.
    int hitPriority() const override { return 1; }

    void onLevelReady(LevelSession& s) override { this->ensureNode(s); }

    void onAttemptStart(LevelSession&, bool) override {
        m_left = 0.f;
        m_smashed = 0;
        m_rollLogged = false;
        if (m_aura) m_aura->reset();
    }

    // Close the window and put back everything this attempt's smashes took.
    // Runs before GD's own reset so any per-object reset GD does still gets
    // the last word.
    bool onBeforeReset(LevelSession&) override {
        if (m_left > 0.f) log::info("Berserk: window ({:.2f}s left) closed by the reset", m_left);
        m_left = 0.f;
        if (m_node) m_node->setWindow(0.f, 0.f);
        if (m_aura) m_aura->reset();
        if (m_removed.empty()) return false;
        auto count = m_removed.size();
        int stillDisabled = m_removed.restore();
        log::info("Berserk: restored {} hazards ({} were still disabled)", count, stillDisabled);
        return false;
    }

    // One roll per destroyed hazard; the first hit opens (or refreshes) the
    // window and the rest of the batch is moot.
    void onHazardsDestroyed(LevelSession& s, int count) override {
        if (!s.owns(ids::Berserk) || !s.runAttempt()) return;
        float chance = s.mgr().berserkChance();
        if (chance <= 0.f) return;
        if (!m_rollLogged) {
            log::info("Berserk: rolling {} x {:.0f}% (first destruction this attempt)", count, chance * 100.f);
            m_rollLogged = true;
        }
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> roll(0.f, 1.f);
        for (int i = 0; i < count; i++) {
            if (roll(rng) < chance) {
                this->enter(s, count);
                return;
            }
        }
    }

    bool onHit(LevelSession& s, PlayerObject*, GameObject* object) override {
        if (m_left <= 0.f) return false;
        // GD names no object for some deaths (qolmod guards the same way);
        // nothing to smash, so the death stands.
        if (!object) {
            log::info("Berserk: death with no object at {:.1f}%, not swallowed", s.percent());
            return false;
        }
        if (!hazard::isTarget(object)) {
            log::info("Berserk: killed by a non-hazard at {:.1f}%, not swallowed", s.percent());
            return false;
        }
        // Already gone (a second call for the same object): swallow, but
        // don't record it twice.
        if (object->m_isDisabled || object->m_isDisabled2) {
            log::info("Berserk: hit an already smashed hazard at {:.1f}%", s.percent());
            return true;
        }

        m_removed.take(object);
        m_smashed++;
        if (m_node) m_node->smashAt(object);
        log::info(
            "Berserk: smashed a hazard at {:.1f}% ({} this attempt, {:.2f}s left)",
            s.percent(), m_smashed, m_left
        );
        // A smash is a destroyed hazard like any other, so it rolls too and
        // can refresh the window.
        s.onHazardsDestroyed(1);
        return true;
    }

    void onFrame(LevelSession& s, float dt) override {
        if (!s.owns(ids::Berserk)) return;
        this->ensureNode(s);
        auto layer = s.layer();
        // Paused: leave the picture exactly as it was, like the cat and the
        // missile do.
        if (layer->m_isPaused) return;

        // The window only runs down while the attempt is really being played;
        // the pictures still animate after a death so nothing freezes on
        // screen through GD's respawn delay (they are told the window is shut).
        bool live = s.runAttempt() && layer->m_player1 && !layer->m_player1->m_isDead;
        if (live && m_left > 0.f) {
            m_left -= dt;
            if (m_left <= 0.f) {
                m_left = 0.f;
                log::info("Berserk: window over at {:.1f}% ({} smashed)", s.percent(), m_smashed);
            }
        }
        if (m_node) {
            m_node->setWindow(live ? m_left : 0.f, s.mgr().berserkSeconds());
            m_node->tick(dt);
        }
        this->tickAura(s, dt, live);
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::Berserk);
        if (m_left > 0.f) return fmt::format("Lv{}  BERSERK {:.1f}s, smashed {}", lvl, m_left, m_smashed);
        // Nothing destroys hazards without the cat or the missile, so nothing
        // ever rolls: say so rather than showing a chance that cannot fire.
        char const* source = (s.owns(ids::Cat) || s.owns(ids::Missile)) ? "" : ", needs cat/missile";
        return fmt::format(
            "Lv{}  {:.0f}% per hazard, {:.1f}s, smashed {}{}",
            lvl, s.mgr().berserkChance() * 100.f, s.mgr().berserkSeconds(), m_smashed, source
        );
    }

private:
    void ensureNode(LevelSession& s) {
        if (m_node || !s.owns(ids::Berserk)) return;
        auto layer = s.layer();
        if (!layer->m_uiLayer) return;
        m_node = BerserkNode::create();
        layer->m_uiLayer->addChild(m_node, 998);
        log::info("Berserk: node added to the UI layer");
    }

    // The fire goes in the object layer (world space, like ShieldNode) one z
    // order below the player, so the icon stays drawn on top of it.
    // (unverified: the player's z is read at creation time; if the flame ends
    // up hidden behind level blocks, that z is the thing to tune.)
    void ensureAura(LevelSession& s) {
        if (m_aura || !s.owns(ids::Berserk)) return;
        auto layer = s.layer();
        if (!layer->m_objectLayer || !layer->m_player1) return;
        int playerZ = layer->m_player1->getZOrder();
        m_aura = BerserkAura::create();
        layer->m_objectLayer->addChild(m_aura, playerZ - 1);
        log::info("Berserk: aura added to the object layer at z {} (player 1 is at z {})", playerZ - 1, playerZ);
    }

    // Feeds the aura one entry per live player, and whether the window is open.
    void tickAura(LevelSession& s, float dt, bool live) {
        this->ensureAura(s);
        if (!m_aura) return;
        auto layer = s.layer();

        std::vector<BerserkAura::Flame> flames;
        auto add = [&](PlayerObject* p) {
            if (!p || p->m_isDead) return;
            flames.push_back({ p->getPosition(), kAuraSize * p->m_vehicleSize, p->m_isGoingLeft });
        };
        add(layer->m_player1);
        if (layer->m_gameState.m_isDualMode) add(layer->m_player2);

        m_aura->tick(dt, flames, live && m_left > 0.f);
    }

    // Opens the window, or refreshes it when one is already running.
    void enter(LevelSession& s, int ofCount) {
        float seconds = s.mgr().berserkSeconds();
        bool again = m_left > 0.f;
        m_left = seconds;
        if (m_node) m_node->setWindow(m_left, seconds);
        log::info(
            "Berserk: {} for {:.1f}s at {:.1f}% (rolled {:.0f}% on 1 of {} destroyed)",
            again ? "refreshed" : "triggered", seconds, s.percent(), s.mgr().berserkChance() * 100.f, ofCount
        );
        // Only entering the mode is announced, above the draft gauge rather
        // than in the notice corner (user, 2026-09-29); a refresh is not.
        if (!again) s.banner("버서커!");
    }

    // The flame's reach around a normal-size icon (30 units); scaled by
    // m_vehicleSize for mini, the way ShieldNode sizes its bubble.
    static constexpr float kAuraSize = 17.f;

    // Seconds left of the window (game seconds: the dt is time-scaled, like
    // the cat's and the missile's timers), smashes this attempt, and what
    // they took.
    float m_left = 0.f;
    int m_smashed = 0;
    bool m_rollLogged = false;
    hazard::Removed m_removed;
    // Children of the level's layers; both die with the level.
    BerserkNode* m_node = nullptr;
    BerserkAura* m_aura = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeBerserk() { return std::make_unique<Berserk>(); }

} // namespace augment
