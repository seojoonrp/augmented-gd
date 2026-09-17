// startpos (스타트포스): `level` checkpoint placements per attempt (a life
// from 0 %), each one good for one respawn, newest first; with none left the
// next death restarts from 0 and the budget refills. Z places.
//
// GD's checkpoint internals (docs/GD-INTERNALS.md "Checkpoints in normal
// mode"): our own vector of refs decides where to respawn; GD's
// m_checkpointArray is rebuilt from it right before a respawn if GD dropped
// entries (removePlacedCheckpoint deletes a checkpoint placed < 0.1 s before
// the death), and the practice-mode respawn path is borrowed for exactly the
// one PlayLayer::resetLevel() call that respawns.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../ui/ProgressMarks.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class StartPos : public Augment {
public:
    StartPos() : Augment({ ids::StartPos }) {}

    // Any unused placement means we come back to the newest one. Our own
    // refs decide, not GD's array.
    void onDeath(LevelSession& s) override {
        if (!s.owns(ids::StartPos)) return;
        m_respawnPending = !m_checkpoints.empty();
        log::info(
            "Checkpoint: death with {}/{} placed, {} ready, GD array {} -> {}",
            m_placed, s.levelOf(ids::StartPos), m_checkpoints.size(),
            this->gdCount(s), m_respawnPending ? "respawn" : "restart from 0"
        );
    }

    // Decides the kind of reset and points GD at the right checkpoint (or
    // none) before its resetLevel runs.
    bool onBeforeReset(LevelSession& s) override {
        auto layer = s.layer();
        bool fromCheckpoint = m_respawnPending && !m_checkpoints.empty() && s.runAttempt();
        if (m_respawnPending && !fromCheckpoint) {
            log::info("Checkpoint: respawn dropped (ready {}, runAttempt {})", m_checkpoints.size(), s.runAttempt());
        }
        m_respawnPending = false;
        m_lastRespawn = nullptr;

        if (fromCheckpoint) {
            m_target = m_checkpoints.back();
            m_checkpoints.pop_back();
            if (!m_percents.empty()) m_percents.pop_back();

            // Borrow the practice-mode respawn path for exactly this reset.
            // GD respawns at m_currentCheckpoint / the last array entry, so
            // both are pointed at the target first; onAttemptStart undoes the
            // practice flag right after GD's reset.
            this->syncGdArray(s, m_target);
            layer->m_currentCheckpoint = m_target;
            m_wasPractice = layer->m_isPracticeMode;
            layer->m_isPracticeMode = true;
            return true;
        }

        // Fresh attempt: refill everything. Same as qolmod's StartposSwitcher:
        // a null current checkpoint makes GD start from the start position.
        // Practice mode on a run level keeps the player's own checkpoints.
        m_placed = 0;
        m_checkpoints.clear();
        m_percents.clear();
        if (s.runAttempt()) {
            layer->m_currentCheckpoint = nullptr;
            if (this->gdCount(s) > 0) layer->removeAllCheckpoints();
        }
        return false;
    }

    void onAttemptStart(LevelSession& s, bool fromCheckpoint) override {
        if (!fromCheckpoint || !m_target) return;
        s.layer()->m_isPracticeMode = m_wasPractice;
        this->consume(s, m_target);
        m_target = nullptr;
        log::info(
            "Respawned from checkpoint ({} ready, {}/{} placed this attempt)",
            m_checkpoints.size(), m_placed, s.levelOf(ids::StartPos)
        );
        s.notice("CHECKPOINT", { 120, 255, 120 });
    }

    // The dots on GD's progress bar follow our placements; ProgressMarks only
    // redraws when the list changes, and the bar may attach late.
    void onFrame(LevelSession& s, float) override {
        if (auto marks = s.marks()) marks->setCheckpoints(m_percents);
    }

    bool onHotkey(LevelSession& s, Hotkey which) override {
        if (which != Hotkey::Checkpoint) return false;
        return this->tryPlace(s);
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::StartPos);
        return fmt::format("Lv{}  {}/{} placed, {} ready  [Z]", lvl, m_placed, lvl, m_checkpoints.size());
    }

