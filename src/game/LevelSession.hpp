#pragma once

// Everything that lives as long as one PlayLayer of a run level. Reach it via
// AugmentManager::get().session() so nothing holds a PlayLayer across frames.

#include "../augments/Augment.hpp"
#include "../core/RunState.hpp"
#include "../ui/ProgressMarks.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class PlayLayer;

namespace augment {

class AugmentManager;
class RunHud;

class LevelSession {
public:
    // made before PlayLayer::init so objects created inside it are seen;
    // m_level isn't set yet then, hence levelID
    LevelSession(PlayLayer* layer, int levelID);
    ~LevelSession();

    PlayLayer* layer() const { return m_layer; }
    AugmentManager& mgr() const;
    RunHud* hud() const { return m_hud; }
    void setHud(RunHud* hud) { m_hud = hud; }
    // dots on GD's progress bar; null until the bar exists (setupHasCompleted on online levels)
    ProgressMarks* marks() const;
    void setMarks(ProgressMarks* marks);
    // text rebuilds at most kHudRate times a second unless forced; the gauge
    // fill eases every frame inside RunHud
    void refreshHud(bool force = false);
    // once per frame
    void tickHud(float dt);

    // false after levelComplete, while the layer lives on
    bool runLevel() const;
    // normal mode only, practice/test attempts get no augments
    bool runAttempt() const;
    // set at levelComplete, read by the end screen, cleared on the next reset
    // (a replay is a normal attempt)
    bool runCleared() const { return m_runCleared; }
    void markRunCleared() { m_runCleared = true; }
    float percent() const;
    // 0..1, for nerve
    float progress() const;
    int levelOf(std::string_view id) const;
    bool owns(std::string_view id) const { return this->levelOf(id) > 0; }
    // short line in the HUD's bottom-left (no-op without a HUD)
    void notice(std::string const& text);
    // bigger line above the gauge (berserk)
    void banner(std::string const& text);
    // numbers next to the dead icon (`at`, screen space), particles into the
    // gauge, fill from `before` / `cost`; refreshes the HUD text right away
    void rewardDeath(cocos2d::CCPoint at, float before, float cost, DeathResult const& r);

    // one column of GD's section grid in object-layer x (EditorUI's inline xPosFromValue)
    static constexpr float SectionWidth = 100.f;
    // fn(obj) for every object GD files in the columns covering lo..hi, all
    // rows (callers check y). Skips decoration, the anticheat spike and the
    // player collision blocks, like qolmod's hitbox pass. GD refiles objects
    // that triggers move, so this sees them where they are now. Filed by
    // position: pad the range by how far an object reaches past it.
    template <class Fn>
    void forEachObjectInX(float lo, float hi, Fn&& fn) {
        auto const& columns = m_layer->m_sections;
        auto const& sizes = m_layer->m_sectionSizes;
        if (columns.empty() || hi < lo) return;
        if (!m_filingChecked) this->checkSectionFiling();
        int const last = static_cast<int>(columns.size()) - 1;
        int const from = std::clamp(static_cast<int>(std::floor(lo / SectionWidth)), 0, last);
        int const to = std::clamp(static_cast<int>(std::floor(hi / SectionWidth)), 0, last);
        for (int i = from; i <= to; i++) {
            auto column = columns[i];
            auto counts = i < static_cast<int>(sizes.size()) ? sizes[i] : nullptr;
            if (!column || !counts) continue;
            std::size_t const rows = std::min(column->size(), counts->size());
            for (std::size_t j = 0; j < rows; j++) {
                auto section = column->at(j);
                if (!section) continue;
                std::size_t const count = std::min(static_cast<std::size_t>(std::max(0, counts->at(j))), section->size());
                for (std::size_t k = 0; k < count; k++) {
                    auto obj = section->at(k);
                    if (obj && !this->skippedByScan(obj)) fn(obj);
                }
            }
        }
    }

    // --- fan-out to the augments ---
    void onLevelInit();
    void onLevelReady();
    void onObjectAdded(GameObject* obj);
    void onQuit();
    // true = an augment turns this reset into a respawn (same attempt)
    bool onBeforeReset();
    // also fans out onCheckpointRespawn when fromCheckpoint
    void onAttemptStart(bool fromCheckpoint);
    void onCheckpointPlaced();
    bool onHit(PlayerObject* player, GameObject* object);
    // berserker rolls on these
    void onHazardsDestroyed(int count);
    // true = a checkpoint respawn follows, so the gauge charge waits
    bool onDeath();
    void onFrame(float dt);
    void onPause();
    void onGranted(std::string const& id, int level);
    bool onHotkey(Hotkey which, bool down);
    Augment* find(std::string const& id) const;

    // destroyPlayer can fire more than once per attempt; false if already counted
    bool countDeath();

    // --- debug keys ---
    // one level of the index-th augment, with a pick's side effects. true = key consumed
    bool debugGrant(int index);
    bool debugFillGauge();

private:
    bool skippedByScan(GameObject* obj) const {
        return obj->m_objectType == GameObjectType::Decoration || obj->m_isDecoration
            || obj == m_layer->m_anticheatSpike
            || obj == m_layer->m_player1CollisionBlock || obj == m_layer->m_player2CollisionBlock;
    }
    // warns once per level if the columns aren't SectionWidth wide.
    // TODO: the width comes from editor code, confirm it holds in a PlayLayer and drop this
    void checkSectionFiling();

    PlayLayer* m_layer;
    int m_levelID;
    RunHud* m_hud = nullptr;
    geode::Ref<ProgressMarks> m_marks;
    std::vector<std::unique_ptr<Augment>> m_augments;
    bool m_filingChecked = false;
    bool m_deathCounted = false;
    bool m_runCleared = false;
    float m_hudClock = 1.f;   // seconds since the last text rebuild; starts due
};

} // namespace augment
