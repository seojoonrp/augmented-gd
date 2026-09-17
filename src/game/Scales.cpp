#include "Scales.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace augment::scales {

void setTime(float scale) {
    if (std::abs(g_time - scale) < 0.001f) return;
    g_time = scale;

    auto engine = FMODAudioEngine::sharedEngine();
    FMOD::ChannelGroup* master = nullptr;
    if (engine && engine->m_system && engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
        master->setPitch(scale);
    }
    log::info("Game speed -> {:.2f}", scale);
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
    setTime(1.f);
    setHazard(1.f);
    setWave(1.f);
}

} // namespace augment::scales
