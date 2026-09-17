#pragma once

// One augment's in-level behaviour. The PlayLayer hook owns the lifecycle and
// calls the LevelSession, which fans each event out to every augment in
// table order; an augment keeps its own per-attempt state and reads its
// level from the run. Every hook has an empty default, so a new augment
// overrides only what it needs and touches nothing else.
//
// Levels are per run (RunState); everything in here is per level load.

#include "../input/Hotkeys.hpp"

#include <string>
#include <vector>

class GameObject;
class PlayerObject;

namespace augment {

class LevelSession;

class Augment {
public:
    // `ids` are the augment ids this behaviour answers for (usually one;
    // HitboxScales serves hazard-hitbox, wave-hitbox and nerve together).
    explicit Augment(std::vector<std::string> ids) : m_ids(std::move(ids)) {}
    virtual ~Augment() = default;

    std::vector<std::string> const& ids() const { return m_ids; }
    bool handles(std::string const& id) const;

    // --- level ---
    // Before PlayLayer::init runs: nothing exists yet. The place to publish
    // globals that objects read as they are created.
    virtual void onLevelInit(LevelSession&) {}
    // After PlayLayer::init: objects exist, the HUD is up.
    virtual void onLevelReady(LevelSession&) {}
    // PlayLayer::addObject, run levels only.
    virtual void onObjectAdded(LevelSession&, GameObject*) {}
    // PlayLayer::onQuit, before the layer goes.
    virtual void onQuit(LevelSession&) {}

    // --- attempt ---
    // Inside resetLevel, before GD resets (the dying attempt's state is
    // still there). Return true when this reset *continues* the attempt (a
    // checkpoint respawn) instead of starting over from 0.
    virtual bool onBeforeReset(LevelSession&) { return false; }
    // Inside resetLevel, after GD reset. A checkpoint respawn is the same
    // attempt: per-attempt budgets refill only when `fromCheckpoint` is false.
    virtual void onAttemptStart(LevelSession&, bool fromCheckpoint) {}
    // destroyPlayer on a run attempt, before the death is counted. Return
    // true to swallow the hit (the original is not called).
    virtual bool onHit(LevelSession&, PlayerObject*) { return false; }
    // Player 1 really died (counted once per attempt), before GD's reset.
    virtual void onDeath(LevelSession&) {}
    // A checkpoint was placed (startpos). Snapshot whatever per-attempt
    // state should come back with the respawn (shield charges, brake time).
    // Only the newest checkpoint is ever respawned at, so one slot suffices.
    virtual void onCheckpointPlaced(LevelSession&) {}
    // Right after onAttemptStart(fromCheckpoint = true): restore the snapshot.
    virtual void onCheckpointRespawn(LevelSession&) {}
    // postUpdate, run levels only. dt is already time-scaled.
    virtual void onFrame(LevelSession&, float dt) {}
    // PlayLayer::pauseGame.
    virtual void onPause(LevelSession&) {}

    // --- run ---
    // An augment was granted (draft pick or debug key); `level` is the new
    // level. Every augment hears every grant, so cross effects (nerve →
    // hitbox scales) need no wiring.
    virtual void onGranted(LevelSession&, std::string const& id, int level) {}
    // A hotkey press (`down`) or release. Return true when consumed; most
    // augments act on the press only.
    virtual bool onHotkey(LevelSession&, Hotkey, bool down) { return false; }

    // --- HUD ---
    // Per-attempt state text for the HUD row of `id` (English; the row's
    // name is the Korean augment name). Empty is fine.
    virtual std::string hudState(LevelSession&, std::string const& id) { return ""; }

private:
    std::vector<std::string> m_ids;
};

} // namespace augment
