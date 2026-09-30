#include "AugmentDef.hpp"
#include "Formulas.hpp"

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
    // Korean count words for the small steps the cat text uses.
    std::string countKo(int n) { return n == 1 ? "한 개" : std::to_string(n) + "개"; }
    // English amounts with their unit: "1 second", "1.5 seconds", "3 blocks".
    std::string amountEn(float v, char const* one, char const* many) {
        return num(v) + " " + (v == 1.f ? one : many);
    }
    std::string secondsEn(float v) { return amountEn(v, "second", "seconds"); }
    std::string blocksEn(float v) { return amountEn(v, "block", "blocks"); }

    static_assert(tune::NerveSlope == 0.5f, "the nerve card spells the slope as X/2");
}

std::string const& AugmentDef::describe(int level, Lang lang) const {
    if (level <= 1 || levelUpDesc.in(lang).empty()) return initialDesc.in(lang);
    if (lateFrom > 0 && level >= lateFrom && !lateLevelUpDesc.in(lang).empty()) return lateLevelUpDesc.in(lang);
    return levelUpDesc.in(lang);
}

// The initial text's sentences with level `n`'s numbers from formula::, so
// the detail card and the game cannot disagree.
std::string AugmentDef::describeAt(int level, Lang lang) const {
    if (level <= 1 || maxLevel <= 1) return initialDesc.in(lang);
    int const n = std::min(level, maxLevel);
    bool const ko = lang == Lang::Korean;
    if (id == ids::Shield) {
        if (ko) {
            return "매 어템마다 보호막이 " + std::to_string(n) + "개 지급됩니다.\n보호막이 깨지면 "
                + num(tune::NoclipSeconds) + "초간 노클립 상태로 전환됩니다.";
        }
        return "Get " + std::to_string(n) + " shields every attempt.\nWhen one breaks, you noclip for "
            + secondsEn(tune::NoclipSeconds) + ".";
    }
    if (id == ids::SlowMo) {
        if (ko) return "게임 속도가 " + pct(1.f - formula::slowMoScale(n)) + "% 감소합니다.\nX를 눌러 토글할 수 있습니다.";
        return "The game runs " + pct(1.f - formula::slowMoScale(n)) + "% slower.\nPress X to toggle it.";
    }
    if (id == ids::StartPos) {
        if (ko) {
            return "매 어템마다 Z를 눌러 체크포인트를 최대 " + std::to_string(n) + "번 찍을 수 있습니다.\n"
                "해당 어템에 죽으면 체크포인트에서 부활합니다.";
        }
        return "Press Z to place a checkpoint up to " + std::to_string(n) + " times per attempt.\n"
            "If you die, you respawn at the last one.";
    }
    if (id == ids::HazardHitbox) {
        if (ko) return "위험 요소(빨간 히트박스)의 크기가 " + pct(tune::HazardStep * n) + "% 감소합니다.";
        return "Hazard hitboxes (the red ones) shrink by " + pct(tune::HazardStep * n) + "%.";
    }
    if (id == ids::WaveHitbox) {
        if (ko) return "웨이브 모드일 때 플레이어 히트박스 크기가 " + pct(tune::WaveStep * n) + "% 감소합니다.";
        return "Your hitbox shrinks by " + pct(tune::WaveStep * n) + "% in wave mode.";
    }
    if (id == ids::Cat) {
        if (ko) {
            return "마법 고양이를 소환합니다.\n고양이는 " + num(formula::catInterval(n)) + "초마다 시야에 있는 위험 요소 "
                + std::to_string(formula::catCount(n)) + "개를 랜덤으로 제거합니다.";
        }
        return "Summons a magic cat.\nEvery " + secondsEn(formula::catInterval(n)) + " it removes "
            + std::to_string(formula::catCount(n)) + " random hazards in view.";
    }
    if (id == ids::Brake) {
        if (ko) {
            return "C를 누르고 있으면 게임 속도가 " + pct(tune::BrakeCut) + "% 감소합니다.\n어템마다 최대 "
                + num(formula::brakeBudget(n)) + "초씩 사용할 수 있습니다.";
        }
        return "Hold C to slow the game down by " + pct(tune::BrakeCut) + "%.\nUp to "
            + secondsEn(formula::brakeBudget(n)) + " per attempt.";
    }
    if (id == ids::Missile) {
        if (ko) {
            return num(formula::missileInterval(n)) + "초마다 시야 내 위험 요소 하나에 미사일이 떨어집니다.\n반경 "
                + num(formula::missileRadius(n) / tune::BlockUnits) + "칸 안의 위험 요소가 모두 제거됩니다.";
        }
        return "Every " + secondsEn(formula::missileInterval(n)) + ", a missile hits a hazard in view.\n"
            "Every hazard within " + blocksEn(formula::missileRadius(n) / tune::BlockUnits) + " is removed.";
    }
    if (id == ids::Berserk) {
        if (ko) {
            return "위험 요소가 파괴될 때마다 " + pct(formula::berserkChance(n)) + "% 확률로\n"
                + num(formula::berserkSeconds(n)) + "초간 버서커 모드에 돌입합니다.\n"
                "버서커 모드에서는 부딪히는 위험 요소가 모두 파괴됩니다.";
        }
        return "Each destroyed hazard has a " + pct(formula::berserkChance(n)) + "% chance "
            "to set off " + secondsEn(formula::berserkSeconds(n)) + " of berserk mode.\n"
            "In berserk mode, hazards you hit are destroyed.";
    }
    // A multi-level augment without its own sentence here: the initial text
    // is still true, only its numbers are the first level's.
    return initialDesc.in(lang);
}

