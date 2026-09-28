// Keeps GD's own records away from run attempts (Records.hpp): no normal
// percent saved, no GD New Best!, and a clear's end screen that says it was
// a run. The pattern is qolmod's safe mode (`refs/qolmod/src/SafeMode/Hooks.cpp:31-78`),
// which blocks exactly these two calls to keep a level's progress untouched.
// The clear itself is blocked in PlayLayerHook.cpp (levelComplete).

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
        if (records::hiddenFromGd()) {
            log::info("savePercentage({}%, practice {}) kept from GD: run attempt", percent, isPracticeMode);
            return;
        }
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

class $modify(AugRecordPlayLayer, PlayLayer) {
    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (records::hiddenFromGd() && !records::showingOwnNewBest()) {
            log::info("showNewBest kept from GD: run attempt (orbs {}, diamonds {})", orbs, diamonds);
            return;
        }
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

        // node-ids names the random completion quote `complete-message`
        // (TextArea) and a plain label before it `end-text`; qolmod's safe
        // mode rewrites the same two (`refs/qolmod/src/SafeMode/Hooks.cpp:141-156`).
        if (auto area = typeinfo_cast<TextArea*>(m_mainLayer->getChildByID("complete-message"))) {
            area->setString(kClearMessage);
            log::info("EndLevelLayer: run clear, complete-message rewritten");
        }
        else if (auto label = typeinfo_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("end-text"))) {
            label->setString(kClearMessage);
            label->limitLabelWidth(320.f, label->getScale(), 0.2f);
            log::info("EndLevelLayer: run clear, end-text rewritten");
        }
        else {
            // Nothing to rewrite: list what is there so the next round knows.
            std::string ids;
            for (auto child : CCArrayExt<CCNode*>(m_mainLayer->getChildren())) {
                auto id = std::string(child->getID());
                ids += id.empty() ? std::string("?") : id;
                ids += ' ';
            }
            log::info("EndLevelLayer: run clear, no complete-message / end-text; children: {}", ids);
        }
    }
};
