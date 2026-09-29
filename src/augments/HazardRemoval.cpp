#include "HazardRemoval.hpp"
#include "../game/LevelSession.hpp"
#include "../hooks/HazardHitboxHook.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace augment::hazard {

void Removed::take(GameObject* obj) {
    m_entries.push_back({ obj, obj->getOpacity() });
    obj->destroyObject();
}

int Removed::restore() {
    int stillDisabled = 0;
    for (auto& e : m_entries) {
        if (!e.obj) continue;
        if (e.obj->m_isDisabled || e.obj->m_isDisabled2) stillDisabled++;
        e.obj->m_isDisabled = false;
        e.obj->m_isDisabled2 = false;
        e.obj->setOpacity(e.opacity);
    }
    m_entries.clear();
    return stillDisabled;
}

bool View::contains(GameObject* obj, CCNode* fallbackParent) const {
    CCNode* parent = obj->getParent() ? obj->getParent() : fallbackParent;
    CCPoint onScreen = parent->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
    return screen.containsPoint(onScreen);
}

View viewAhead(PlayLayer* layer, float lead) {
    constexpr float kMargin = 30.f;
    auto win = CCDirector::get()->getWinSize();
    View v;
    v.screen = { -kMargin, -kMargin, win.width + 2 * kMargin, win.height + 2 * kMargin };
    float px = layer->m_player1->getPositionX();
    bool aheadIsLeft = layer->m_player1->m_isGoingLeft;

    // Bound the scan by the screen's extent in object-layer x, whatever the
    // camera does.
    v.lo = px;
    v.hi = px;
    for (auto corner : { CCPoint{ v.screen.getMinX(), v.screen.getMinY() }, CCPoint{ v.screen.getMaxX(), v.screen.getMinY() },
                         CCPoint{ v.screen.getMinX(), v.screen.getMaxY() }, CCPoint{ v.screen.getMaxX(), v.screen.getMaxY() } }) {
        float x = layer->m_objectLayer->convertToNodeSpace(corner).x;
        v.lo = std::min(v.lo, x);
        v.hi = std::max(v.hi, x);
    }
    if (aheadIsLeft) v.hi = px - lead; else v.lo = px + lead;
    return v;
}

std::vector<GameObject*> hazardsInView(LevelSession& s, View const& view) {
    auto layer = s.layer();
    std::vector<GameObject*> found;
    s.forEachObjectInX(view.lo, view.hi, [&](GameObject* obj) {
        if (!isTarget(obj) || obj == layer->m_anticheatSpike) return;
        if (obj->m_isDisabled || obj->m_isDisabled2) return;
        if (!view.contains(obj, layer->m_objectLayer)) return;
        found.push_back(obj);
    });
    return found;
}

bool touchesCircle(GameObject* obj, CCPoint centre, float radius) {
    if (obj->m_objectRadius > 0.f) {
        float dx = obj->getPositionX() - centre.x;
        float dy = obj->getPositionY() - centre.y;
        float reach = radius + obj->m_objectRadius * std::max(obj->getScaleX(), obj->getScaleY());
        return dx * dx + dy * dy <= reach * reach;
    }
    // Closest point of the rect to the centre, then the distance to it.
    auto rect = obj->getObjectRect();
    float cx = std::clamp(centre.x, rect.getMinX(), rect.getMaxX());
    float cy = std::clamp(centre.y, rect.getMinY(), rect.getMaxY());
    float dx = cx - centre.x;
    float dy = cy - centre.y;
    return dx * dx + dy * dy <= radius * radius;
}

} // namespace augment::hazard
