#include "AugmentCard.hpp"
#include "CardStyle.hpp"
#include "Fonts.hpp"

#include <Geode/ui/NineSlice.hpp>

#include <string>
#include <unordered_map>

using namespace geode::prelude;

namespace augment::card {

namespace {
    constexpr float kInset = 10.f;

    // Vertical slots, measured down from the card's top edge.
    constexpr float kNameY = 18.f;
    constexpr float kNameScale = 0.6f;
    constexpr float kImageTop = 34.f;
    constexpr float kImageWidth = 120.f;
    constexpr float kImageHeight = 70.f;
    constexpr float kImageBorder = 2.f;
    constexpr float kImageRadius = 4.5f;
    // Description block is centred between the image box and the footer band,
    // with this much breathing room above and below.
    constexpr float kDescMargin = 5.f;
    constexpr float kDescScale = 0.5f;
    constexpr float kDescMinScale = 0.35f;
    constexpr float kFooterHeight = 20.f;
    constexpr float kFooterRadius = 4.f;
    constexpr float kFooterScale = 0.5f;
    constexpr float kPipRadius = 3.8f;    // outline included
    constexpr float kPipSpacing = 7.f;

    // Label widths for one font, in label units: one probe label re-set per
    // string (setString reuses its glyph sprites; a fresh label per word
    // built every glyph again), and each width kept, because the card's
    // shrink loop wraps the same text again at every step.
    class TextMeasure {
    public:
        explicit TextMeasure(char const* font) : m_probe(CCLabelBMFont::create("", font)) {}

        float width(std::string const& s) {
            if (auto it = m_widths.find(s); it != m_widths.end()) return it->second;
            float w = 0.f;
            if (m_probe) {
                m_probe->setString(s.c_str());
                w = m_probe->getContentSize().width;
            }
            m_widths.emplace(s, w);
            return w;
        }

    private:
        Ref<CCLabelBMFont> m_probe;
        std::unordered_map<std::string, float> m_widths;
    };

    // Word-wrap by measuring words (TextMeasure). CCLabelBMFont's width
    // argument never wrapped our fonts in game (Pretendard or the baked
    // ImcreSoojin, 2026-09-17), so the label gets explicit newlines instead.
    // `maxWidth` is in label units (pre-scale). Existing newlines are kept.
    std::string wrapText(std::string const& text, TextMeasure& measure, float maxWidth) {
        std::string out;
        size_t paraStart = 0;
        while (paraStart <= text.size()) {
            size_t paraEnd = text.find('\n', paraStart);
            if (paraEnd == std::string::npos) paraEnd = text.size();
            std::string line;
            size_t wordStart = paraStart;
            while (wordStart <= paraEnd) {
                size_t wordEnd = text.find(' ', wordStart);
                if (wordEnd == std::string::npos || wordEnd > paraEnd) wordEnd = paraEnd;
                auto word = text.substr(wordStart, wordEnd - wordStart);
                if (!word.empty()) {
                    auto candidate = line.empty() ? word : line + " " + word;
                    if (!line.empty() && measure.width(candidate) > maxWidth) {
                        out += line + '\n';
                        line = word;
                    }
                    else {
                        line = candidate;
                    }
                }
                wordStart = wordEnd + 1;
            }
            out += line;
            if (paraEnd < text.size()) out += '\n';
            paraStart = paraEnd + 1;
        }
        return out;
    }
}

CCNode* augmentCard(AugmentDef const& def, CardFace const& face) {
    auto card = CCNode::create();
    card->setContentSize({ CardWidth, CardHeight });
    card->setAnchorPoint({ 0.5f, 0.5f });
    auto const centre = CCPoint{ CardWidth / 2, CardHeight / 2 };
    auto const fromTop = [](float dy) { return CCPoint{ CardWidth / 2, CardHeight - dy }; };
    float const inner = CardWidth - 2 * kInset;

    // White rim (full-size slices: its corner radius must cover the body's).
    auto rim = roundedBox({ CardWidth + 2 * Rim, CardHeight + 2 * Rim }, ccWHITE, RimRadius);
    rim->setPosition(centre);
    card->addChild(rim, 0);

    // Body: GD's green button, black ring and highlight included.
    auto body = NineSlice::create("GJ_button_01.png");
    body->setContentSize({ CardWidth, CardHeight });
    body->setPosition(centre);
    card->addChild(body, 1);

    // Footer band: the darker strip along the bottom of GD's big buttons.
    auto band = roundedBox({ CardWidth - 2 * Ring, kFooterHeight }, ccBLACK, kFooterRadius, 70);
    band->setPosition({ CardWidth / 2, Ring + kFooterHeight / 2 });
    card->addChild(band, 2);

    auto name = CCLabelBMFont::create(face.name.c_str(), fonts::Name);
    name->limitLabelWidth(inner, kNameScale, 0.3f);
    name->setPosition(fromTop(kNameY));
    card->addChild(name, 3);

    // Image slot: the augment's art in a bordered white panel (CardStyle.hpp).
    auto image = artSlot(def.id, { kImageWidth, kImageHeight }, kImageRadius, kImageBorder);
    image->setPosition(fromTop(kImageTop + kImageHeight / 2));
    card->addChild(image, 2);

    // Description: wrapped here (see wrapText) at the card's inner width and
    // shrunk in steps until it fits between the image box and the footer,
    // then centred in that gap.
    float const slotTop = kImageTop + kImageHeight + kDescMargin;
    float const slotBottom = CardHeight - Ring - kFooterHeight - kDescMargin;
    float const slot = slotBottom - slotTop;
    float descScale = kDescScale;
    CCLabelBMFont* desc = nullptr;
    TextMeasure measure(fonts::Text);
    for (;;) {
        auto wrapped = wrapText(face.description, measure, inner / descScale);
        desc = CCLabelBMFont::create(
            wrapped.c_str(), fonts::Text, kCCLabelAutomaticWidth, kCCTextAlignmentCenter
        );
        if (desc->getContentSize().height * descScale <= slot || descScale <= kDescMinScale + 1e-3f) break;
        descScale -= 0.05f;
    }
    if (desc->getContentSize().height * descScale > slot) {
        log::warn(
            "Card '{}' description still overflows at scale {:.2f} ({:.0f} > {:.0f} pt)",
            def.id, descScale, desc->getContentSize().height * descScale, slot
        );
    }
    desc->setScale(descScale);
    desc->setAnchorPoint({ 0.5f, 0.5f });
    desc->setPosition(fromTop((slotTop + slotBottom) / 2));
    card->addChild(desc, 3);

    // Footer: text on the left, drawn level stars on the right.
    auto footer = CCLabelBMFont::create(face.footer.c_str(), fonts::Text);
    footer->setScale(kFooterScale);
    footer->setColor(face.footerColor);
    footer->setAnchorPoint({ 0.f, 0.5f });
    footer->setPosition({ kInset, band->getPositionY() });
    card->addChild(footer, 3);

    auto pips = stars(face.pips, def.maxLevel, kPipRadius, kPipSpacing);
    pips->setAnchorPoint({ 1.f, 0.5f });
    pips->setPosition({ CardWidth - kInset, band->getPositionY() });
    card->addChild(pips, 3);
    return card;
}

} // namespace augment::card
