// hazard-hitbox (위협제거) + wave-hitbox (웨이브브레이커) + nerve (청심환).
// One class because nerve scales both shrinks by level progress. This owns
// scales::hazard() / scales::wave(), which the GameObject hooks read.
// m_objectRadius is a plain field GD reads inline, so it gets scaled in
// place; rects and OBBs are just marked dirty and the hooks shrink them.

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

// re-applying walks every hazard, so nerve's drift only triggers it past this step
constexpr float kHazardReapplyStep = 0.01f;

class HitboxScales : public Augment {
public:
    HitboxScales() : Augment({ ids::HazardHitbox, ids::WaveHitbox, ids::Nerve }) {}

    // Objects compute their rects as they're added, so the scale has to be
    // set before init. Progress is 0 here, nerve adds nothing yet.
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
        // dirty, so whatever GD cached in addObject goes through the hooks again
        float s = m_hazardApplied;
        if (s < 1.f) {
            if (obj->m_objectRadius > 0.f) obj->m_objectRadius *= s;
            obj->m_isObjectRectDirty = true;
            obj->m_isOrientedBoxDirty = true;
        }
    }

    // cheap when nothing changed; also covers "run ended, still playing" -> 1.0
    bool onBeforeReset(LevelSession& s) override { this->apply(s); return false; }
    void onAttemptStart(LevelSession& s, bool) override { this->apply(s); }

    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (this->handles(id)) this->apply(s);
    }

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
        // don't leak into the editor or the next level
        scales::setHazard(1.f);
        scales::setWave(1.f);
    }

    std::string hudState(LevelSession& s, std::string const& id) override {
        auto& mgr = s.mgr();
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
    static bool playerInWave(LevelSession& s) {
        auto layer = s.layer();
        return (layer->m_player1 && layer->m_player1->m_isDart) || (layer->m_player2 && layer->m_player2->m_isDart);
    }

    void apply(LevelSession& s) {
        auto& mgr = s.mgr();
        bool run = s.runLevel();
        float progress = s.progress();

        // the wave scale is one global the hook reads per call, so it can track nerve exactly
        scales::setWave(run ? mgr.waveScale(progress) : 1.f);
        this->syncPlayerVisuals(s);

        float want = run ? mgr.hazardScale(progress) : 1.f;
        scales::setHazard(want);
        if (std::abs(want - m_hazardApplied) < 0.001f) return;

        // radii were already multiplied in place, so apply the change as a ratio
        float ratio = want / m_hazardApplied;
        for (auto obj : this->levelHazards(s)) {
            if (obj->m_objectRadius > 0.f) obj->m_objectRadius *= ratio;
            obj->m_isObjectRectDirty = true;
            obj->m_isOrientedBoxDirty = true;
        }
        m_hazardApplied = want;
    }

    // Hazards collected from addObject, so a re-apply doesn't walk every
    // object in the level (mostly decoration). If m_objects' count stops
    // matching what came through addObject, rebuild from m_objects.
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
            m_added = count;
        }
        return m_hazards;
    }

    // Shrink the wave icon with its hitbox. GD draws the player at
    // m_vehicleSize (same factor as the rect), so multiply that. Re-set every
    // frame since GD snaps the scale back on its own (resets, mode changes,
    // mini portals); put back once when leaving wave. Trail width
    // (m_waveSize) follows too.
    void syncPlayerVisuals(LevelSession& s) {
        auto layer = s.layer();
        this->syncPlayerVisual(layer->m_player1, m_visualApplied[0]);
        this->syncPlayerVisual(layer->m_player2, m_visualApplied[1]);
    }

    static void syncPlayerVisual(PlayerObject* p, float& applied) {
        if (!p) return;
        float want = scales::wave();
        if (want < 1.f && p->m_isDart) {
            float scale = p->m_vehicleSize * want;
            p->setScale(scale);
            if (p->m_waveTrail) p->m_waveTrail->m_waveSize = scale;
            applied = want;
            return;
        }
        if (applied == 1.f) return;
        p->setScale(p->m_vehicleSize);
        if (p->m_waveTrail) p->m_waveTrail->m_waveSize = p->m_vehicleSize;
        applied = 1.f;
    }

    float m_hazardApplied = 1.f;   // what the objects currently carry
    std::size_t m_added = 0;       // objects seen in addObject
    std::vector<GameObject*> m_hazards;
    float m_visualApplied[2] = { 1.f, 1.f };   // per player node, 1 = GD's own
};

} // namespace

std::unique_ptr<Augment> makeHitboxScales() { return std::make_unique<HitboxScales>(); }

} // namespace augment
