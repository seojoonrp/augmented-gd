#pragma once

// The runs' own record per level, kept apart from GD's normal-mode best
// (user, 2026-09-28: a run on a level with no record used to write its
// percent into GD's). A run attempt never reaches GJGameLevel::savePercentage,
// GD's New Best! or the clear (GdRecordHook.cpp); instead the furthest any
// run got on a level is kept here, in Geode saved values so it survives
// restarts, and beating it shows GD's New Best! popup. The per-run best
// (RunState::bestPercent, which pays the gauge's new-best bonus) is a
// separate thing and untouched.

class GJGameLevel;
class PlayLayer;

namespace augment::records {

// Whole percent the runs on `level` have reached (0 = none yet, 100 = cleared
// with augments).
int best(GJGameLevel* level);
// Keeps `percent` when it beats best(level); true when it did.
bool submit(GJGameLevel* level, int percent);

// GD's own New Best! popup for `percent` — the one call hiddenFromGd() lets
// through.
void showNewBest(PlayLayer* layer, int percent);
bool showingOwnNewBest();

// Whether GD's own record keeping must not see what happens now: a run
// attempt is in progress, or a HideFromGd is alive.
bool hiddenFromGd();

// For the stretches where the session can no longer say so itself:
// levelComplete, which borrows m_isTestMode (runAttempt() reads it), and
// onQuit, where the session is dropped before GD's own onQuit runs.
class HideFromGd {
public:
    HideFromGd();
    ~HideFromGd();
    HideFromGd(HideFromGd const&) = delete;
    HideFromGd& operator=(HideFromGd const&) = delete;
};

} // namespace augment::records