private:
    int gdCount(LevelSession& s) const {
        auto arr = s.layer()->m_checkpointArray;
        return arr ? static_cast<int>(arr->count()) : 0;
    }

    CheckpointObject* gdLast(LevelSession& s) const {
        // getLastCheckpoint() is inline and dereferences the array unguarded.
        return s.layer()->m_checkpointArray ? s.layer()->getLastCheckpoint() : nullptr;
    }

    // GD respawns at the last entry of m_checkpointArray. Normally that
    // already is `target` (GD kept our placements). If GD dropped them on
    // the normal-mode death, rebuild the array from our refs so the older
    // checkpoints stay available for later respawns too.
    void syncGdArray(LevelSession& s, CheckpointObject* target) {
        auto layer = s.layer();
        int count = this->gdCount(s);
        bool inSync = count == static_cast<int>(m_checkpoints.size()) + 1 && this->gdLast(s) == target;
        if (inSync) return;

        if (count > 0) layer->removeAllCheckpoints();
        for (auto& cp : m_checkpoints) layer->storeCheckpoint(cp);
        layer->storeCheckpoint(target);
        log::info("Checkpoint: rebuilt GD array ({} -> {} entries)", count, this->gdCount(s));
    }

    // A respawn uses its checkpoint up. removeCheckpoint(false) drops the
    // newest entry — it is what GD itself calls (removePlacedCheckpoint) to
    // undo a checkpoint placed right before a death — so the next death goes
    // to the previous one. The object stays alive in m_lastRespawn because
    // GD may still point at it this attempt.
    void consume(LevelSession& s, CheckpointObject* target) {
        m_lastRespawn = target;
        int before = this->gdCount(s);
        if (this->gdLast(s) == target) s.layer()->removeCheckpoint(false);
        log::info(
            "Checkpoint: consumed (GD array {} -> {}{})",
            before, this->gdCount(s), this->gdLast(s) == target ? ", still on top!" : ""
        );
    }

    // Returns true when the key was consumed (also when it only showed a
    // "none left" notice: the player has the augment, so Z is ours).
    bool tryPlace(LevelSession& s) {
        auto layer = s.layer();
        int lvl = s.levelOf(ids::StartPos);
        bool dead = !layer->m_player1 || layer->m_player1->m_isDead;
        if (!s.runAttempt() || layer->m_isPaused || lvl == 0 || dead) {
            log::info("Z ignored: runAttempt={} paused={} cpLv={} dead={}", s.runAttempt(), layer->m_isPaused, lvl, dead);
            return false;
        }

        if (m_placed >= lvl) {
            s.notice("NO CHECKPOINTS LEFT", { 255, 120, 120 });
            return true;
        }

        bool wasPractice = layer->m_isPracticeMode;
        layer->m_isPracticeMode = true;
        auto cp = layer->markCheckpoint();
        layer->m_isPracticeMode = wasPractice;

        if (cp) {
            m_placed++;
            m_checkpoints.push_back(cp);
            m_percents.push_back(s.percent());
            log::info("Checkpoint placed ({}/{}), {} ready, GD array {}", m_placed, lvl, m_checkpoints.size(), this->gdCount(s));
            s.notice("CHECKPOINT PLACED", { 120, 255, 120 });
        }
        else {
            // GD refused (its own conditions, e.g. mid-dash). Say so, or the
            // player believes a checkpoint exists.
            log::info("markCheckpoint returned null at {:.1f}%", s.percent());
            s.notice("CAN'T PLACE HERE", { 255, 120, 120 });
        }
        return true;
    }

    // Placed this attempt (the budget), and those not yet used, oldest first.
    int m_placed = 0;
    std::vector<Ref<CheckpointObject>> m_checkpoints;
    // Percent at which each entry of m_checkpoints was placed (same order);
    // only feeds the progress-bar dots.
    std::vector<float> m_percents;
    bool m_respawnPending = false;
    // Between onBeforeReset and onAttemptStart of a respawn.
    Ref<CheckpointObject> m_target;
    bool m_wasPractice = false;
    Ref<CheckpointObject> m_lastRespawn;
};

} // namespace

std::unique_ptr<Augment> makeStartPos() { return std::make_unique<StartPos>(); }

} // namespace augment
