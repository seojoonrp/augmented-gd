#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace augment {

// PlayLayer overlay: draft gauge at the bottom centre, debug column on the
// left, notices in the bottom-left corner, berserk banner over the gauge.
class RunHud : public cocos2d::CCNode {
public:
    struct Slot {
        std::string name;   // mod font
        std::string state;  // per-attempt state, may be empty
    };

    static RunHud* create();

    // percentLabel = PlayLayer::m_percentageLabel, for its font and scale.
    // Null when GD's percent is hidden; falls back to bigFont.
    void attachGauge(cocos2d::CCLabelBMFont* percentLabel);
    // full: a gauge-earned draft is waiting, so the bar shows full and
    // threshold is that draft's cost. max: every augment maxed.
    // During a death reward this only records the target.
    void setGauge(float value, float threshold, bool full, bool max);
    // 0..1. The reward particles and numbers stay opaque.
    void setBarOpacity(float opacity);
    // Runs during GD's death delay. `at` is in this node's space; `before` and
    // `threshold` are the gauge as it was before the death.
    void playDeathReward(cocos2d::CCPoint at, float before, float normal, float bonus, float threshold);
    // Cuts a running reward short so it never spills into the next attempt.
    void settleGauge();
    void setHeader(std::string const& text);
    void setSlots(std::vector<Slot> const& slots);
    // Bottom-left line. A new one pushes the ones still up a line higher (3 max).
    void notice(std::string const& text);
    // Bigger line above the gauge; replaces whatever is showing.
    void banner(std::string const& text);

protected:
    bool init() override;
    void update(float dt) override;

    struct SlotNodes {
        cocos2d::CCNode* root = nullptr;
        cocos2d::CCLabelBMFont* name = nullptr;
        cocos2d::CCLabelBMFont* state = nullptr;
    };
    SlotNodes makeSlot(size_t index);

    // quadratic bezier from the death spot to the gauge tip
    struct Particle {
        cocos2d::CCPoint from, ctrl, to;
        float depart = 0.f;
        bool gold = false;
        bool landed = false;
    };
    struct RewardText {
        cocos2d::CCLabelBMFont* label = nullptr;
        bool used = false;
        float baseY = 0.f;
        float born = 0.f;   // seconds into the sequence
    };
    struct Reward {
        bool active = false;
        float t = 0.f;                              // seconds since the death
        float start = 0.f, mid = 0.f, end = 0.f;    // fill ratios: before / +normal / +bonus
        float fillAt = 0.f, goldFillAt = 0.f, mergeAt = 0.f, endAt = 0.f;
        float threshold = 0.f;
        std::vector<Particle> particles;
        struct Flash { cocos2d::CCPoint at; float born; bool gold; };
        std::vector<Flash> flashes;
    };
    void stepReward(float dt);
    void finishReward(float ratio);
    void drawFrame();
    void layoutFill();
    void setBarText(cocos2d::CCLabelBMFont* label, std::string const& text);
    void showGaugeText(std::string const& text);
    GLubyte barAlpha() const;
    cocos2d::CCPoint tipAt(float ratio);   // in this node's space
    void spawnParticles(int count, bool gold, cocos2d::CCPoint from, cocos2d::CCPoint to, float departAfter);
    void placeRewardText(RewardText& text, std::string const& str, cocos2d::CCPoint at, float dy, float born);
    void animateRewardText(RewardText& text);

    cocos2d::CCNode* m_gaugeBar = nullptr;       // rim + track + fills
    cocos2d::CCDrawNode* m_gaugeFrame = nullptr;
    float m_barOpacity = 1.f;
    cocos2d::CCLabelBMFont* m_draftLabel = nullptr;
    float m_gaugeTop = 0.f;                      // top of the gauge labels, 0 until attached
    cocos2d::CCDrawNode* m_gaugeFill = nullptr;  // green + gold segments
    cocos2d::ccColor3B m_goldTint = { 255, 210, 60 };   // blends into green after a reward
    float m_drawnGreen = -1.f;
    float m_drawnGold = -1.f;
    cocos2d::ccColor3B m_drawnTint = { 0, 0, 0 };
    cocos2d::CCLabelBMFont* m_gaugeLabel = nullptr;
    float m_trackLeft = 0.f;    // bar units
    float m_trackWidth = 0.f;
    float m_fillHeight = 0.f;
    float m_fillBottom = 0.f;
    float m_gaugeShown = 0.f;   // eased toward m_gaugeTarget
    float m_goldShown = 0.f;    // >= m_gaugeShown
    float m_gaugeTarget = 0.f;
    float m_gaugeValue = -1.f;  // -1 = never set
    float m_gaugeCost = -1.f;
    bool m_gaugeFull = false;
    bool m_gaugeMax = false;
    std::string m_gaugeText;    // shown once no reward is running

    Reward m_reward;
    cocos2d::CCDrawNode* m_particles = nullptr;
    RewardText m_rewardText;       // "+normal"
    RewardText m_rewardGoldText;   // "+bonus"

    cocos2d::CCLabelBMFont* m_header = nullptr;
    std::vector<SlotNodes> m_slots;

    // notice labels, reused oldest first
    struct Toast {
        cocos2d::CCLabelBMFont* label = nullptr;
        float age = -1.f;         // < 0 = idle
        float line = 0.f;         // 0 = bottom
        float lineShown = 0.f;
    };
    std::vector<Toast> m_toasts;
    std::size_t m_nextToast = 0;
    void stepToasts(float dt);

    struct Banner {
        cocos2d::CCLabelBMFont* label = nullptr;
        float age = -1.f;
    };
    Banner m_banner;
    void stepBanner(float dt);
};

} // namespace augment
