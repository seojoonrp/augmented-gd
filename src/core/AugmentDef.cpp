#include "AugmentDef.hpp"

namespace augment {

std::string const& AugmentDef::describe(int level) const {
    if (level <= 1 || levelUpDesc.empty()) return initialDesc;
    return levelUpDesc;
}

// Text is the design table verbatim (docs/DESIGN.md). Number keys 1-9 grant
// augments in this order.
std::vector<AugmentDef> const& allAugments() {
    static std::vector<AugmentDef> const pool = {
        { ids::Shield, "결계인가?", 3,
            "매 어템마다 보호막이 지급됩니다.\n보호막이 깨지면 1.5초간 노클립 상태로 전환됩니다.",
            "보호막 개수가 하나 늘어납니다." },
        { ids::SlowMo, "나무늘보", 3,
            "게임 속도가 5% 감소합니다.\nX를 눌러 토글할 수 있습니다.",
            "게임 속도가 5% 더 감소합니다." },
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
            "위험 요소(빨간 히트박스)의 크기가 5% 감소합니다.",
            "위험 요소의 크기가 5% 더 감소합니다." },
        { ids::WaveHitbox, "웨이브브레이커", 5,
            "웨이브 모드일 때 플레이어 히트박스 크기가 10% 감소합니다.",
            "웨이브 모드일 때 플레이어 히트박스 크기가 10% 더 감소합니다." },
        { ids::Nerve, "청심환", 2,
            "레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다.\nX% 도달 시 두 능력의 효과가 각각 X% 증가합니다.",
            "X% 도달 시 두 능력의 효과가 각각 1.5X% 증가합니다." },
        { ids::DraftCount, "기회비용", 1,
            "다음 드래프트부터 카드가 4개씩 등장합니다.",
            "" },
        { ids::Cat, "고양이", 5,
            "마법 고양이를 소환합니다.\n고양이는 4초마다 시야에 있는 장애물 5개를 랜덤으로 제거합니다.",
            "고양이가 매번 장애물을 한 개 더 제거하고,\n제거 쿨타임이 0.5초 감소합니다." },
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
