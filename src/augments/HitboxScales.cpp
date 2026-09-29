// hazard-hitbox (위협제거) + wave-hitbox (웨이브브레이커) + nerve (청심환):
// one behaviour, because nerve makes both shrinks depend on how far into
// the level the player is, so they are published together.
//
// The GameObject hooks (HazardHitboxHook.cpp, PlayerHitboxHook.cpp) read
// scales::hazard() / scales::wave(). This file owns those two values for the
// life of a level and brings the objects already in the level in line with
// the hazard one (m_objectRadius is a plain field GD reads inline, so it is
// scaled in place; rects and oriented boxes are re-dirtied and shrunk lazily
// by the hooks).

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Scales.hpp"
#include "../hooks/HazardHitboxHook.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace augment {

namespace {

// nerve moves the hazard scale continuously, but re-applying it walks every
// object in the level, so the per-frame update only does that once the scale
// has drifted this far from what the objects currently carry.
constexpr float kHazardReapplyStep = 0.01f;

class HitboxScales : public Augment {
public:
    HitboxScales() : Augment({ ids::HazardHitbox, ids::WaveHitbox, ids::Nerve }) {}

    // Before PlayLayer::init creates the first object: objects compute their
    // rects as they are added, so the scale has to be in place already. A
    // level always starts at 0 %, so nerve contributes nothing yet.
    void onLevelInit(LevelSession& s) override {
        auto& mgr = s.mgr();
        bool run = s.runLevel();
        scales::setHazard(run ? mgr.hazardScale(0.f) : 1.f);
        scales::setWave(run ? mgr.waveScale(0.f) : 1.f);
        m_hazardApplied = scales::hazard();
    }

    void onObjectAdded(LevelSession&, GameObject* obj) override {
        m_added++;
        if (!hazard::isTarget(obj)) return;
        m_hazards.push_back(obj);
        // Dirtying makes sure anything GD cached during addObject is
        // recomputed through the hooks.
        float s = m_hazardApplied;
        if (s < 1.f) {
            if (obj->m_objectRadius > 0.f) obj->m_objectRadius *= s;
            obj->m_isObjectRectDirty = true;
            obj->m_isOrientedBoxDirty = true;
        }
    }

    // Cheap when nothing changed, so it runs around every reset (also covers
    // "run ended, still playing" -> 1.0).
    bool onBeforeReset(LevelSession& s) override { this->apply(s); return false; }
    void onAttemptStart(LevelSession& s, bool) override { this->apply(s); }

    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (this->handles(id)) this->apply(s);
    }

    // Per frame. Only pays the full re-apply once the nerve boost has moved
    // the hazard scale by kHazardReapplyStep.
    void onFrame(LevelSession& s, float) override {
        auto& mgr = s.mgr();
        bool run = s.runLevel();
        float progress = s.progress();

        scales::setWave(run ? mgr.waveScale(progress) : 1.f);
        this->syncPlayerVisuals(s);

        float want = run ? mgr.hazardScale(progress) : 1.f;
        if (std::abs(want - m_hazardApplied) >= kHazardReapplyStep) this->apply(s);
    }

    void onQuit(LevelSession&) override {
        // Never let the scales leak into the editor or the next level.
        scales::setHazard(1.f);
        scales::setWave(1.f);
    }

    std::string hudState(LevelSession& s, std::string const& id) override {
        auto& mgr = s.mgr();
        // Both shrinks are read at the player's current position, because
        // nerve grows them as the level goes on.
        float progress = s.progress();
        if (id == ids::HazardHitbox) {
            return fmt::format("Lv{}  hazards {:.0f}%", mgr.levelOf(id), mgr.hazardScale(progress) * 100.f);
        }
        if (id == ids::WaveHitbox) {
            return fmt::format(
                "Lv{}  player {:.0f}% in wave{}", mgr.levelOf(id), mgr.waveScale(progress) * 100.f,
                this->playerInWave(s) ? "  ACTIVE" : ""
            );
        }
        return fmt::format("Lv{}  shrink x{:.2f}", mgr.levelOf(id), mgr.nerveBoost(progress));
    }

private:
    // wave-hitbox only bites while a player is in wave mode; in dual either
    // player being in wave is enough for the HUD to call it active.
    static bool playerInWave(LevelSession& s) {
        auto layer = s.layer();
        return (layer->m_player1 && layer->m_player1->m_isDart) || (layer->m_player2 && layer->m_player2->m_isDart);
    }

