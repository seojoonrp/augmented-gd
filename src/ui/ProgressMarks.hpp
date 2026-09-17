#pragma once

#include <Geode/Geode.hpp>

#include <vector>

namespace augment {

// Markers drawn on GD's own progress bar (PlayLayer::m_progressBar): a white
// dot at the run's best percent and a green dot per placed checkpoint, each
// with a black rim like the bar's, sized to sit inside the fill track.
// Lives as a child of the bar, so it follows the bar's position, scale and
// visibility and dies with it. One draw node, redrawn only when a value
// changes.
class ProgressMarks : public cocos2d::CCNode {
public:
    // `fill` (PlayLayer::m_progressFill) only serves to measure the track;
    // nullptr falls back to the geometry seen in the 2026-09-17 log.
    static ProgressMarks* create(cocos2d::CCSprite* bar, cocos2d::CCSprite* fill);

    void setBest(float percent);
    void setCheckpoints(std::vector<float> const& percents);

protected:
    bool init(cocos2d::CCSprite* bar, cocos2d::CCSprite* fill);
    float xForPercent(float percent) const;
    void redraw();

    float m_trackLeft = 0.f;
    float m_trackWidth = 0.f;
    float m_centreY = 0.f;
    float m_radius = 0.f;

    cocos2d::CCDrawNode* m_node = nullptr;
    float m_best = 0.f;
    std::vector<float> m_checkpoints;
};

} // namespace augment
