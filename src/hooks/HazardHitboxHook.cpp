// Shrinks hazard hitboxes as GD computes them (see the header). The oriented box
// part is the same trick as qolmod's accurate hitboxes.

#include "HazardHitboxHook.hpp"
#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

namespace {

void shrinkRect(CCRect& r, float s) {
    float w = r.size.width * s, h = r.size.height * s;
    r.origin.x += (r.size.width - w) * 0.5f;
    r.origin.y += (r.size.height - h) * 0.5f;
    r.size.width = w;
    r.size.height = h;
}

} // namespace

namespace augment::hazard {

bool isTarget(GameObject* obj) {
    return obj
        && (obj->m_objectType == GameObjectType::Hazard
            || obj->m_objectType == GameObjectType::AnimatedHazard);
}

} // namespace augment::hazard

class $modify(AugGameObject, GameObject) {
    CCRect const& getObjectRect() {
        // only shrink a fresh computation, a cached rect is already shrunk
        bool wasDirty = m_isObjectRectDirty;
        auto& rect = GameObject::getObjectRect();
        float scale = augment::scales::hazard();
        if (scale >= 1.f || !wasDirty || !augment::hazard::isTarget(this)) return rect;
        // rotated off-grid: the rect is the oriented box's bounds, shrunk below
        if (m_shouldUseOuterOb && m_orientedBox) return rect;

        shrinkRect(m_objectRect, scale);
        return rect;
    }

    void updateOrientedBox() {
        bool dirty = m_isOrientedBoxDirty || !m_orientedBox;
        GameObject::updateOrientedBox();
        float scale = augment::scales::hazard();
        if (scale >= 1.f || !dirty || !m_orientedBox || !augment::hazard::isTarget(this)) return;

        auto box = m_orientedBox;
        auto c = box->m_center;
        for (auto& corner : box->m_corners) corner = c + (corner - c) * scale;
        box->computeAxes();
        box->orderCorners();
    }
};
