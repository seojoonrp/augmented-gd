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
    // as the fill ratio. `full` = a gauge-earned draft is waiting: the bar
    // shows full and the readout says DRAFT! (the cost has already risen
    // by then, so numbers would show the *next* cost). While a death
    // reward plays this only records the target.
    void setGauge(float value, float threshold, bool full);
    // Death reward sequence, driven by update() so it plays during GD's
    // death delay: "+normal" (and "+bonus" in gold when > 0) pop up beside
    // `at` (this node's space), particles fly from there into the gauge,
    // then the fill grows by the normal part (green) and by the bonus
    // (gold), which blends into the bar afterwards. `before` and
    // `threshold` are the gauge and its cost as they were before the death.
    void playDeathReward(cocos2d::CCPoint at, float before, float normal, float bonus, float threshold);
    // Ends a running reward at once: fill snapped to the last setGauge
    // target, particles and numbers gone. Called at attempt start so the
    // sequence never spills into the next attempt.
    void settleGauge();
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

    // One reward particle: a quadratic bezier from the death spot to the
    // gauge tip, leaving `depart` seconds into the sequence.
    struct Particle {
        cocos2d::CCPoint from, ctrl, to;
        float depart = 0.f;
        bool gold = false;
        bool landed = false;   // landing flash drawn
    };
    struct RewardText {
        cocos2d::CCLabelBMFont* label = nullptr;
        bool used = false;  // part of the current sequence at all
        float baseY = 0.f;
        float born = 0.f;   // seconds into the sequence
    };
    struct Reward {
        bool active = false;
        float t = 0.f;                              // seconds since the death
        float start = 0.f, mid = 0.f, end = 0.f;    // fill ratios: before / +normal / +bonus
        float fillAt = 0.f, goldFillAt = 0.f, mergeAt = 0.f, endAt = 0.f;   // phase starts (s)
        float threshold = 0.f;   // the cost the readout counts toward
        std::vector<Particle> particles;
        struct Flash { cocos2d::CCPoint at; float born; bool gold; };
        std::vector<Flash> flashes;   // one short burst per landed particle
    };
    void stepReward(float dt);
    // Stops the sequence with both segments at `ratio`.
    void finishReward(float ratio);
    // Lays out the green and gold segments from m_gaugeShown / m_goldShown.
    void layoutFill();
    // Fill tip for a ratio, in this node's space.
    cocos2d::CCPoint tipAt(float ratio);
    void spawnParticles(int count, bool gold, cocos2d::CCPoint from, cocos2d::CCPoint to, float departAfter);
    void placeRewardText(RewardText& text, std::string const& str, cocos2d::CCPoint at, float dy, float born);
    void animateRewardText(RewardText& text);

    cocos2d::CCSprite* m_gaugeBar = nullptr;     // GJ_progressBar_001 like GD's
    cocos2d::CCLayerColor* m_gaugeFill = nullptr; // child of m_gaugeBar
    cocos2d::CCLayerColor* m_gaugeGold = nullptr; // new-best segment right after the green one
    cocos2d::CCLabelBMFont* m_gaugeLabel = nullptr;
    float m_trackLeft = 0.f;    // fill inset inside the bar sprite (bar units)
    float m_trackWidth = 0.f;
    float m_fillHeight = 0.f;
    float m_fillBottom = 0.f;
    float m_gaugeShown = 0.f;   // 0..1, eased toward m_gaugeTarget every frame
    float m_goldShown = 0.f;    // 0..1, where the gold segment ends (>= m_gaugeShown)
    float m_gaugeTarget = 0.f;
    bool m_gaugeFull = false;   // last setGauge said a draft is waiting
    std::string m_gaugeText;    // last setGauge readout; applied once no reward runs

    Reward m_reward;
    cocos2d::CCDrawNode* m_particles = nullptr;   // all reward particles, redrawn every frame
    RewardText m_rewardText;       // "+normal" beside the dead icon
    RewardText m_rewardGoldText;   // "+bonus" under it

    cocos2d::CCLabelBMFont* m_header = nullptr;
    std::vector<SlotNodes> m_slots;
    cocos2d::CCLabelBMFont* m_notice = nullptr;
};

} // namespace augment
