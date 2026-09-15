#include "RunHud.hpp"

using namespace geode::prelude;

namespace augment {

namespace {
    constexpr float kMargin = 8.f;
    constexpr float kLineHeight = 13.f;
    constexpr float kLineScale = 0.5f;
}

RunHud* RunHud::create() {
    auto ret = new RunHud();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RunHud::init() {
    if (!CCNode::init()) return false;

    auto winSize = CCDirector::get()->getWinSize();
    this->setContentSize(winSize);
    this->setAnchorPoint({ 0.f, 0.f });
    this->setPosition({ 0.f, 0.f });
    this->setID("run-hud"_spr);

    m_notice = CCLabelBMFont::create("", "goldFont.fnt");
    m_notice->setScale(0.7f);
    m_notice->setPosition({ winSize.width / 2, winSize.height * 0.7f });
    m_notice->setOpacity(0);
    m_notice->setID("notice");
    this->addChild(m_notice, 1);

    return true;
}

void RunHud::setLines(std::vector<std::string> const& lines) {
    auto winSize = CCDirector::get()->getWinSize();

    // Grow the label pool as needed.
    while (m_labels.size() < lines.size()) {
        auto label = CCLabelBMFont::create("", "chatFont.fnt");
        label->setScale(kLineScale);
        label->setAnchorPoint({ 0.f, 1.f });
        label->setPosition({ kMargin, winSize.height - kMargin - kLineHeight * m_labels.size() });
        this->addChild(label);
        m_labels.push_back(label);
    }

    for (size_t i = 0; i < m_labels.size(); i++) {
        if (i < lines.size()) {
            if (lines[i] != m_labels[i]->getString()) m_labels[i]->setString(lines[i].c_str());
            m_labels[i]->setVisible(true);
        }
        else {
            m_labels[i]->setVisible(false);
        }
    }
}

void RunHud::notice(std::string const& text, ccColor3B color) {
    m_notice->setString(text.c_str());
    m_notice->setColor(color);
    m_notice->stopAllActions();
    m_notice->setOpacity(255);
    m_notice->runAction(CCSequence::create(
        CCDelayTime::create(1.f),
        CCFadeOut::create(0.5f),
        nullptr
    ));
}

} // namespace augment
