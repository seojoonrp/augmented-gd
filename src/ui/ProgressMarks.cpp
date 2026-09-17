#include "ProgressMarks.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace augment {

namespace {
    // Used when the fill sprite can't be measured (2026-09-17 log: bar
    // 210x16, fill at (2, 4) 8 high).
    constexpr float kFallbackInset = 2.f;
    constexpr float kFallbackFillHeight = 8.f;
    constexpr float kRim = 1.f;

    constexpr ccColor4F kRimColor = { 0.f, 0.f, 0.f, 1.f };
    constexpr ccColor4F kBestColor = { 1.f, 1.f, 1.f, 1.f };
    constexpr ccColor4F kCheckpointColor = { 0.47f, 1.f, 0.47f, 1.f };
}

ProgressMarks* ProgressMarks::create(CCSprite* bar, CCSprite* fill) {
    auto ret = new ProgressMarks();
    if (ret->init(bar, fill)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ProgressMarks::init(CCSprite* bar, CCSprite* fill) {
    if (!bar || !CCNode::init()) return false;

    auto size = bar->getContentSize();

    // The fill is placed at the track's left edge; assume the same rim on
    // the right. If GD puts the fill somewhere else, fall back to a guess.
    bool fillIsChild = fill && fill->getParent() == bar;
    float inset = fillIsChild ? fill->getPositionX() : kFallbackInset;
    m_trackLeft = inset;
    m_trackWidth = std::max(1.f, size.width - 2.f * inset);
    float fillHeight = fillIsChild ? fill->getContentSize().height : kFallbackFillHeight;
    if (fillHeight <= 0.f || fillHeight > size.height) fillHeight = kFallbackFillHeight;
    float fillBottom = fillIsChild
        ? fill->getPositionY() - fill->getAnchorPoint().y * fillHeight
        : (size.height - fillHeight) / 2.f;
    m_centreY = fillBottom + fillHeight / 2.f;
    m_radius = fillHeight / 2.f;

    log::info(
        "ProgressMarks: bar size {}x{} at ({:.1f}, {:.1f}); fill {} -> track left {:.1f} width {:.1f}, dots r {:.1f} at y {:.1f}",
        size.width, size.height, bar->getPositionX(), bar->getPositionY(),
        fill ? (fillIsChild ? "measured" : "not a child of the bar") : "missing",
        m_trackLeft, m_trackWidth, m_radius, m_centreY
    );

    this->setPosition({ 0.f, 0.f });
    this->setContentSize(size);
    this->setID("progress-marks"_spr);
    bar->addChild(this, 10);

    m_node = CCDrawNode::create();
    this->addChild(m_node);
    return true;
}

float ProgressMarks::xForPercent(float percent) const {
    return m_trackLeft + m_trackWidth * std::clamp(percent, 0.f, 100.f) / 100.f;
}

void ProgressMarks::setBest(float percent) {
    if (percent == m_best) return;
    m_best = percent;
    this->redraw();
}

void ProgressMarks::setCheckpoints(std::vector<float> const& percents) {
    if (percents == m_checkpoints) return;
    m_checkpoints = percents;
    this->redraw();
}

void ProgressMarks::redraw() {
    m_node->clear();
    auto dot = [&](float percent, ccColor4F color) {
        CCPoint c{ this->xForPercent(percent), m_centreY };
        m_node->drawDot(c, m_radius, kRimColor);
        m_node->drawDot(c, m_radius - kRim, color);
    };
    for (float p : m_checkpoints) dot(p, kCheckpointColor);
    if (m_best > 0.f) dot(m_best, kBestColor);
}

} // namespace augment
