// cat (고양이): every catInterval() seconds of play, remove catCount()
// random hazards that are on screen and ahead of the player. Removal is
// GD's own GameObject::destroyObject() (m_isDisabled + m_isDisabled2 +
// opacity 0, Geode inline source): collisionCheckObjects skips objects with
// either flag set (xdBot's trajectory sim relies on that), and opacity 0
// takes the sprite with it. Everything is put back before every reset.
// A CatNode in the UI layer fires a laser at each removed hazard.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../hooks/HazardHitboxHook.hpp"
#include "../ui/CatNode.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <random>

using namespace geode::prelude;

namespace augment {

namespace {

class Cat : public Augment {
public:
    Cat() : Augment({ ids::Cat }) {}

    void onFrame(LevelSession& s, float dt) override {
        if (!s.owns(ids::Cat)) return;
        auto layer = s.layer();
        if (!m_node && layer->m_uiLayer) {
            m_node = CatNode::create();
            layer->m_uiLayer->addChild(m_node, 999);
            log::info("Cat: node added to the UI layer");
        }
        if (!s.runAttempt() || layer->m_isPaused || !layer->m_player1 || layer->m_player1->m_isDead) return;

        float interval = s.mgr().catInterval();
        if (interval <= 0.f) return;
        m_timer += dt;
        if (m_timer < interval) return;
        m_timer = 0.f;
        this->sweep(s);
    }

    // Put back everything the cat took this attempt. Runs before GD's own
    // reset so any per-object reset GD does still gets the last word.
    bool onBeforeReset(LevelSession&) override {
        m_timer = 0.f;
        if (m_removed.empty()) return false;
        int stillDisabled = 0;
        for (auto& r : m_removed) {
            if (!r.obj) continue;
            if (r.obj->m_isDisabled || r.obj->m_isDisabled2) stillDisabled++;
            r.obj->m_isDisabled = false;
            r.obj->m_isDisabled2 = false;
            r.obj->setOpacity(r.opacity);
        }
        log::info("Cat: restored {} hazards ({} were still disabled)", m_removed.size(), stillDisabled);
        m_removed.clear();
        return false;
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        auto& mgr = s.mgr();
        return fmt::format(
            "Lv{}  {} per {:.1f}s, next in {:.1f}s, removed {}",
            s.levelOf(ids::Cat), mgr.catCount(), mgr.catInterval(),
            std::max(0.f, mgr.catInterval() - m_timer), m_removed.size()
        );
    }

private:
    void sweep(LevelSession& s) {
        auto layer = s.layer();
        int want = s.mgr().catCount();
        if (want <= 0 || !layer->m_objectLayer || !layer->m_player1) return;

        // "In view" = the object's screen position is inside the window (plus
        // a margin), computed through the real node transform so camera zoom,
        // offset and rotation all count. "Ahead" = further along in the
        // object layer's x than the player (behind them in platformer mode
        // when they are heading left).
        constexpr float kMargin = 30.f;
        auto win = CCDirector::get()->getWinSize();
        CCRect screen{ -kMargin, -kMargin, win.width + 2 * kMargin, win.height + 2 * kMargin };
        float px = layer->m_player1->getPositionX();
        bool aheadIsLeft = layer->m_player1->m_isGoingLeft;

        // Bound the scan by the screen's extent in object-layer x, whatever
        // the camera does.
        float lo = px, hi = px;
        for (auto corner : { CCPoint{ screen.getMinX(), screen.getMinY() }, CCPoint{ screen.getMaxX(), screen.getMinY() },
                             CCPoint{ screen.getMinX(), screen.getMaxY() }, CCPoint{ screen.getMaxX(), screen.getMaxY() } }) {
            float x = layer->m_objectLayer->convertToNodeSpace(corner).x;
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
        if (aheadIsLeft) hi = px; else lo = px;

        std::vector<GameObject*> candidates;
        auto& objs = s.objectsByX();
        auto it = std::lower_bound(objs.begin(), objs.end(), lo, [](GameObject* o, float x) {
            return o->getPositionX() < x;
        });
        for (; it != objs.end() && (*it)->getPositionX() <= hi; ++it) {
            auto obj = *it;
            if (!hazard::isTarget(obj) || obj == layer->m_anticheatSpike) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2) continue;
            CCNode* parent = obj->getParent() ? obj->getParent() : layer->m_objectLayer;
            CCPoint onScreen = parent->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
            if (!screen.containsPoint(onScreen)) continue;
            candidates.push_back(obj);
        }

        static std::mt19937 rng{ std::random_device{}() };
        std::shuffle(candidates.begin(), candidates.end(), rng);
        int removed = 0;
        std::vector<GameObject*> hit;
        for (auto obj : candidates) {
            if (removed >= want) break;
            m_removed.push_back({ obj, obj->getOpacity() });
            hit.push_back(obj);
            obj->destroyObject();
            removed++;
        }
        if (m_node) m_node->fireAt(hit);
        log::info(
            "Cat: removed {}/{} of {} hazards in view at {:.1f}% (scan x {:.0f}..{:.0f}, {} removed this attempt)",
            removed, want, candidates.size(), s.percent(), lo, hi, m_removed.size()
        );
        if (removed > 0) s.notice(fmt::format("CAT  -{}", removed), { 255, 180, 230 });
    }

    // Seconds since the last sweep, and what this attempt's sweeps removed
    // (a removed object keeps the opacity it had).
    float m_timer = 0.f;
    struct Removal { Ref<GameObject> obj; unsigned char opacity; };
    std::vector<Removal> m_removed;
    // Child of m_uiLayer; dies with the level.
    CatNode* m_node = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeCat() { return std::make_unique<Cat>(); }

} // namespace augment
