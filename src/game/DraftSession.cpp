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
// a popup is up right now (its draft stays pending until the pick)
bool g_showing = false;

void pauseDirector() {
    if (g_directorPaused) return;
    auto director = CCDirector::get();
    // pause() drops to 4 fps; put the interval back so the popup stays
    // responsive. resume() restores it by itself.
    auto interval = director->getAnimationInterval();
    director->pause();
    director->setAnimationInterval(interval);
    g_directorPaused = true;
}

void onPick(std::string const& id) {
    auto& mgr = AugmentManager::get();
    g_showing = false;
    // taken only now: leaving the level mid-draft keeps it for the next visit
    mgr.clearPendingDraft();
    mgr.applyPick(id);

    // the layer may be gone by now
    auto session = mgr.session();
    if (session) session->onGranted(id, mgr.levelOf(id));

    // more waiting: next popup right away, still paused
    if (session && mgr.hasPendingDraft()) {
        showNext();
        return;
    }
    close();
}

// resume + give the level its cursor state back
void close() {
    abandon();
    if (g_cursorWasHidden) CCEGLView::get()->showCursor(false);
}

} // namespace

void showNext() {
    auto& mgr = AugmentManager::get();
    if (g_showing || !mgr.hasPendingDraft()) return;

    auto choices = mgr.rollDraft(mgr.draftCardCount());
    if (choices.empty()) {
        mgr.dropPendingDrafts();
        // may be mid-chain, still paused
        close();
        return;
    }

    auto popup = AugmentDraftPopup::create(choices, onPick);
    if (!popup) return;
    g_showing = true;

    // the director pause freezes actions too, the pop-in would never finish
    popup->m_noElasticity = true;
    popup->show();

    // mid-chain: keep the cursor state the first popup saved
    if (g_directorPaused) return;

    // GD hides the cursor in levels
    auto view = CCEGLView::get();
    g_cursorWasHidden = view->m_bShouldHideCursor;
    view->showCursor(true);

    pauseDirector();
}

bool isOpen() {
    return g_directorPaused;
}

void abandon() {
    // the popup goes with the scene, its draft stays pending
    g_showing = false;
    if (!g_directorPaused) return;
    CCDirector::get()->resume();
    g_directorPaused = false;
}

} // namespace augment::draft
