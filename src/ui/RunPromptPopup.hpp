#pragma once

#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>

namespace augment {

// Two-button prompt behind the AUG button on the level info screen: resume
// this level's run, or start here and drop the run on another level.
class RunPromptPopup : public geode::Popup {
public:
    using Callback = std::function<void()>;

    struct Button {
        char const* label;     // goldFont, so English only
        char const* texture;   // GJ_button_0N.png
        Callback onClick;      // runs after the popup is gone; empty = just close
    };

    // Restart / Continue
    static RunPromptPopup* resume(Callback onRestart, Callback onContinue);
    // Cancel / Start (ends the run on otherLevel)
    static RunPromptPopup* replace(std::string const& otherLevel, Callback onStart);

protected:
    // name goes into the node ID
    bool init(char const* name, std::string const& title, std::string const& message, Button left, Button right);
    void choose(Button const& button);

    std::string m_name;
    Button m_left;
    Button m_right;
};

} // namespace augment
