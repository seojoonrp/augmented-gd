// berserker (버서커): each hazard an augment destroys rolls berserkChance() to
// open a berserkSeconds() window; while it's open, running into a hazard
// smashes it instead of killing you. Without cat or missile nothing ever rolls.

#include "Augments.hpp"
#include "HazardRemoval.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"
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

    int hitPriority() const override { return 1; }

    void onLevelReady(LevelSession& s) override { this->ensureNode(s); }

    void onAttemptStart(LevelSession&, bool) override {
        m_left = 0.f;
        m_smashed = 0;
        if (m_aura) m_aura->reset();
    }

    // before GD's reset, so GD's own per-object reset still gets the last word
    bool onBeforeReset(LevelSession&) override {
        m_left = 0.f;
        if (m_node) m_node->setWindow(0.f, 0.f);
        if (m_aura) m_aura->reset();
        m_removed.restore();
        return false;
    }

    // one roll per destroyed hazard, first success wins
    void onHazardsDestroyed(LevelSession& s, int count) override {
        if (!s.owns(ids::Berserk) || !s.runAttempt()) return;
        float chance = s.mgr().berserkChance();
        if (chance <= 0.f) return;
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> roll(0.f, 1.f);
        for (int i = 0; i < count; i++) {
            if (roll(rng) < chance) {
                this->enter(s);
                return;
            }
        }
    }

    bool onHit(LevelSession& s, PlayerObject*, GameObject* object) override {
        if (m_left <= 0.f) return false;
        // no object (GD names none for some deaths) or not a hazard: the death stands
        if (!object || !hazard::isTarget(object)) return false;
        // second call for an object we already smashed
        if (object->m_isDisabled || object->m_isDisabled2) return true;

        m_removed.take(object);
        m_smashed++;
        if (m_node) m_node->smashAt(object);
        // a smash counts as a destroyed hazard too, so it can refresh the window
        s.onHazardsDestroyed(1);
        return true;
    }

    void onFrame(LevelSession& s, float dt) override {
        if (!s.owns(ids::Berserk)) return;
        this->ensureNode(s);
        auto layer = s.layer();
        if (layer->m_isPaused) return;

        // The timer only runs while actually playing, but the effects keep
        // animating through the respawn delay (told the window is shut) so
        // nothing freezes on screen.
        bool live = s.runAttempt() && layer->m_player1 && !layer->m_player1->m_isDead;
        if (live && m_left > 0.f) {
            m_left -= dt;
            if (m_left <= 0.f) m_left = 0.f;
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
    }

    // object layer, one z below the player so the icon draws over the fire
    void ensureAura(LevelSession& s) {
        if (m_aura || !s.owns(ids::Berserk)) return;
        auto layer = s.layer();
        if (!layer->m_objectLayer || !layer->m_player1) return;
        int playerZ = layer->m_player1->getZOrder();
        m_aura = BerserkAura::create();
        layer->m_objectLayer->addChild(m_aura, playerZ - 1);
    }

    void tickAura(LevelSession& s, float dt, bool live) {
        this->ensureAura(s);
        if (!m_aura) return;
        auto layer = s.layer();

        BerserkAura::Flame flames[2];
        std::size_t count = 0;
        auto add = [&](PlayerObject* p) {
            if (!p || p->m_isDead) return;
            flames[count++] = { p->getPosition(), kAuraSize * p->m_vehicleSize, p->m_isGoingLeft };
        };
        add(layer->m_player1);
        if (layer->m_gameState.m_isDualMode) add(layer->m_player2);

        m_aura->tick(dt, { flames, count }, live && m_left > 0.f);
    }

    // opens the window, or refreshes it if already open
    void enter(LevelSession& s) {
        float seconds = s.mgr().berserkSeconds();
        bool again = m_left > 0.f;
        m_left = seconds;
        if (m_node) m_node->setWindow(m_left, seconds);
        if (!again) s.banner(tr("BERSERK!", "버서커!"));   // no banner on refresh
    }

    // flame reach around a normal-size icon, times m_vehicleSize for mini
    static constexpr float kAuraSize = 17.f;

    float m_left = 0.f;   // game seconds (dt is time-scaled)
    int m_smashed = 0;
    hazard::Removed m_removed;
    // children of the level's layers, die with it
    BerserkNode* m_node = nullptr;
    BerserkAura* m_aura = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeBerserk() { return std::make_unique<Berserk>(); }

} // namespace augment
