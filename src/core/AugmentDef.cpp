#include "AugmentDef.hpp"

#include <cmath>

namespace augment {

namespace {
    // "1.5" / "4" / "0.5": the shortest decimal that reads naturally in a
    // sentence. Only used for the handful of tune:: values the cards quote.
    std::string num(float v) {
        if (v == std::floor(v)) return std::to_string(static_cast<int>(v));
        std::string s = std::to_string(v);
        while (s.back() == '0') s.pop_back();
        return s;
    }
    // A step stored as a fraction (0.05) quoted as a percentage ("5").
    std::string pct(float step) { return num(std::round(step * 1000.f) / 10.f); }
    // Nerve multiplier as it appears in "X% 도달 시 … 1.5X% 증가": 1 → "X".
    std::string multX(float m) { return m == 1.f ? "X" : num(m) + "X"; }
    // Korean count words for the small steps the cat text uses.
    std::string countKo(int n) { return n == 1 ? "한 개" : std::to_string(n) + "개"; }
}

std::string const& AugmentDef::describe(int level) const {
    if (level <= 1 || levelUpDesc.empty()) return initialDesc;
    return levelUpDesc;
}

// Text is the design table (docs/DESIGN.md) with its numbers taken from
// tune::. Number keys 1-9 grant augments in this order (Shift+1 = the 10th, Shift+2 the 11th).
std::vector<AugmentDef> const& allAugments() {
    static std::vector<AugmentDef> const pool = {
        { ids::Shield, "결계인가?", 3,
            "매 어템마다 보호막이 지급됩니다.\n보호막이 깨지면 " + num(tune::NoclipSeconds) + "초간 노클립 상태로 전환됩니다.",
            "보호막 개수가 하나 늘어납니다." },
        { ids::SlowMo, "나무늘보", 3,
            "게임 속도가 " + pct(tune::SlowMoStep) + "% 감소합니다.\nX를 눌러 토글할 수 있습니다.",
            "게임 속도가 " + pct(tune::SlowMoStep) + "% 더 감소합니다." },
        { ids::StartPos, "스타트포스", 5,
            "매 어템마다 Z를 눌러 체크포인트를 찍을 수 있습니다.\n해당 어템에 죽으면 체크포인트에서 부활합니다.",
            "체크포인트를 한 번 더 찍을 수 있습니다." },
        { ids::Foresight, "사륜안", 1,
            "히트박스를 보여줍니다.",
            "" },
        { ids::Unmirror, "멀미약", 1,
            "레벨 내 모든 미러포탈을 제거합니다.",
            "" },
        { ids::HazardHitbox, "위협제거", 5,
            "위험 요소(빨간 히트박스)의 크기가 " + pct(tune::HazardStep) + "% 감소합니다.",
            "위험 요소의 크기가 " + pct(tune::HazardStep) + "% 더 감소합니다." },
        { ids::WaveHitbox, "웨이브브레이커", 5,
            "웨이브 모드일 때 플레이어 히트박스 크기가 " + pct(tune::WaveStep) + "% 감소합니다.",
            "웨이브 모드일 때 플레이어 히트박스 크기가 " + pct(tune::WaveStep) + "% 더 감소합니다." },
        { ids::Nerve, "청심환", 2,
            "레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다.\nX% 도달 시 두 능력의 효과가 각각 "
                + multX(tune::NerveMult[0]) + "% 증가합니다.",
            "X% 도달 시 두 능력의 효과가 각각 " + multX(tune::NerveMult[1]) + "% 증가합니다." },
        { ids::DraftCount, "기회비용", 1,
            "다음 드래프트부터 카드가 " + std::to_string(tune::DraftCountCards) + "개씩 등장합니다.",
            "" },
        { ids::Cat, "고양이", 5,
            "마법 고양이를 소환합니다.\n고양이는 " + num(tune::CatBaseInterval) + "초마다 시야에 있는 장애물 "
                + std::to_string(tune::CatBaseCount) + "개를 랜덤으로 제거합니다.",
            "고양이가 매번 장애물을 " + countKo(tune::CatCountStep) + " 더 제거하고,\n제거 쿨타임이 "
                + num(tune::CatIntervalStep) + "초 감소합니다." },
        { ids::Brake, "브레이크", 3,
            "C를 누르고 있으면 게임 속도가 " + pct(tune::BrakeCut) + "% 감소합니다.\n어템마다 최대 "
                + num(tune::BrakeSecondsPerLevel) + "초씩 사용할 수 있습니다.",
            "[브레이크]를 어템마다 " + num(tune::BrakeSecondsPerLevel) + "초 더 사용할 수 있습니다." },
    };
    return pool;
}

AugmentDef const* findAugment(std::string const& id) {
    for (auto const& def : allAugments()) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

std::string augmentName(std::string const& id) {
    if (auto def = findAugment(id)) return def->name;
    return id;
}

} // namespace augment
