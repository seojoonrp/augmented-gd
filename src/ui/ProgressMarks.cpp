#include "ProgressMarks.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace augment {

namespace {
    // for when the fill can't be measured (bar is 210x16, fill at (2, 4), 8 high)
    constexpr float kFallbackInset = 2.f;
    constexpr float kFallbackFillHeight = 8.f;
    constexpr float kRim = 1.f;
    // dot radius vs track height. the measured fill is sometimes the whole
    // bar, so the height gets clamped to 8 first
    constexpr float kDotOfTrack = 0.375f;

    constexpr ccColor4F kRimColor = { 0.f, 0.f, 0.f, 1.f };
    // same gold as RunHud's new-best particles
    constexpr ccColor4F kBestColor = { 1.f, 0.824f, 0.235f, 1.f };
    constexpr ccColor4F kCheckpointColor = { 0.47f, 1.f, 0.47f, 1.f };
    // filled circles, not drawDot (GD's drawDot is a square)
    constexpr ccColor4F kNoBorder = { 0.f, 0.f, 0.f, 0.f };
    constexpr int kDotSegments = 14;
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

    // fill x = left inset, assume the same on the right
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
    m_radius = std::min(fillHeight, kFallbackFillHeight) * kDotOfTrack;

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
        m_node->drawCircle(c, m_radius, kRimColor, 0.f, kNoBorder, kDotSegments);
        m_node->drawCircle(c, m_radius - kRim, color, 0.f, kNoBorder, kDotSegments);
    };
    for (float p : m_checkpoints) dot(p, kCheckpointColor);
    if (m_best > 0.f) dot(m_best, kBestColor);
}

} // namespace augment
