// Blunt augment: shrink hazard hitboxes as GD computes them. See the header
// for why there are two hooks plus a field write in PlayLayerHook.
//
// Pattern from qolmod's AccurateHitboxes (refs/qolmod/src/Hacks/Level/
// AccurateHitboxes.cpp): hook updateOrientedBox(), rewrite the corners, then
// computeAxes() + orderCorners(). Whether getObjectRect() is the only path
// that fills m_objectRect is (unverified) — the counters below tell.

#include "HazardHitboxHook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

namespace {

float g_scale = 1.f;
augment::blunt::Stats g_stats;
// The first few shrinks after every scale change are logged in detail.
int g_logBudget = 0;

void shrinkRect(CCRect& r, float s) {
    float w = r.size.width * s, h = r.size.height * s;
    r.origin.x += (r.size.width - w) * 0.5f;
    r.origin.y += (r.size.height - h) * 0.5f;
    r.size.width = w;
    r.size.height = h;
}

} // namespace

namespace augment::blunt {

void setScale(float scale) {
    if (std::abs(g_scale - scale) < 0.001f) return;
    g_scale = scale;
    g_logBudget = 4;
    log::info("Blunt: hazard hitbox scale -> {:.2f}", scale);
}

float scale() { return g_scale; }

bool isTarget(GameObject* obj) {
    return obj
        && (obj->m_objectType == GameObjectType::Hazard
            || obj->m_objectType == GameObjectType::AnimatedHazard);
}

Stats takeStats() {
    auto s = g_stats;
    g_stats = {};
    return s;
}

} // namespace augment::blunt

class $modify(AugGameObject, GameObject) {
    CCRect const& getObjectRect() {
        // Only a fresh computation may be shrunk: GD recomputes from position
        // and size, so this never compounds. A cached read was shrunk already.
        bool wasDirty = m_isObjectRectDirty;
        auto& rect = GameObject::getObjectRect();
        if (g_scale >= 1.f || !wasDirty || !augment::blunt::isTarget(this)) return rect;
        // Off-grid rotation: the rect is the bounding box of the oriented box,
        // which updateOrientedBox() below has already shrunk.
        if (m_shouldUseOuterOb && m_orientedBox) return rect;

        CCRect before = m_objectRect;
        shrinkRect(m_objectRect, g_scale);
        g_stats.rects++;
        if (g_logBudget > 0) {
            g_logBudget--;
            log::info(
                "Blunt: rect of object id {} {:.1f}x{:.1f} -> {:.1f}x{:.1f}{}",
                m_objectID, before.size.width, before.size.height,
                m_objectRect.size.width, m_objectRect.size.height,
                &rect == &m_objectRect ? "" : " [returned ref is NOT m_objectRect]"
            );
        }
        return rect;
    }

    void updateOrientedBox() {
        bool dirty = m_isOrientedBoxDirty || !m_orientedBox;
        GameObject::updateOrientedBox();
        if (g_scale >= 1.f || !dirty || !m_orientedBox || !augment::blunt::isTarget(this)) return;

        auto box = m_orientedBox;
        auto c = box->m_center;
        for (auto& corner : box->m_corners) corner = c + (corner - c) * g_scale;
        box->computeAxes();
        box->orderCorners();
        g_stats.boxes++;
        if (g_logBudget > 0) {
            g_logBudget--;
            log::info("Blunt: oriented box of object id {} (rotation {:.0f}) shrunk", m_objectID, this->getRotation());
        }
    }
};