// Text is the design table (docs/DESIGN.md) with its numbers taken from
// tune::, English first. Number keys 1-9 grant augments in this order
// (Shift+1 = the 10th, Shift+2 the 11th, Shift+3 the 12th, Shift+4 the 13th).
std::vector<AugmentDef> const& allAugments() {
    static std::vector<AugmentDef> const pool = {
        { ids::Shield, { "Shield", "결계인가?" }, 5,
            { "Get a shield every attempt.\nWhen it breaks, you noclip for " + secondsEn(tune::NoclipSeconds) + ".",
              "매 어템마다 보호막이 지급됩니다.\n보호막이 깨지면 " + num(tune::NoclipSeconds) + "초간 노클립 상태로 전환됩니다." },
            { "One more shield every attempt.",
              "보호막 개수가 하나 늘어납니다." } },
        { ids::SlowMo, { "Sloth", "나무늘보" }, 5,
            { "The game runs " + pct(tune::SlowMoStep) + "% slower.\nPress X to toggle it.",
              "게임 속도가 " + pct(tune::SlowMoStep) + "% 감소합니다.\nX를 눌러 토글할 수 있습니다." },
            { "The game runs another " + pct(tune::SlowMoStep) + "% slower.",
              "게임 속도가 " + pct(tune::SlowMoStep) + "% 더 감소합니다." } },
        { ids::StartPos, { "Checkpoint", "스타트포스" }, 3,
            { "Press Z to place a checkpoint once per attempt.\nIf you die, you respawn there.",
              "매 어템마다 Z를 눌러 체크포인트를 찍을 수 있습니다.\n해당 어템에 죽으면 체크포인트에서 부활합니다." },
            { "One more checkpoint every attempt.",
              "체크포인트를 한 번 더 찍을 수 있습니다." } },
        { ids::Foresight, { "Foresight", "사륜안" }, 1,
            { "Shows hitboxes.",
              "히트박스를 보여줍니다." },
            {} },
        { ids::Unmirror, { "Unmirror", "멀미약" }, 1,
            { "Removes every mirror portal in the level.",
              "레벨 내 모든 미러포탈을 제거합니다." },
            {} },
        { ids::HazardHitbox, { "Threat Removal", "위협제거" }, 7,
            { "Hazard hitboxes (the red ones) shrink by " + pct(tune::HazardStep) + "%.",
              "위험 요소(빨간 히트박스)의 크기가 " + pct(tune::HazardStep) + "% 감소합니다." },
            { "Hazard hitboxes shrink by another " + pct(tune::HazardStep) + "%.",
              "위험 요소의 크기가 " + pct(tune::HazardStep) + "% 더 감소합니다." } },
        { ids::WaveHitbox, { "Wave Breaker", "웨이브브레이커" }, 7,
            { "Your hitbox shrinks by " + pct(tune::WaveStep) + "% in wave mode.",
              "웨이브 모드일 때 플레이어 히트박스 크기가 " + pct(tune::WaveStep) + "% 감소합니다." },
            { "Your hitbox shrinks by another " + pct(tune::WaveStep) + "% in wave mode.",
              "웨이브 모드일 때 플레이어 히트박스 크기가 " + pct(tune::WaveStep) + "% 더 감소합니다." } },
        { ids::Nerve, { "Calm Nerves", "청심환" }, 1,
            { "The further into the level, the stronger [Threat Removal] and [Wave Breaker] get.\nAt X% progress, both work at (" + pct(1.f + tune::NerveBase) + " + X/2)% of their strength.",
              "레벨 후반에 도달할수록 [위협제거]와 [웨이브브레이커]의 효과가 증가합니다.\nX% 도달 시 두 능력의 효과가 각각 기존의 (" + pct(1.f + tune::NerveBase) + " + X/2)%가 됩니다." },
            {} },
        { ids::DraftCount, { "Opportunity Cost", "기회비용" }, 1,
            { "From the next draft on, drafts show " + std::to_string(tune::DraftCountCards) + " cards.",
              "다음 드래프트부터 카드가 " + std::to_string(tune::DraftCountCards) + "개씩 등장합니다." },
            {} },
        { ids::Cat, { "Cat", "고양이" }, 7,
            { "Summons a magic cat.\nEvery " + secondsEn(tune::CatBaseInterval) + " it removes "
                  + std::to_string(tune::CatBaseCount) + " random hazards in view.",
              "마법 고양이를 소환합니다.\n고양이는 " + num(tune::CatBaseInterval) + "초마다 시야에 있는 위험 요소 "
                  + std::to_string(tune::CatBaseCount) + "개를 랜덤으로 제거합니다." },
            { "The cat removes " + std::to_string(tune::CatCountStep)
                  + (tune::CatCountStep == 1 ? " more hazard" : " more hazards") + " and casts "
                  + secondsEn(tune::CatIntervalStep) + " sooner.",
              "고양이가 매번 위험 요소를 " + countKo(tune::CatCountStep) + " 더 제거하고,\n제거 쿨타임이 "
                  + num(tune::CatIntervalStep) + "초 감소합니다." },
            { "The cat removes " + std::to_string(tune::CatLateCountStep) + " more hazards.",
              "고양이가 매번 위험 요소를 " + countKo(tune::CatLateCountStep) + " 더 제거합니다." },
            tune::CatLateLevel },
        { ids::Brake, { "Brake", "브레이크" }, 3,
            { "Hold C to slow the game down by " + pct(tune::BrakeCut) + "%.\nUp to "
                  + secondsEn(tune::BrakeSecondsPerLevel) + " per attempt.",
              "C를 누르고 있으면 게임 속도가 " + pct(tune::BrakeCut) + "% 감소합니다.\n어템마다 최대 "
                  + num(tune::BrakeSecondsPerLevel) + "초씩 사용할 수 있습니다." },
            { "[Brake] lasts " + secondsEn(tune::BrakeSecondsPerLevel) + " longer per attempt.",
              "[브레이크]를 어템마다 " + num(tune::BrakeSecondsPerLevel) + "초 더 사용할 수 있습니다." } },
        { ids::Missile, { "Air Raid", "공습경보" }, 5,
            { "Every " + secondsEn(tune::MissileBaseInterval) + ", a missile hits a hazard in view.\n"
                  "Every hazard within " + blocksEn(tune::MissileBaseRadius / tune::BlockUnits) + " is removed.",
              num(tune::MissileBaseInterval) + "초마다 시야 내 위험 요소 하나에 미사일이 떨어집니다.\n반경 "
                  + num(tune::MissileBaseRadius / tune::BlockUnits) + "칸 안의 위험 요소가 모두 제거됩니다." },
            { "The blast grows by " + blocksEn(tune::MissileRadiusStep / tune::BlockUnits) + " and missiles come "
                  + secondsEn(tune::MissileIntervalStep) + " sooner.",
              "폭발 반경이 " + num(tune::MissileRadiusStep / tune::BlockUnits) + "칸 커지고,\n미사일 쿨타임이 "
                  + num(tune::MissileIntervalStep) + "초 감소합니다." } },
        { ids::Berserk, { "Berserker", "버서커" }, 3,
            { "Each destroyed hazard has a " + pct(tune::BerserkChanceBase) + "% chance "
                  "to set off " + secondsEn(tune::BerserkSeconds) + " of berserk mode.\n"
                  "In berserk mode, hazards you hit are destroyed.",
              "위험 요소가 파괴될 때마다 " + pct(tune::BerserkChanceBase) + "% 확률로\n"
                  + num(tune::BerserkSeconds) + "초간 버서커 모드에 돌입합니다.\n"
                  "버서커 모드에서는 부딪히는 위험 요소가 모두 파괴됩니다." },
            { "The berserk chance goes up by " + pct(tune::BerserkChanceStep) + "%.",
              "버서커 모드 발동 확률이 " + pct(tune::BerserkChanceStep) + "% 증가합니다." } },
    };
    return pool;
}

AugmentDef const* findAugment(std::string const& id) {
    for (auto const& def : allAugments()) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

std::string augmentName(std::string const& id, Lang lang) {
    if (auto def = findAugment(id)) return def->name.in(lang);
    return id;
}

} // namespace augment
