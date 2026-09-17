#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace augment {

// PlayLayer overlay: a draft gauge that mirrors GD's progress bar at the
// bottom of the screen (same sprite, fills left to right) with "charge/cost"
// in GD's percent font at its right end; a column at the far left with one
// row per owned augment (placeholder icon box + name + state text — art
// comes later); and a transient centred notice.
class RunHud : public cocos2d::CCNode {
public:
    struct Slot {
        std::string name;   // Korean augment name (mod font)
        std::string state;  // English per-attempt state, may be empty
    };

    static RunHud* create();

    // Builds the gauge as a twin of GD's progress bar mirrored to the bottom
    // edge: sprite frame, scale and fill inset copied from `progressBar` /
    // `progressFill`, the readout's font and scale from `percentLabel`
    // (PlayLayer::m_percentageLabel). All three may be null (hidden in
    // settings); sensible fallbacks then.
    void attachGauge(cocos2d::CCSprite* progressBar, cocos2d::CCSprite* progressFill, cocos2d::CCLabelBMFont* percentLabel);
    // Gauge charge against the current cost; shown as "value/threshold" and
    // as the fill ratio. Callers pass value = threshold to show it full.
    void setGauge(float value, float threshold);
    // Debug stat line at the top of the augment column.
    void setHeader(std::string const& text);
    // One row per owned augment, top to bottom. Only rows whose text changed
    // are touched.
    void setSlots(std::vector<Slot> const& slots);
    // Show a big message in the middle of the screen that fades out.
    void notice(std::string const& text, cocos2d::ccColor3B color = { 255, 255, 255 });

protected:
    bool init() override;
    void update(float dt) override;

    struct SlotNodes {
        cocos2d::CCNode* root = nullptr;
        cocos2d::CCLabelBMFont* name = nullptr;
        cocos2d::CCLabelBMFont* state = nullptr;
    };
    SlotNodes makeSlot(size_t index);

    cocos2d::CCSprite* m_gaugeBar = nullptr;     // GJ_progressBar_001 like GD's
    cocos2d::CCLayerColor* m_gaugeFill = nullptr; // child of m_gaugeBar
    cocos2d::CCLabelBMFont* m_gaugeLabel = nullptr;
    float m_trackLeft = 0.f;    // fill inset inside the bar sprite (bar units)
    float m_trackWidth = 0.f;
    float m_gaugeShown = 0.f;   // 0..1, eased toward m_gaugeTarget every frame
    float m_gaugeTarget = 0.f;

    cocos2d::CCLabelBMFont* m_header = nullptr;
    std::vector<SlotNodes> m_slots;
    cocos2d::CCLabelBMFont* m_notice = nullptr;
};

} // namespace augment
