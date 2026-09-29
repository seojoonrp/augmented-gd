#include "Sfx.hpp"

#include <Geode/Geode.hpp>

#include <iterator>

using namespace geode::prelude;

namespace augment::sfx {

namespace {

struct CueDef {
    char const* name;      // for the log
    // Ours (resources/sfx, cut by scripts/sfxcut.py), or a GD resource.
    char const* file;
    float gain;            // times GD's SFX volume
    // Level sounds slow down with the game (slow-mo and the brake pitch the
    // master group, Scales.cpp); menu sounds keep their own pitch.
    bool gameTime;
};

// Indexed by Cue. The user's picks (2026-09-29), cut from
// resources/sfx/source/ by scripts/sfxcut.py; each file is peak-normalized,
// so the gains here set the balance. The draft's opening sound and the cat's
// were dropped, and so was the missile's falling whistle.
constexpr CueDef kCues[] = {
    { "card-hover", "hover.wav"_spr, 0.55f, false },   // 0.4 until the user's word, 2026-09-29
    { "card-pick", "select.wav"_spr, 0.7f, false },
    { "missile-impact", "missile.wav"_spr, 1.f, true }, // 0.8 before, same day
};
constexpr std::size_t kCueCount = std::size(kCues);

FMOD::Sound* g_sounds[kCueCount] = {};
bool g_tried[kCueCount] = {};
bool g_played[kCueCount] = {};

// Loaded on first use and kept: a few short sounds, decoded into memory.
FMOD::Sound* soundFor(FMOD::System* system, std::size_t i) {
    if (g_tried[i]) return g_sounds[i];
    g_tried[i] = true;
    // Search paths cover GD's Resources, texture packs and our own folder.
    std::string const path = CCFileUtils::get()->fullPathForFilename(kCues[i].file, true);
    FMOD::Sound* sound = nullptr;
    auto const result = system->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &sound);
    if (result != FMOD_OK || !sound) {
        log::warn("Sfx: '{}' could not load '{}' (FMOD {})", kCues[i].name, path, static_cast<int>(result));
        return nullptr;
    }
    g_sounds[i] = sound;
    log::info("Sfx: '{}' loaded from '{}'", kCues[i].name, path);
    return sound;
}

} // namespace

void play(Cue cue) {
    if (!Mod::get()->getSettingValue<bool>("sound-effects")) return;
    auto const i = static_cast<std::size_t>(cue);
    if (i >= kCueCount) return;
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;
    auto sound = soundFor(engine->m_system, i);
    if (!sound) return;

    // Started paused so volume and pitch are in place before the first sample.
    FMOD::Channel* channel = nullptr;
    if (engine->m_system->playSound(sound, nullptr, true, &channel) != FMOD_OK || !channel) {
        log::warn("Sfx: '{}' did not start", kCues[i].name);
        return;
    }
    float const volume = kCues[i].gain * engine->m_sfxVolume;
    channel->setVolume(volume);
    float pitch = 1.f;
    if (!kCues[i].gameTime) {
        FMOD::ChannelGroup* master = nullptr;
        float masterPitch = 1.f;
        if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master
            && master->getPitch(&masterPitch) == FMOD_OK && masterPitch > 0.f) {
            pitch = 1.f / masterPitch;
            channel->setPitch(pitch);
        }
    }
    channel->setPaused(false);
    if (!g_played[i]) {
        g_played[i] = true;
        log::info("Sfx: '{}' first play, volume {:.2f}, pitch x{:.2f}", kCues[i].name, volume, pitch);
    }
}

} // namespace augment::sfx
