// foresight (사륜안): draw GD-style hitboxes around the player. GD's own
// hitbox drawing is gated by an inlined "practice mode && show-hitboxes"
// check that can't be reached from a hook (OpenHack byte-patches it), so
// the boxes are drawn on our own CCDrawNode in the object layer.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <optional>

using namespace geode::prelude;

namespace augment {

namespace {

// Past the screen edge a box still counts as on screen: its outline is drawn
// centred on the edge and anti-aliased, so one just outside can still show.
constexpr float kViewPad = 4.f;

class Foresight : public Augment {
public:
    Foresight() : Augment({ ids::Foresight }) {}

    void onFrame(LevelSession& s, float) override {
        if (s.owns(ids::Foresight)) this->draw(s);
    }

private:
    // The screen in object-layer coordinates (the draw node's space): the
    // bounding box of its four corners, so camera zoom, offset and rotation
    // all count, like hazard::viewAhead.
    static CCRect visibleRect(PlayLayer* layer) {
        auto const win = CCDirector::get()->getWinSize();
        CCPoint const corners[] = { { 0.f, 0.f }, { win.width, 0.f }, { 0.f, win.height }, { win.width, win.height } };
        auto const first = layer->m_objectLayer->convertToNodeSpace(corners[0]);
        float minX = first.x, maxX = first.x, minY = first.y, maxY = first.y;
        for (auto const& corner : corners) {
            auto const p = layer->m_objectLayer->convertToNodeSpace(corner);
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        return { minX - kViewPad, minY - kViewPad, maxX - minX + 2 * kViewPad, maxY - minY + 2 * kViewPad };
    }

    void draw(LevelSession& s) {
        auto layer = s.layer();
        if (!layer->m_objectLayer || !layer->m_objects) return;

        if (!m_node) {
            m_node = CCDrawNode::create();
            m_node->setID("foresight-hitboxes"_spr);
            layer->m_objectLayer->addChild(m_node, 1000);
        }

        auto node = m_node;
        node->clear();
        node->setVisible(true);
        if (!layer->m_player1) return;

        // GD-style: thin outlines, no fill. Blue = solid, red = hazard,
        // green = everything else that interacts (portals, pads, rings, coins).
        constexpr float kBorder = 0.25f;
        ccColor4F const noFill = { 0.f, 0.f, 0.f, 0.f };
        auto colorFor = [](GameObjectType type) -> std::optional<ccColor4F> {
            switch (type) {
                case GameObjectType::Solid:
                case GameObjectType::Slope:
                case GameObjectType::Breakable:
                    return ccColor4F{ 0.f, 0.25f, 1.f, 1.f };
                case GameObjectType::Hazard:
                case GameObjectType::AnimatedHazard:
                    return ccColor4F{ 1.f, 0.f, 0.f, 1.f };
                case GameObjectType::Decoration:
                case GameObjectType::Special:
                case GameObjectType::Modifier:
                case GameObjectType::EnterEffectObject:
                case GameObjectType::CollisionObject:
                    return std::nullopt;
                default:
                    return ccColor4F{ 0.f, 1.f, 0.f, 1.f };
            }
        };

        // Objects within roughly one screen ahead / a bit behind the player,
        // at any height; of those, only the boxes that reach the screen are
        // drawn. Every box is a polygon rebuilt each frame, and the window
        // holds far more than the screen does on a tall or zoomed-in level.
        CCRect const view = visibleRect(layer);
        float px = layer->m_player1->getPositionX();
        s.forEachObjectInX(px - 240.f, px + 720.f, [&](GameObject* obj) {
            if (!obj->isVisible() || obj->m_isHide) return;
            // Disabled = removed by the cat (or toggled off by the level); GD
            // skips these in collision, so no box.
            if (obj->m_isDisabled || obj->m_isDisabled2) return;
            auto color = colorFor(obj->m_objectType);
            if (!color) return;
            if (obj->m_objectRadius > 0.f) {
                auto const& centre = obj->getPosition();
                float const r = obj->m_objectRadius;
                if (!view.intersectsRect({ centre.x - r, centre.y - r, 2 * r, 2 * r })) return;
                node->drawCircle(centre, r, noFill, kBorder, *color, 32);
            }
            else {
                auto const& rect = obj->getObjectRect();
                if (!view.intersectsRect(rect)) return;
                node->drawRect(rect, noFill, kBorder, *color);
            }
        });

        // Player: yellow outer box plus the smaller inner box GD uses for
        // solid collisions.
        ccColor4F const playerColor = { 1.f, 1.f, 0.f, 1.f };
        auto drawPlayer = [&](PlayerObject* p) {
            auto rect = p->getObjectRect();
            node->drawRect(rect, noFill, kBorder, playerColor);
            float k = p->m_vehicleSize >= 1.f ? 0.25f : 0.4f;
            CCRect inner{
                rect.origin.x + rect.size.width * (1.f - k) / 2.f,
                rect.origin.y + rect.size.height * (1.f - k) / 2.f,
                rect.size.width * k, rect.size.height * k
            };
            node->drawRect(inner, noFill, kBorder, playerColor);
        };
        drawPlayer(layer->m_player1);
        if (layer->m_player2 && layer->m_gameState.m_isDualMode) drawPlayer(layer->m_player2);
    }

    // Child of m_objectLayer; dies with the level.
    CCDrawNode* m_node = nullptr;
};

} // namespace

std::unique_ptr<Augment> makeForesight() { return std::make_unique<Foresight>(); }

} // namespace augment
