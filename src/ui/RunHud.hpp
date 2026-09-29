#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace augment {

// PlayLayer overlay: a draft gauge at the bottom centre (a rounded bar that
// fills left to right, "DRAFT" over its left end and "charge/cost" over its
// right end, in GD's percent font); a debug-mode column at the far left with
// one row per owned augment; short notice lines that rise and sink near the
// bottom-left corner; and a banner above the gauge (berserk).
class RunHud : public cocos2d::CCNode {
public:
    struct Slot {
        std::string name;   // Korean augment name (mod font)
        std::string state;  // English per-attempt state, may be empty
    };

    static RunHud* create();

    // Builds the gauge at the bottom centre: a rounded bar with "DRAFT" over
    // its left end and the count over its right end, in the font of
    // `percentLabel` (PlayLayer::m_percentageLabel, 90 % of its scale) —
    // bigFont at 0.5 when that is null (hidden in settings).
    void attachGauge(cocos2d::CCLabelBMFont* percentLabel);
    // Gauge charge against a cost; shown as "value/threshold" and as the
    // fill ratio. `full` = a gauge-earned draft is waiting: the bar shows
    // full and `threshold` is that draft's cost, read as e.g. "30/30". While
    // a death reward plays this only records the target.
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
    // One short player-facing line (Korean, UI font) near the bottom-left
    // corner: it rises into place fading in, holds, then sinks fading out. A
    // second line while the first is up pushes it a line higher, three at most.
    void notice(std::string const& text);
    // One bigger line centred just above the draft gauge (berserk), with the
    // same rise-and-fade in and out; a new one replaces the one showing.
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

    cocos2d::CCNode* m_gaugeBar = nullptr;       // rim + track + fills, bottom centre
    cocos2d::CCLabelBMFont* m_draftLabel = nullptr;   // "DRAFT" over the bar's left end
    float m_gaugeTop = 0.f;                      // top of the gauge's labels (0 until attached)
    // The fill: the green segment and the new-best (gold) one right after it,
    // both rounded, redrawn by layoutFill() when a width or the tint moves.
    cocos2d::CCDrawNode* m_gaugeFill = nullptr;   // child of m_gaugeBar
    cocos2d::ccColor3B m_goldTint = { 255, 210, 60 };   // blends into green after a reward
    float m_drawnGreen = -1.f;
    float m_drawnGold = -1.f;
    cocos2d::ccColor3B m_drawnTint = { 0, 0, 0 };
    cocos2d::CCLabelBMFont* m_gaugeLabel = nullptr;
    float m_trackLeft = 0.f;    // fill inset inside the bar sprite (bar units)
    float m_trackWidth = 0.f;
    float m_fillHeight = 0.f;
    float m_fillBottom = 0.f;
    float m_gaugeShown = 0.f;   // 0..1, eased toward m_gaugeTarget every frame
    float m_goldShown = 0.f;    // 0..1, where the gold segment ends (>= m_gaugeShown)
    float m_gaugeTarget = 0.f;
    float m_gaugeValue = -1.f;  // last setGauge's arguments (-1 = never set)
    float m_gaugeCost = -1.f;
    bool m_gaugeFull = false;   // last setGauge said a draft is waiting
    std::string m_gaugeText;    // last setGauge readout; applied once no reward runs

    Reward m_reward;
    cocos2d::CCDrawNode* m_particles = nullptr;   // all reward particles, redrawn every frame
    RewardText m_rewardText;       // "+normal" beside the dead icon
    RewardText m_rewardGoldText;   // "+bonus" under it

    cocos2d::CCLabelBMFont* m_header = nullptr;
    std::vector<SlotNodes> m_slots;

    // Notice lines, reused round-robin (oldest first), animated by
    // stepToasts() from update() on real time.
    struct Toast {
        cocos2d::CCLabelBMFont* label = nullptr;
        float age = -1.f;         // seconds since shown; < 0 = idle
        float line = 0.f;         // stack line it belongs on (0 = bottom)
        float lineShown = 0.f;    // eased toward `line`
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
