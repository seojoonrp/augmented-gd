#pragma once

// Everything that lives as long as one PlayLayer of a run level: the augment
// behaviours, the HUD handle, the shared object index, per-attempt bookkeeping
// the hook needs. Created by the PlayLayer hook after init, dropped in onQuit;
// reached through AugmentManager::get().session(), which also checks that the
// layer is still the live one, so popup callbacks and key listeners never hold
// a PlayLayer pointer of their own (CLAUDE.md rule 5).

#include "../augments/Augment.hpp"
#include "../ui/ProgressMarks.hpp"

#include <Geode/Geode.hpp>

#include <memory>
#include <string>
#include <vector>

class PlayLayer;

namespace augment {

class AugmentManager;
class RunHud;

class LevelSession {
public:
    // Made before PlayLayer::init (so objects created inside it are seen);
    // `levelID` because m_level is not set yet at that point.
    LevelSession(PlayLayer* layer, int levelID);
    ~LevelSession();

    PlayLayer* layer() const { return m_layer; }
    AugmentManager& mgr() const;
    RunHud* hud() const { return m_hud; }
    void setHud(RunHud* hud) { m_hud = hud; }
    // Best / checkpoint dots on GD's progress bar; null until the bar exists
    // (setupHasCompleted for online levels).
    ProgressMarks* marks() const;
    void setMarks(ProgressMarks* marks);
    // Gauge, header, best mark and one row per owned augment. Text is
    // rebuilt at most kHudRate times a second unless `force`d (the gauge
    // fill itself eases every frame inside RunHud).
    void refreshHud(bool force = false);
    // Advances the refresh clock; the hook calls this once per frame.
    void tickHud(float dt);

    // The run is still on for this level (it ends at levelComplete while the
    // layer lives on).
    bool runLevel() const;
    // Normal-mode only: practice/test attempts don't count and get no augments.
    bool runAttempt() const;
    float percent() const;
    // 0..1; what nerve scales the hitbox shrinks by.
    float progress() const;
    int levelOf(std::string const& id) const;
    bool owns(std::string const& id) const { return this->levelOf(id) > 0; }
    // Centre-screen message via the HUD (no-op without one).
    void notice(std::string const& text, cocos2d::ccColor3B color);

    // Non-decoration objects sorted by x, built on first use (foresight, cat).
    std::vector<GameObject*>& objectsByX();

    // --- fan-out to the augments (see Augment.hpp for when each fires) ---
    void onLevelInit();
    void onLevelReady();
    void onObjectAdded(GameObject* obj);
    void onQuit();
    // True when an augment turns this reset into a respawn (same attempt).
    bool onBeforeReset();
    // A checkpoint respawn also fans out onCheckpointRespawn afterwards.
    void onAttemptStart(bool fromCheckpoint);
    void onCheckpointPlaced();
    bool onHit(PlayerObject* player);
    void onDeath();
    void onFrame(float dt);
    void onPause();
    void onGranted(std::string const& id, int level);
    bool onHotkey(Hotkey which, bool down);
    Augment* find(std::string const& id) const;

    // --- death bookkeeping (destroyPlayer can fire more than once per attempt) ---
    // Counts the death once per attempt; returns false when already counted.
    bool countDeath();

    // --- debug keys ---
    // Grants one level of the index-th augment in the table with the same
    // side effects a pick has. Returns true when the key was consumed.
    bool debugGrant(int index);
    // Tops the gauge up so the next death drafts.
    bool debugFillGauge();

private:
    PlayLayer* m_layer;
    int m_levelID;
    RunHud* m_hud = nullptr;
    geode::Ref<ProgressMarks> m_marks;
    std::vector<std::unique_ptr<Augment>> m_augments;
    std::vector<GameObject*> m_objectsByX;
    bool m_deathCounted = false;
    float m_hudClock = 1.f;   // seconds since the last text rebuild; starts due
};

} // namespace augment
