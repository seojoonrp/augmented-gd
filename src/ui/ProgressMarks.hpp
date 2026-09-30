#pragma once

#include <Geode/Geode.hpp>

#include <vector>

namespace augment {

// Dots on GD's progress bar: gold at the run's best, green per checkpoint.
// Child of the bar, so it follows its position/scale/visibility.
class ProgressMarks : public cocos2d::CCNode {
public:
    // fill (m_progressFill) is only used to measure the track; null = fallback numbers
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
