#pragma once

// The runs' own per-level record, apart from GD's normal-mode best. Run
// attempts never reach GD's savePercentage / New Best! / clear (GdRecordHook);
// the furthest any run got is kept here in Geode saved values instead.
// Not the same as RunState::bestPercent (per run, pays the gauge bonus).

class GJGameLevel;
class PlayLayer;

namespace augment::records {

// whole percent, 0 = none yet, 100 = cleared
int best(GJGameLevel* level);
// true when `percent` beat best(level) and was kept
bool submit(GJGameLevel* level, int percent);

// GD's New Best! popup for `percent`, the one call hiddenFromGd() lets through
void showNewBest(PlayLayer* layer, int percent);
bool showingOwnNewBest();

// true while a run attempt is on or a HideFromGd is alive
bool hiddenFromGd();

// For when the session can't tell: levelComplete (borrows m_isTestMode, which
// runAttempt() reads) and onQuit (session is dropped before GD's onQuit runs).
class HideFromGd {
public:
    HideFromGd();
    ~HideFromGd();
    HideFromGd(HideFromGd const&) = delete;
    HideFromGd& operator=(HideFromGd const&) = delete;
};

} // namespace augment::records
