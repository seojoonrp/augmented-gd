// startpos (스타트포스): Z places a checkpoint, `level` per attempt. Only the
// newest counts: a death respawns there once, after that the next death
// restarts from 0 unless a new one was placed. Placing also snapshots the
// other augments (shield charges, brake time) for the respawn.
//
// Our own ref decides where to respawn, not GD's m_checkpointArray: GD drops
// a checkpoint placed < 0.1 s before a death (removePlacedCheckpoint), so the
// array gets patched right before the respawn, and the respawn itself borrows
// the practice-mode path for that one resetLevel() call.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Language.hpp"
#include "../ui/ProgressMarks.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class StartPos : public Augment {
public:
    StartPos() : Augment({ ids::StartPos }) {}

    // a death we come back from doesn't end the life, so the gauge isn't charged for it
    bool onDeath(LevelSession& s) override {
        if (!s.owns(ids::StartPos)) return false;
        m_respawnPending = m_checkpoint != nullptr;
        return m_respawnPending;
    }

    bool onBeforeReset(LevelSession& s) override {
        auto layer = s.layer();
        bool fromCheckpoint = m_respawnPending && m_checkpoint && s.runAttempt();
        m_respawnPending = false;
        m_lastRespawn = nullptr;

        if (fromCheckpoint) {
            m_target = m_checkpoint;
            m_checkpoint = nullptr;
            m_percents.clear();

            // Practice respawn path for this one reset. It uses
            // m_currentCheckpoint / the last array entry, so point both at
            // the target. onAttemptStart puts the practice flag back.
            this->syncGdArray(s, m_target);
            layer->m_currentCheckpoint = m_target;
            m_wasPractice = layer->m_isPracticeMode;
            layer->m_isPracticeMode = true;
            return true;
        }

        // Fresh attempt. A null current checkpoint makes GD start from the
        // start (same trick as qolmod's StartposSwitcher). Practice on a run
        // level keeps the player's own checkpoints.
        m_placed = 0;
        m_checkpoint = nullptr;
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
        s.notice(tr("Back at the checkpoint.", "체크포인트에서 부활합니다."));
    }

    // every frame: ProgressMarks only redraws on change, and the bar can attach late
    void onFrame(LevelSession& s, float) override {
        if (auto marks = s.marks()) marks->setCheckpoints(m_percents);
    }

    bool onHotkey(LevelSession& s, Hotkey which, bool down) override {
        if (which != Hotkey::Checkpoint || !down) return false;
        return this->tryPlace(s);
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::StartPos);
        return fmt::format("Lv{}  {}/{} placed, {}  [Z]", lvl, m_placed, lvl, m_checkpoint ? "READY" : "none");
    }

private:
    int gdCount(LevelSession& s) const {
        auto arr = s.layer()->m_checkpointArray;
        return arr ? static_cast<int>(arr->count()) : 0;
    }

    CheckpointObject* gdLast(LevelSession& s) const {
        // getLastCheckpoint() is inline and doesn't null-check the array
        return s.layer()->m_checkpointArray ? s.layer()->getLastCheckpoint() : nullptr;
    }

    // if GD dropped our placement on the death, rebuild the array as just the target
    void syncGdArray(LevelSession& s, CheckpointObject* target) {
        auto layer = s.layer();
        int count = this->gdCount(s);
        if (this->gdLast(s) == target) return;

        if (count > 0) layer->removeAllCheckpoints();
        layer->storeCheckpoint(target);
    }

    // A respawn uses the checkpoint up. removeCheckpoint(false) pops the
    // newest entry (GD's own removePlacedCheckpoint does the same). Keep a ref
    // in m_lastRespawn since GD may still point at it this attempt.
    void consume(LevelSession& s, CheckpointObject* target) {
        m_lastRespawn = target;
        if (this->gdLast(s) == target) s.layer()->removeCheckpoint(false);
    }

    // true = key consumed, even when out of placements or GD refuses
    bool tryPlace(LevelSession& s) {
        auto layer = s.layer();
        int lvl = s.levelOf(ids::StartPos);
        bool dead = !layer->m_player1 || layer->m_player1->m_isDead;
        if (!s.runAttempt() || layer->m_isPaused || lvl == 0 || dead) return false;

        if (m_placed >= lvl) return true;

        bool wasPractice = layer->m_isPracticeMode;
        layer->m_isPracticeMode = true;
        auto cp = layer->markCheckpoint();
        layer->m_isPracticeMode = wasPractice;

        if (cp) {
            m_placed++;
            m_checkpoint = cp;
            m_percents.assign(1, s.percent());
            s.onCheckpointPlaced();
            s.notice(tr("Checkpoint placed.", "체크포인트가 설정되었습니다."));
        }
        else {
            // GD's own conditions (e.g. mid-dash); nothing on screen says so
            log::info("StartPos: GD refused the checkpoint at {:.1f}%", s.percent());
        }
        return true;
    }

    int m_placed = 0;   // this attempt's budget
    Ref<CheckpointObject> m_checkpoint;   // the one live placement
    std::vector<float> m_percents;   // 0 or 1 entries, for the progress bar dot
    bool m_respawnPending = false;
    // between onBeforeReset and onAttemptStart of a respawn
    Ref<CheckpointObject> m_target;
    bool m_wasPractice = false;
    Ref<CheckpointObject> m_lastRespawn;
};

} // namespace

std::unique_ptr<Augment> makeStartPos() { return std::make_unique<StartPos>(); }

} // namespace augment
