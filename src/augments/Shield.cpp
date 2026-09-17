// shield (결계인가?): `level` hits per attempt are absorbed; each absorbed hit
// opens a short noclip window so the player gets clear of what killed them.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class Shield : public Augment {
public:
    Shield() : Augment({ ids::Shield }) {}

    void onAttemptStart(LevelSession&, bool fromCheckpoint) override {
        m_noclipTimer = 0.f;
        // Charges are derived from (level - used) so a shield drafted mid-run
        // is usable in the very next attempt.
        if (!fromCheckpoint) m_used = 0;
    }

    // A checkpoint remembers the charges left when it was placed; the
    // respawn brings them back (the hit that killed us is undone with it).
    void onCheckpointPlaced(LevelSession&) override { m_savedUsed = m_used; }
    void onCheckpointRespawn(LevelSession& s) override {
        m_used = m_savedUsed;
        log::info("Shield: restored from checkpoint, {} left", s.levelOf(ids::Shield) - m_used);
    }

    bool onHit(LevelSession& s, PlayerObject*) override {
        // Still inside the noclip window -> ignore the hit entirely.
        if (m_noclipTimer > 0.f) return true;

        int shields = s.levelOf(ids::Shield) - m_used;
        if (shields <= 0) return false;
        m_used++;
        m_noclipTimer = tune::NoclipSeconds;
        log::info("Shield broke ({} left), noclip for {}s", shields - 1, tune::NoclipSeconds);
        s.notice("SHIELD BROKEN", { 120, 200, 255 });
        return true;
    }

    void onFrame(LevelSession&, float dt) override {
        if (m_noclipTimer > 0.f) {
            m_noclipTimer -= dt;
            if (m_noclipTimer < 0.f) m_noclipTimer = 0.f;
        }
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::Shield);
        if (m_noclipTimer > 0.f) return fmt::format("Lv{}  NOCLIP {:.1f}s", lvl, m_noclipTimer);
        return fmt::format("Lv{}  {}/{}", lvl, lvl - m_used, lvl);
    }

private:
    int m_used = 0;
    int m_savedUsed = 0;
    float m_noclipTimer = 0.f;
};

} // namespace

std::unique_ptr<Augment> makeShield() { return std::make_unique<Shield>(); }

} // namespace augment
