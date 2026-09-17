#include "Scales.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace augment::scales {

namespace {

void applyTime() {
    float scale = g_timeOverride > 0.f ? g_timeOverride : g_timeBase;
    if (std::abs(g_time - scale) < 0.001f) return;
    g_time = scale;

    auto engine = FMODAudioEngine::sharedEngine();
    FMOD::ChannelGroup* master = nullptr;
    if (engine && engine->m_system && engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
        master->setPitch(scale);
    }
    log::info("Game speed -> {:.2f}", scale);
}

} // namespace

void setTime(float scale) {
    g_timeBase = scale;
    applyTime();
}

void setTimeOverride(float scale) {
    g_timeOverride = scale;
    applyTime();
}

void resetTime() {
    g_timeBase = 1.f;
    g_timeOverride = 0.f;
    applyTime();
}

void setHazard(float scale) {
    if (std::abs(g_hazard - scale) < 0.001f) return;
    g_hazard = scale;
    log::info("HazardHitbox: hazard hitbox scale -> {:.2f}", scale);
}

void setWave(float scale) {
    if (std::abs(g_wave - scale) < 0.001f) return;
    g_wave = scale;
    log::info("WaveHitbox: player hitbox scale -> {:.2f}", scale);
}

void resetAll() {
    resetTime();
    setHazard(1.f);
    setWave(1.f);
}

} // namespace augment::scales