    // Publish both scales for where the player is right now and bring every
    // object already in the level in line with the hazard one.
    void apply(LevelSession& s) {
        auto& mgr = s.mgr();
        bool run = s.runLevel();
        float progress = s.progress();

        // The player's scale is a single global the hook reads per call, so it
        // can follow the nerve boost exactly.
        scales::setWave(run ? mgr.waveScale(progress) : 1.f);
        this->syncPlayerVisuals(s);

        float want = run ? mgr.hazardScale(progress) : 1.f;
        scales::setHazard(want);
        if (std::abs(want - m_hazardApplied) < 0.001f) return;

        int hazards = 0, radii = 0;
        // Radii are multiplied in place, so a change is applied as a ratio.
        float ratio = want / m_hazardApplied;
        for (auto obj : this->levelHazards(s)) {
            hazards++;
            if (obj->m_objectRadius > 0.f) {
                obj->m_objectRadius *= ratio;
                radii++;
            }
            obj->m_isObjectRectDirty = true;
            obj->m_isOrientedBoxDirty = true;
        }
        m_hazardApplied = want;
        log::info("HazardHitbox: scale {:.2f} applied to {} hazards ({} circular)", want, hazards, radii);
    }

    // Every hazard in m_objects, which a re-apply has to reach. nerve
    // re-applies every 0.01 of scale, and walking the whole level for them
    // touched every object (decoration included, often most of a level) each
    // time, so the hazards are kept as PlayLayer::addObject hands them over.
    // Should m_objects hold a different number of objects than came through
    // there, the list is rebuilt from m_objects, i.e. the old walk.
    std::vector<GameObject*> const& levelHazards(LevelSession& s) {
        auto objects = s.layer()->m_objects;
        std::size_t const count = objects ? objects->count() : 0;
        if (count != m_added) {
            m_hazards.clear();
            if (objects) {
                for (auto obj : CCArrayExt<GameObject*>(objects)) {
                    if (hazard::isTarget(obj)) m_hazards.push_back(obj);
                }
            }
            log::info(
                "HazardHitbox: {} objects in the level but {} came through addObject, hazard list rebuilt ({})",
                count, m_added, m_hazards.size()
            );
            m_added = count;
        }
        return m_hazards;
    }

    // Visual half of wave-hitbox: the icon shrinks with the rect. GD draws the
    // player node at m_vehicleSize (updatePlayerScale = setScaleX/Y(m_vehicleSize),
    // bindings inline/PlayerObject.cpp:823; the rect uses the same factor), so
    // the same scale times ours keeps icon and hitbox in step. Re-asserted every
    // frame after GD's update because GD snaps the scale back on its own
    // (resets, mode changes, the mini-portal action), and restored once when the
    // player leaves wave or the run ends. The trail thickness follows too
    // (m_waveSize is the same factor, qolmod WaveTrailSize.cpp:47-49).
    void syncPlayerVisuals(LevelSession& s) {
        auto layer = s.layer();
        this->syncPlayerVisual(layer->m_player1, m_visualApplied[0], 1);
        this->syncPlayerVisual(layer->m_player2, m_visualApplied[1], 2);
    }

    static void syncPlayerVisual(PlayerObject* p, float& applied, int index) {
        if (!p) return;
        float want = scales::wave();
        if (want < 1.f && p->m_isDart) {
            float scale = p->m_vehicleSize * want;
            p->setScale(scale);
            if (p->m_waveTrail) p->m_waveTrail->m_waveSize = scale;
            if (applied == 1.f) log::info("WaveHitbox: player {} icon x{:.2f} (node scale {:.2f})", index, want, scale);
            applied = want;
            return;
        }
        if (applied == 1.f) return;
        p->setScale(p->m_vehicleSize);
        if (p->m_waveTrail) p->m_waveTrail->m_waveSize = p->m_vehicleSize;
        log::info("WaveHitbox: player {} icon restored (node scale {:.2f})", index, p->m_vehicleSize);
        applied = 1.f;
    }

    // The hazard scale every object currently in the level carries.
    float m_hazardApplied = 1.f;
    // Objects PlayLayer::addObject has handed over, and the hazards among them.
    std::size_t m_added = 0;
    std::vector<GameObject*> m_hazards;
    // The wave scale each player's node currently carries (1 = GD's own).
    float m_visualApplied[2] = { 1.f, 1.f };
};

} // namespace

std::unique_ptr<Augment> makeHitboxScales() { return std::make_unique<HitboxScales>(); }

} // namespace augment
