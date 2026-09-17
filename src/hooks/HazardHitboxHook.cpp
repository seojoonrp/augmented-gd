// hazard-hitbox augment: shrink hazard hitboxes as GD computes them. See the header
// for why there are two hooks plus a field write in PlayLayerHook.
//
// Pattern from qolmod's AccurateHitboxes (refs/qolmod/src/Hacks/Level/
// AccurateHitboxes.cpp): hook updateOrientedBox(), rewrite the corners, then
// computeAxes() + orderCorners(). Verified in game 2026-09-16.

#include "HazardHitboxHook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

namespace {

float g_scale = 1.f;

void shrinkRect(CCRect& r, float s) {
    float w = r.size.width * s, h = r.size.height * s;
    r.origin.x += (r.size.width - w) * 0.5f;
    r.origin.y += (r.size.height - h) * 0.5f;
    r.size.width = w;
    r.size.height = h;
}

} // namespace

namespace augment::hazard {

void setScale(float scale) {
    if (std::abs(g_scale - scale) < 0.001f) return;
    g_scale = scale;
    log::info("HazardHitbox: hazard hitbox scale -> {:.2f}", scale);
}

float scale() { return g_scale; }

bool isTarget(GameObject* obj) {
    return obj
        && (obj->m_objectType == GameObjectType::Hazard
            || obj->m_objectType == GameObjectType::AnimatedHazard);
}

} // namespace augment::hazard

class $modify(AugGameObject, GameObject) {
    CCRect const& getObjectRect() {
        // Only a fresh computation may be shrunk: GD recomputes from position
        // and size, so this never compounds. A cached read was shrunk already.
        bool wasDirty = m_isObjectRectDirty;
        auto& rect = GameObject::getObjectRect();
        if (g_scale >= 1.f || !wasDirty || !augment::hazard::isTarget(this)) return rect;
        // Off-grid rotation: the rect is the bounding box of the oriented box,
        // which updateOrientedBox() below has already shrunk.
        if (m_shouldUseOuterOb && m_orientedBox) return rect;

        shrinkRect(m_objectRect, g_scale);
        return rect;
    }

    void updateOrientedBox() {
        bool dirty = m_isOrientedBoxDirty || !m_orientedBox;
        GameObject::updateOrientedBox();
        if (g_scale >= 1.f || !dirty || !m_orientedBox || !augment::hazard::isTarget(this)) return;

        auto box = m_orientedBox;
        auto c = box->m_center;
        for (auto& corner : box->m_corners) corner = c + (corner - c) * g_scale;
        box->computeAxes();
        box->orderCorners();
    }
};
