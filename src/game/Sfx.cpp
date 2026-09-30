#include "Sfx.hpp"

#include <Geode/Geode.hpp>

#include <iterator>

using namespace geode::prelude;

namespace augment::sfx {

namespace {

struct CueDef {
    char const* name;      // for warnings
    char const* file;      // ours or a GD resource
    float gain;            // times GD's SFX volume
    // follows the master pitch (slow-mo, brake); false = menu sound, keeps its pitch
    bool gameTime;
};

// indexed by Cue. files are peak-normalized, the gains set the balance
constexpr CueDef kCues[] = {
    { "card-hover", "hover.wav"_spr, 0.55f, false },
    { "card-pick", "select.wav"_spr, 0.7f, false },
    { "missile-impact", "missile.wav"_spr, 1.f, true },
};
constexpr std::size_t kCueCount = std::size(kCues);

FMOD::Sound* g_sounds[kCueCount] = {};
bool g_tried[kCueCount] = {};

// loaded on first use and kept
FMOD::Sound* soundFor(FMOD::System* system, std::size_t i) {
    if (g_tried[i]) return g_sounds[i];
    g_tried[i] = true;
    // search paths cover GD's Resources, texture packs and our own folder
    std::string const path = CCFileUtils::get()->fullPathForFilename(kCues[i].file, true);
    FMOD::Sound* sound = nullptr;
    auto const result = system->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &sound);
    if (result != FMOD_OK || !sound) {
        log::warn("Sfx: '{}' could not load '{}' (FMOD {})", kCues[i].name, path, static_cast<int>(result));
        return nullptr;
    }
    g_sounds[i] = sound;
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

    // start paused so volume and pitch are set before the first sample
    FMOD::Channel* channel = nullptr;
    if (engine->m_system->playSound(sound, nullptr, true, &channel) != FMOD_OK || !channel) {
        log::warn("Sfx: '{}' did not start", kCues[i].name);
        return;
    }
    channel->setVolume(kCues[i].gain * engine->m_sfxVolume);
    if (!kCues[i].gameTime) {
        FMOD::ChannelGroup* master = nullptr;
        float masterPitch = 1.f;
        if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master
            && master->getPitch(&masterPitch) == FMOD_OK && masterPitch > 0.f) {
            channel->setPitch(1.f / masterPitch);
        }
    }
    channel->setPaused(false);
}

} // namespace augment::sfx
