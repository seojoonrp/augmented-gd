// shield (결계인가?): absorbs `level` hits per attempt; each one opens a short
// noclip window so the player can get clear of what hit them.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"
#include "../ui/ShieldNode.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {

class Shield : public Augment {
public:
    Shield() : Augment({ ids::Shield }) {}

    void onAttemptStart(LevelSession& s, bool fromCheckpoint) override {
        m_noclipTimer = 0.f;
        this->stopBlink(s);
        // charges = level - used, so a shield drafted mid-run works next attempt
        if (!fromCheckpoint) m_used = 0;
        if (m_node) m_node->reset();
    }

    // respawn gives back the charges the checkpoint had
    void onCheckpointPlaced(LevelSession&) override { m_savedUsed = m_used; }
    void onCheckpointRespawn(LevelSession&) override {
        m_used = m_savedUsed;
        if (m_node) m_node->reset();
    }

    bool onHit(LevelSession& s, PlayerObject*, GameObject*) override {
        if (m_noclipTimer > 0.f) return true;

        int shields = s.levelOf(ids::Shield) - m_used;
        if (shields <= 0) return false;
        m_used++;
        m_noclipTimer = tune::NoclipSeconds;
        s.notice(tr("Your shield broke.", "보호막이 깨졌습니다."));
        if (m_node) m_node->shatter();
        return true;
    }

    void onFrame(LevelSession& s, float dt) override {
        if (m_noclipTimer > 0.f) {
            m_noclipTimer -= dt;
            if (m_noclipTimer < 0.f) m_noclipTimer = 0.f;
        }
        if (m_noclipTimer > 0.f) this->blink(s);
        else this->stopBlink(s);
        this->drawBubble(s, dt);
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::Shield);
        if (m_noclipTimer > 0.f) return fmt::format("Lv{}  NOCLIP {:.1f}s", lvl, m_noclipTimer);
        return fmt::format("Lv{}  {}/{}", lvl, lvl - m_used, lvl);
    }

private:
    // Icon pulses 100 -> 50 -> 100 % during noclip. Set every frame in case GD
    // touches player opacity itself; restored when the window ends.
    void blink(LevelSession& s) {
        auto layer = s.layer();
        PlayerObject* players[2] = { layer->m_player1, layer->m_gameState.m_isDualMode ? layer->m_player2 : nullptr };
        if (!m_blinking) {
            for (int i = 0; i < 2; ++i) {
                m_blinked[i] = players[i] != nullptr;
                if (players[i]) m_savedOpacity[i] = players[i]->getOpacity();
            }
            m_blinking = true;
        }
        float elapsed = tune::NoclipSeconds - m_noclipTimer;
        float k = 0.75f + 0.25f * std::cos(elapsed * 2.f * 3.14159265f / kBlinkPeriod);
        for (int i = 0; i < 2; ++i) {
            if (players[i] && m_blinked[i]) players[i]->setOpacity(static_cast<GLubyte>(m_savedOpacity[i] * k));
        }
    }

    void stopBlink(LevelSession& s) {
        if (!m_blinking) return;
        m_blinking = false;
        auto layer = s.layer();
        PlayerObject* players[2] = { layer->m_player1, layer->m_player2 };
        for (int i = 0; i < 2; ++i) {
            if (players[i] && m_blinked[i]) players[i]->setOpacity(m_savedOpacity[i]);
        }
    }

    void drawBubble(LevelSession& s, float dt) {
        if (!s.owns(ids::Shield)) return;
        auto layer = s.layer();
        if (!m_node && layer->m_objectLayer) {
            m_node = ShieldNode::create();
            layer->m_objectLayer->addChild(m_node, 999);
        }
        if (!m_node) return;

        ShieldNode::Bubble bubbles[2];
        std::size_t count = 0;
        auto wrap = [&](PlayerObject* p) {
            if (!p || p->m_isDead) return;
            bubbles[count++] = { p->getPosition(), kBubbleRadius * p->m_vehicleSize };
        };
        wrap(layer->m_player1);
        if (layer->m_gameState.m_isDualMode) wrap(layer->m_player2);

        // up when a charge is left and we're not in noclip
        bool up = s.runAttempt() && m_noclipTimer <= 0.f && s.levelOf(ids::Shield) - m_used > 0;
        m_node->tick(dt, { bubbles, count }, up);
    }

    // around a normal-size icon (30 units), times m_vehicleSize for mini
    static constexpr float kBubbleRadius = 25.f;
    static constexpr float kBlinkPeriod = 0.3f;

    int m_used = 0;
    int m_savedUsed = 0;
    float m_noclipTimer = 0.f;
    bool m_blinking = false;
    bool m_blinked[2] = { false, false };
    GLubyte m_savedOpacity[2] = { 255, 255 };
    ShieldNode* m_node = nullptr;   // child of m_objectLayer
};

} // namespace

std::unique_ptr<Augment> makeShield() { return std::make_unique<Shield>(); }

} // namespace augment
