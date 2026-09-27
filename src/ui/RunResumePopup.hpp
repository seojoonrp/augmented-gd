#pragma once

#include <Geode/ui/Popup.hpp>

#include <functional>

namespace augment {

// Shown by the AUG button when the level already has a run: a green card
// in the draft cards' style (ImcreSoojin text), GD Restart / Continue
// buttons under it, the close button cancels. A level without a run skips
// this and starts directly.
class RunResumePopup : public geode::Popup {
public:
    using Callback = std::function<void()>;

    // Callbacks run after the popup has been removed.
    static RunResumePopup* create(Callback onRestart, Callback onContinue);

protected:
    bool init(Callback onRestart, Callback onContinue);
    void choose(bool restart);

    Callback m_onRestart;
    Callback m_onContinue;
};

} // namespace augment
