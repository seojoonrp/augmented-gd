// Keeps run attempts out of GD's own records: no saved percent, no GD New Best,
// and the end screen says it was a run. Same two blocks as qolmod's safe mode.
// The clear itself is handled in PlayLayerHook (levelComplete).

#include "../game/AugmentManager.hpp"
#include "../game/LevelSession.hpp"
#include "../game/Records.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace augment;

class $modify(AugRecordLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (records::hiddenFromGd()) return;
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

class $modify(AugRecordPlayLayer, PlayLayer) {
    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (records::hiddenFromGd() && !records::showingOwnNewBest()) return;
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }
};

namespace {
    constexpr char const* kClearMessage = "Cleared with augments!";
}

class $modify(AugRecordEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();
        auto s = AugmentManager::get().sessionFor(m_playLayer);
        if (!s || !s->runCleared() || !m_mainLayer) return;

        // node-ids: the random quote is `complete-message` (TextArea), the plain
        // label before it `end-text`
        if (auto area = typeinfo_cast<TextArea*>(m_mainLayer->getChildByID("complete-message"))) {
            area->setString(kClearMessage);
        }
        else if (auto label = typeinfo_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("end-text"))) {
            label->setString(kClearMessage);
            label->limitLabelWidth(320.f, label->getScale(), 0.2f);
        }
        else {
            log::warn("EndLevelLayer: no complete-message or end-text to rewrite");
        }
    }
};
