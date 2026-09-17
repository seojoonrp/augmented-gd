#include "DraftSession.hpp"
#include "AugmentManager.hpp"
#include "LevelSession.hpp"
#include "../ui/AugmentDraftPopup.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment::draft {

namespace {

void close();

bool g_directorPaused = false;
bool g_cursorWasHidden = false;

// Pauses CCDirector without dropping the frame rate to 4 fps.
void pauseDirector() {
    if (g_directorPaused) return;
    auto director = CCDirector::get();
    // CCDirector::pause() saves the current interval and drops to 4 fps;
    // put the real interval back so the popup stays responsive.
    // resume() restores the saved value, so nothing extra is needed there.
    auto interval = director->getAnimationInterval();
    director->pause();
    director->setAnimationInterval(interval);
    g_directorPaused = true;
}

void onPick(std::string const& id) {
    auto& mgr = AugmentManager::get();
    mgr.applyPick(id);

    // The PlayLayer may be gone by the time this runs, so look it up fresh.
    auto session = mgr.session();
    if (session) session->onGranted(id, mgr.levelOf(id));

    // More drafts waiting (a big new best can earn several): the next popup
    // opens right away and the game stays paused in between.
    if (session && mgr.hasPendingDraft()) {
        showNext();
        return;
    }
    close();
}

// Resume and put the cursor back the way the level had it.
void close() {
    abandon();
    if (g_cursorWasHidden) CCEGLView::get()->showCursor(false);
}

} // namespace

void showNext() {
    auto& mgr = AugmentManager::get();
    if (!mgr.hasPendingDraft()) return;
    mgr.clearPendingDraft();

    auto choices = mgr.rollDraft(mgr.draftCardCount());
    if (choices.empty()) {
        log::info("Draft: nothing left to draft, {} pending dropped", mgr.pendingDrafts());
        mgr.dropPendingDrafts();
        // Reached from a pick in a chain: the game is still paused for it.
        close();
        return;
    }
    log::info("Draft: showing {} cards, {} more pending", choices.size(), mgr.pendingDrafts());

    auto popup = AugmentDraftPopup::create(choices, onPick);
    if (!popup) return;

    // The director is about to be paused, which also freezes actions, so
    // the pop-in animation would never finish. Skip it.
    popup->m_noElasticity = true;
    popup->show();

    // Already paused for a previous draft in this chain: the cursor state
    // saved then is the one to restore, so don't overwrite it.
    if (g_directorPaused) return;

    // GD hides the cursor in levels; the draft needs it.
    auto view = CCEGLView::get();
    g_cursorWasHidden = view->m_bShouldHideCursor;
    view->showCursor(true);

    pauseDirector();
}

bool isOpen() {
    return g_directorPaused;
}

void abandon() {
    if (!g_directorPaused) return;
    CCDirector::get()->resume();
    g_directorPaused = false;
}

} // namespace augment::draft
