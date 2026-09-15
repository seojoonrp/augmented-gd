#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace augment {

// Small text overlay in the top-left of PlayLayer: draft gauge + one line per
// owned augment, plus a transient centered notice ("SHIELD BROKEN" etc).
class RunHud : public cocos2d::CCNode {
public:
    static RunHud* create();

    // Replace all lines. Only touches labels whose text actually changed.
    void setLines(std::vector<std::string> const& lines);
    // Show a big message in the middle of the screen that fades out.
    void notice(std::string const& text, cocos2d::ccColor3B color = { 255, 255, 255 });

protected:
    bool init() override;

    std::vector<cocos2d::CCLabelBMFont*> m_labels;
    cocos2d::CCLabelBMFont* m_notice = nullptr;
};

} // namespace augment
