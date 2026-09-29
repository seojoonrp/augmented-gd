// shield (결계인가?): `level` hits per attempt are absorbed; each absorbed hit
// opens a short noclip window so the player gets clear of what killed them.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
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
        // Charges are derived from (level - used) so a shield drafted mid-run
        // is usable in the very next attempt.
        if (!fromCheckpoint) m_used = 0;
        if (m_node) m_node->reset();
    }

    // A checkpoint remembers the charges left when it was placed; the
    // respawn brings them back (the hit that killed us is undone with it).
    void onCheckpointPlaced(LevelSession&) override { m_savedUsed = m_used; }
    void onCheckpointRespawn(LevelSession& s) override {
        m_used = m_savedUsed;
        if (m_node) m_node->reset();
        log::info("Shield: restored from checkpoint, {} left", s.levelOf(ids::Shield) - m_used);
    }

    bool onHit(LevelSession& s, PlayerObject*, GameObject*) override {
        // Still inside the noclip window -> ignore the hit entirely.
        if (m_noclipTimer > 0.f) return true;

        int shields = s.levelOf(ids::Shield) - m_used;
        if (shields <= 0) return false;
        m_used++;
        m_noclipTimer = tune::NoclipSeconds;
        log::info("Shield broke ({} left), noclip for {}s", shields - 1, tune::NoclipSeconds);
        s.notice("보호막이 깨졌습니다.");
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
    // Noclip window: the icon pulses 100 % -> 50 % -> 100 %. Re-set every
    // frame (unverified whether GD touches player opacity on its own), and
    // put back to what it was before once the window ends.
    void blink(LevelSession& s) {
        auto layer = s.layer();
        PlayerObject* players[2] = { layer->m_player1, layer->m_gameState.m_isDualMode ? layer->m_player2 : nullptr };
        if (!m_blinking) {
            for (int i = 0; i < 2; ++i) {
                m_blinked[i] = players[i] != nullptr;
                if (players[i]) m_savedOpacity[i] = players[i]->getOpacity();
            }
            m_blinking = true;
            log::info("Shield: noclip blink on (opacity {})", m_savedOpacity[0]);
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
        log::info("Shield: noclip blink off");
    }

    void drawBubble(LevelSession& s, float dt) {
        if (!s.owns(ids::Shield)) return;
        auto layer = s.layer();
        if (!m_node && layer->m_objectLayer) {
            m_node = ShieldNode::create();
            layer->m_objectLayer->addChild(m_node, 999);
            log::info("Shield: bubble node added to the object layer");
        }
        if (!m_node) return;

        // One per live player, two at most (dual).
        ShieldNode::Bubble bubbles[2];
        std::size_t count = 0;
        auto wrap = [&](PlayerObject* p) {
            if (!p || p->m_isDead) return;
            bubbles[count++] = { p->getPosition(), kBubbleRadius * p->m_vehicleSize };
        };
        wrap(layer->m_player1);
        if (layer->m_gameState.m_isDualMode) wrap(layer->m_player2);

        // A charge is ready and we're not inside the post-hit noclip window;
        // with charges left the bubble comes back when the window ends.
        bool up = s.runAttempt() && m_noclipTimer <= 0.f && s.levelOf(ids::Shield) - m_used > 0;
        m_node->tick(dt, { bubbles, count }, up);
    }

    // Around a normal-size icon (30 units); scaled by m_vehicleSize for mini.
    static constexpr float kBubbleRadius = 25.f;
    static constexpr float kBlinkPeriod = 0.3f;

    int m_used = 0;
    int m_savedUsed = 0;
    float m_noclipTimer = 0.f;
    bool m_blinking = false;
    bool m_blinked[2] = { false, false };
    GLubyte m_savedOpacity[2] = { 255, 255 };
    // Child of m_objectLayer; dies with the level.
    ShieldNode* m_node = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeShield() { return std::make_unique<Shield>(); }

} // namespace augment
