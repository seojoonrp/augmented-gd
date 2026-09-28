#include "AugmentInfoPopup.hpp"
#include "AugmentCard.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {
    // A bit bigger than in a draft so the text reads comfortably; 1.3 (273 pt)
    // filled the screen's height and felt heavy (user), 1.1 is 231 pt.
    constexpr float kCardScale = 1.1f;
    // Lighter than the summary's overlay, which is already under it.
    constexpr GLubyte kOverlayOpacity = 120;
    constexpr ccColor3B kGold = { 255, 215, 60 };
}

AugmentInfoPopup* AugmentInfoPopup::create(AugmentDef const& def, int level) {
    auto ret = new AugmentInfoPopup();
    if (ret->init(def, level)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AugmentInfoPopup::init(AugmentDef const& def, int level) {
    float const width = card::CardWidth * kCardScale;
    float const height = card::CardHeight * kCardScale;
    if (!Popup::init(width, height)) return false;
    this->setID("augment-info-popup"_spr);
    m_bgSprite->setVisible(false);
    this->setOpacity(kOverlayOpacity);

    card::CardFace face;
    face.description = def.describeAt(level);
    face.footer = fmt::format("Lv {}", level);
    if (level >= def.maxLevel) face.footerColor = kGold;
    face.pips = level;
    auto node = card::augmentCard(def, face);
    node->setScale(kCardScale);
    node->setPosition({ width / 2, height / 2 });
    m_mainLayer->addChild(node, 1);

    log::info("AugmentInfoPopup: '{}' at Lv {}/{}", def.id, level, def.maxLevel);
    return true;
}

} // namespace augment
