#include "audio.hpp"

#include "catalogue.hpp"
#include "pokemon_red_audio_driver.hpp"
#include "pokemon_red_audio_output.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& value) {
    char byte = 0;
    if (!input.get(byte)) return false;
    value = static_cast<std::uint8_t>(
        static_cast<unsigned char>(byte));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& value) {
    std::uint8_t low = 0U;
    std::uint8_t high = 0U;
    if (!read_u8(input, low) || !read_u8(input, high))
        return false;
    value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        static_cast<std::uint16_t>(high) << 8U);
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& value) {
    value = 0U;
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        std::uint8_t byte = 0U;
        if (!read_u8(input, byte)) return false;
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return true;
}

bool read_string(std::istream& input, std::string& value) {
    std::uint16_t size = 0U;
    if (!read_u16(input, size) || size > 4096U)
        return false;
    value.resize(size);
    return input.read(value.data(),
                      static_cast<std::streamsize>(size)).good();
}

bool read_source(std::istream& input, RomProvenance& source) {
    std::uint32_t offset = 0U;
    std::uint32_t size = 0U;
    if (!read_u32(input, offset) || !read_u32(input, size))
        return false;
    source.offset = offset;
    source.size = size;
    return true;
}

struct SceneMusicDispatch {
    std::uint8_t bank{};
    std::uint8_t sound{};
    bool present{};
};

using SemanticSoundDispatch =
    std::array<std::array<SceneMusicDispatch, 4>, 8>;

struct MoveSoundDispatch {
    std::uint8_t sound{};
    std::uint8_t frequency_modifier{};
    std::uint8_t tempo_modifier{};
};

bool load_audio_catalog(const std::filesystem::path& path,
                        ContentCatalogue& catalog,
                        std::array<SceneMusicDispatch, 3>& scenes,
                        SemanticSoundDispatch& semantic_sounds,
                        std::array<MoveSoundDispatch, 166>& move_sounds,
                        std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), 4) ||
        magic != std::array{'P', 'R', 'A', '6'}) {
        error = "audio cache has an invalid header";
        return false;
    }
    ContentCatalogue loaded;
    std::uint16_t count = 0U;

    if (!read_u16(input, count) || count != 3U) {
        error = "audio cache has an invalid bank census";
        return false;
    }
    loaded.audio_banks.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        AudioBankDefinition bank;
        if (!read_u8(input, bank.id) ||
            !read_u8(input, bank.rom_bank) ||
            !read_string(input, bank.name)) {
            error = "audio cache has a truncated bank";
            return false;
        }
        loaded.audio_banks.push_back(std::move(bank));
    }

    if (!read_u16(input, count) || count != 362U) {
        error = "audio cache has an invalid header census";
        return false;
    }
    loaded.audio_headers.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        AudioHeaderDefinition header;
        std::uint8_t kind = 0U;
        std::uint8_t unused = 0U;
        std::uint16_t channels = 0U;
        if (!read_u16(input, header.id) ||
            !read_string(input, header.name) ||
            !read_u8(input, header.audio_bank) ||
            !read_u8(input, header.sound_id) ||
            !read_u8(input, kind) || kind > 3U ||
            !read_u8(input, unused) || unused > 1U ||
            !read_u16(input, channels) || channels > 8U) {
            error = "audio cache has an invalid audio header";
            return false;
        }
        header.kind = static_cast<AudioHeaderKind>(kind);
        header.unused = unused != 0U;
        header.channels.reserve(channels);
        for (std::uint16_t channel_index = 0U;
             channel_index < channels; ++channel_index) {
            AudioHeaderChannelDefinition channel;
            std::uint32_t target = 0U;
            if (!read_u8(input, channel.channel) ||
                !read_u32(input, target) ||
                !read_u16(input, channel.program_id)) {
                error = "audio cache has a truncated header channel";
                return false;
            }
            channel.target_offset = target;
            header.channels.push_back(channel);
        }
        loaded.audio_headers.push_back(std::move(header));
    }

    if (!read_u16(input, count) || count != 785U) {
        error = "audio cache has an invalid program census";
        return false;
    }
    loaded.audio_programs.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        AudioProgramDefinition program;
        std::uint8_t referenced = 0U;
        std::uint32_t commands = 0U;
        if (!read_u16(input, program.id) ||
            !read_string(input, program.name) ||
            !read_u8(input, program.audio_bank) ||
            !read_u8(input, program.channel) ||
            !read_u8(input, referenced) || referenced > 1U ||
            !read_source(input, program.source) ||
            !read_u32(input, commands) || commands > 100000U) {
            error = "audio cache has an invalid program";
            return false;
        }
        program.header_referenced = referenced != 0U;
        program.commands.reserve(commands);
        for (std::uint32_t command_index = 0U;
             command_index < commands; ++command_index) {
            AudioCommandDefinition command;
            std::uint8_t kind = 0U;
            std::uint8_t music = 0U;
            std::uint8_t signed_value = 0U;
            std::uint32_t target = 0U;
            if (!read_u16(input, command.id) ||
                !read_u8(input, kind) || kind > 20U ||
                !read_u8(input, command.opcode) ||
                !read_u8(input, command.encoded_size) ||
                !read_u8(input, music) || music > 1U ||
                !read_u16(input, command.length_parameter) ||
                !read_u8(input, command.pitch) ||
                !read_u8(input, command.octave) ||
                !read_u8(input, command.instrument) ||
                !read_u8(input, command.speed) ||
                !read_u8(input, command.volume) ||
                !read_u8(input, signed_value)) {
                error = "audio cache has a truncated command";
                return false;
            }
            command.kind = static_cast<AudioCommandKind>(kind);
            command.music_mode = music != 0U;
            command.fade = static_cast<std::int8_t>(signed_value);
            if (!read_u16(input, command.frequency) ||
                !read_u8(input, command.sweep_length) ||
                !read_u8(input, signed_value)) {
                error = "audio cache has a truncated command sweep";
                return false;
            }
            command.sweep_change =
                static_cast<std::int8_t>(signed_value);
            if (!read_u8(input, command.vibrato_delay) ||
                !read_u8(input, command.vibrato_depth) ||
                !read_u8(input, command.vibrato_rate) ||
                !read_u8(input, command.pitch_slide_octave) ||
                !read_u8(input, command.duty_cycle) ||
                !input.read(
                    reinterpret_cast<char*>(
                        command.duty_cycle_pattern.data()),
                    static_cast<std::streamsize>(
                        command.duty_cycle_pattern.size())) ||
                !read_u16(input, command.tempo) ||
                !read_u8(input, command.left_mask) ||
                !read_u8(input, command.right_mask) ||
                !read_u8(input, command.left_master_volume) ||
                !read_u8(input, command.right_master_volume) ||
                !read_u8(input, command.unknown_value) ||
                !read_u8(input, command.loop_count) ||
                !read_u16(input, command.target_address) ||
                !read_u32(input, target) ||
                !read_source(input, command.source)) {
                error = "audio cache has a truncated command tail";
                return false;
            }
            command.target_offset = target;
            program.commands.push_back(command);
        }
        loaded.audio_programs.push_back(std::move(program));
    }

    if (!read_u16(input, count) || count != 18U) {
        error = "audio cache has an invalid wave census";
        return false;
    }
    loaded.audio_waves.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        AudioWaveDefinition wave;
        if (!read_u8(input, wave.audio_bank) ||
            !read_u8(input, wave.id) ||
            !input.read(
                reinterpret_cast<char*>(wave.packed_samples.data()),
                static_cast<std::streamsize>(
                    wave.packed_samples.size())) ||
            !input.read(reinterpret_cast<char*>(wave.samples.data()),
                        static_cast<std::streamsize>(
                            wave.samples.size()))) {
            error = "audio cache has a truncated wave";
            return false;
        }
        loaded.audio_waves.push_back(wave);
    }

    if (!read_u16(input, count) || count != 36U) {
        error = "audio cache has an invalid pitch census";
        return false;
    }
    loaded.audio_pitches.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        AudioPitchDefinition pitch;
        if (!read_u8(input, pitch.audio_bank) ||
            !read_u8(input, pitch.pitch) ||
            !read_u16(input, pitch.frequency_word)) {
            error = "audio cache has a truncated pitch";
            return false;
        }
        loaded.audio_pitches.push_back(pitch);
    }

    if (!read_u16(input, count) || count != 190U) {
        error = "audio cache has an invalid cry census";
        return false;
    }
    loaded.pokemon_cries.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        PokemonCryDefinition cry;
        if (!read_u8(input, cry.internal_species_id) ||
            !read_u8(input, cry.base_cry) ||
            !read_u8(input, cry.pitch_modifier) ||
            !read_u8(input, cry.length_modifier)) {
            error = "audio cache has a truncated cry";
            return false;
        }
        loaded.pokemon_cries.push_back(cry);
    }

    if (!read_u16(input, count) || count != 248U) {
        error = "audio cache has an invalid map-music census";
        return false;
    }
    loaded.map_music.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        MapMusicDefinition music;
        if (!read_u8(input, music.map_id) ||
            !read_u8(input, music.audio_bank) ||
            !read_u8(input, music.sound_id) ||
            !read_u16(input, music.header_definition_id)) {
            error = "audio cache has truncated map music";
            return false;
        }
        loaded.map_music.push_back(music);
    }

    if (!read_u16(input, count) || count != 4U) {
        error = "audio cache has an invalid battle-music census";
        return false;
    }
    loaded.battle_music.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        BattleMusicDefinition music;
        std::uint8_t role = 0U;
        if (!read_u8(input, role) || role > 3U ||
            !read_u8(input, music.audio_bank) ||
            !read_u8(input, music.sound_id) ||
            !read_u16(input, music.header_definition_id)) {
            error = "audio cache has truncated battle music";
            return false;
        }
        music.role = static_cast<BattleMusicRole>(role);
        loaded.battle_music.push_back(music);
    }

    if (!read_u16(input, count) || count != scenes.size()) {
        error = "audio cache has an invalid scene-music census";
        return false;
    }
    for (std::uint16_t index = 0U; index < count; ++index) {
        std::uint8_t scene = 0U;
        SceneMusicDispatch dispatch;
        if (!read_u8(input, scene) || scene >= scenes.size() ||
            scenes[scene].present ||
            !read_u8(input, dispatch.bank) ||
            !read_u8(input, dispatch.sound)) {
            error = "audio cache has invalid scene music";
            return false;
        }
        dispatch.present = true;
        scenes[scene] = dispatch;
    }

    if (!read_u16(input, count) || count != 16U) {
        error = "audio cache has an invalid semantic-sound census";
        return false;
    }
    for (std::uint16_t index = 0U; index < count; ++index) {
        std::uint8_t cue = 0U;
        std::uint8_t bank = 0U;
        SceneMusicDispatch dispatch;
        if (!read_u8(input, cue) ||
            cue >= semantic_sounds.size() ||
            !read_u8(input, bank) || bank == 0U ||
            bank >= semantic_sounds[cue].size() ||
            semantic_sounds[cue][bank].present ||
            !read_u8(input, dispatch.sound)) {
            error = "audio cache has invalid semantic sound";
            return false;
        }
        dispatch.bank = bank;
        dispatch.present = true;
        semantic_sounds[cue][bank] = dispatch;
    }
    if (!read_u16(input, count) || count != move_sounds.size()) {
        error = "audio cache has an invalid move-sound census";
        return false;
    }
    for (MoveSoundDispatch& move : move_sounds) {
        if (!read_u8(input, move.sound) ||
            !read_u8(input, move.frequency_modifier) ||
            !read_u8(input, move.tempo_modifier)) {
            error = "audio cache has a truncated move sound";
            return false;
        }
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "audio cache contains trailing data";
        return false;
    }
    catalog = std::move(loaded);
    error.clear();
    return true;
}

bool any_active(const PokemonRedAudioDriver& driver) {
    return std::ranges::any_of(
        driver.channels(),
        [](const PokemonRedAudioChannelState& channel) {
            return channel.active;
        });
}

} // namespace

struct AudioSystem::Impl {
    struct MusicVoice {
        PokemonRedAudioDriver driver;
        PokemonRedAudioPcmOutput pcm;
        std::uint8_t bank{};
        std::uint8_t sound{};
        float gain{};
        float target_gain{};
        bool active{};
    };

    struct EffectVoice {
        PokemonRedAudioDriver driver;
        PokemonRedAudioPcmOutput pcm;
        bool rendered{};
    };

    ContentCatalogue catalog;
    std::array<SceneMusicDispatch, 3> scenes;
    SemanticSoundDispatch semantic_sounds;
    std::array<MoveSoundDispatch, 166> move_sounds;
    std::array<MusicVoice, 2> music;
    std::vector<std::unique_ptr<EffectVoice>> effects;
    SDL_AudioStream* stream{};
    std::vector<float> mix;
    std::uint8_t preferred_bank{1U};
    bool owns_audio{};

    bool request_music(std::uint8_t bank, std::uint8_t sound,
                       std::string& error) {
        for (MusicVoice& voice : music) {
            if (voice.active && voice.bank == bank &&
                voice.sound == sound) {
                voice.target_gain = 1.0F;
                for (MusicVoice& other : music)
                    if (&other != &voice)
                        other.target_gain = 0.0F;
                preferred_bank = bank;
                error.clear();
                return true;
            }
        }
        MusicVoice* destination = nullptr;
        for (MusicVoice& voice : music)
            if (!voice.active || voice.gain <= 0.0F)
                destination = &voice;
        if (destination == nullptr)
            destination = music[0].gain <= music[1].gain
                              ? &music[0]
                              : &music[1];
        destination->driver.stop_all();
        destination->pcm.reset();
        if (!destination->driver.attach(catalog, error) ||
            !destination->driver.play_music(bank, sound, error))
            return false;
        destination->bank = bank;
        destination->sound = sound;
        destination->gain = 0.0F;
        destination->target_gain = 1.0F;
        destination->active = true;
        for (MusicVoice& voice : music)
            if (&voice != destination)
                voice.target_gain = 0.0F;
        preferred_bank = bank;
        error.clear();
        return true;
    }

    bool spawn_effect(std::uint8_t bank, std::uint8_t sound,
                      std::string& error) {
        auto voice = std::make_unique<EffectVoice>();
        if (!voice->driver.attach(catalog, error) ||
            !voice->driver.play_sound(bank, sound, error))
            return false;
        effects.push_back(std::move(voice));
        error.clear();
        return true;
    }

    bool spawn_semantic(std::size_t cue, std::uint8_t bank) {
        if (cue >= semantic_sounds.size() ||
            bank >= semantic_sounds[cue].size())
            return false;
        const SceneMusicDispatch& dispatch =
            semantic_sounds[cue][bank];
        if (!dispatch.present) return false;
        std::string ignored;
        return spawn_effect(
            dispatch.bank, dispatch.sound, ignored);
    }

    bool spawn_map_semantic(std::size_t cue,
                            std::uint8_t map_id) {
        const auto dispatch = std::ranges::find_if(
            catalog.map_music,
            [map_id](const MapMusicDefinition& candidate) {
                return candidate.map_id == map_id;
            });
        return dispatch != catalog.map_music.end() &&
               spawn_semantic(cue, dispatch->audio_bank);
    }

    bool spawn_cry(std::uint8_t internal_species,
                   std::string& error) {
        auto voice = std::make_unique<EffectVoice>();
        if (!voice->driver.attach(catalog, error)) return false;
        if (!voice->driver.play_cry(
                preferred_bank, internal_species, error))
            return false;
        effects.push_back(std::move(voice));
        error.clear();
        return true;
    }

    void render_step() {
        constexpr float fade_step = 1.0F / 24.0F;
        std::size_t sample_count = 0U;
        std::array<std::span<const float>, 2> music_samples;
        for (std::size_t index = 0U; index < music.size(); ++index) {
            MusicVoice& voice = music[index];
            if (!voice.active) continue;
            voice.driver.tick();
            music_samples[index] =
                voice.pcm.render_frame(voice.driver.apu_state());
            sample_count =
                std::max(sample_count, music_samples[index].size());
            if (voice.gain < voice.target_gain)
                voice.gain = std::min(
                    voice.target_gain, voice.gain + fade_step);
            else if (voice.gain > voice.target_gain)
                voice.gain = std::max(
                    voice.target_gain, voice.gain - fade_step);
        }

        std::vector<std::span<const float>> effect_samples;
        effect_samples.reserve(effects.size());
        for (const std::unique_ptr<EffectVoice>& voice : effects) {
            voice->driver.tick();
            const std::span<const float> samples =
                voice->pcm.render_frame(voice->driver.apu_state());
            effect_samples.push_back(samples);
            sample_count = std::max(sample_count, samples.size());
            voice->rendered = true;
        }

        mix.assign(sample_count, 0.0F);
        for (std::size_t voice_index = 0U;
             voice_index < music.size(); ++voice_index) {
            const MusicVoice& voice = music[voice_index];
            const std::span<const float> samples =
                music_samples[voice_index];
            for (std::size_t index = 0U; index < samples.size();
                 ++index)
                mix[index] += samples[index] * voice.gain * 0.65F;
        }
        for (const std::span<const float> samples : effect_samples)
            for (std::size_t index = 0U; index < samples.size();
                 ++index)
                mix[index] += samples[index] * 0.55F;
        for (float& sample : mix)
            sample = std::clamp(sample, -1.0F, 1.0F);
        if (stream != nullptr && !mix.empty())
            (void)SDL_PutAudioStreamData(
                stream, mix.data(),
                static_cast<int>(mix.size() * sizeof(float)));

        for (MusicVoice& voice : music) {
            if (voice.active && voice.target_gain <= 0.0F &&
                voice.gain <= 0.0F) {
                voice.driver.stop_all();
                voice.active = false;
            }
        }
        std::erase_if(
            effects,
            [](const std::unique_ptr<EffectVoice>& voice) {
                return voice->rendered &&
                       !any_active(voice->driver);
            });
    }
};

AudioSystem::AudioSystem() : impl_(std::make_unique<Impl>()) {}
AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::initialize(const std::filesystem::path& cache,
                             std::string& error) {
    shutdown();
    impl_ = std::make_unique<Impl>();
    if (!load_audio_catalog(
            cache, impl_->catalog, impl_->scenes,
            impl_->semantic_sounds, impl_->move_sounds, error))
        return false;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            error = SDL_GetError();
            return false;
        }
        impl_->owns_audio = true;
    }
    const SDL_AudioSpec specification{
        SDL_AUDIO_F32, 2,
        PokemonRedAudioPcmOutput::kSampleRate};
    impl_->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification,
        nullptr, nullptr);
    if (impl_->stream == nullptr ||
        !SDL_ResumeAudioStreamDevice(impl_->stream)) {
        error = SDL_GetError();
        shutdown();
        return false;
    }
    error.clear();
    return true;
}

void AudioSystem::shutdown() {
    if (!impl_) return;
    if (impl_->stream != nullptr)
        SDL_DestroyAudioStream(impl_->stream);
    impl_->stream = nullptr;
    if (impl_->owns_audio)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    impl_->owns_audio = false;
}

void AudioSystem::step() {
    if (impl_ && impl_->stream != nullptr)
        impl_->render_step();
}

bool AudioSystem::request_map_music(std::uint8_t map_id,
                                    std::string& error) {
    if (!impl_) return false;
    const auto found = std::ranges::find_if(
        impl_->catalog.map_music,
        [map_id](const MapMusicDefinition& music) {
            return music.map_id == map_id;
        });
    if (found == impl_->catalog.map_music.end()) {
        error = "map has no imported music dispatch";
        return false;
    }
    return impl_->request_music(
        found->audio_bank, found->sound_id, error);
}

bool AudioSystem::request_battle_music(bool trainer_battle,
                                       std::string& error) {
    if (!impl_) return false;
    const BattleMusicRole role =
        trainer_battle ? BattleMusicRole::Trainer
                       : BattleMusicRole::Wild;
    const auto found = std::ranges::find_if(
        impl_->catalog.battle_music,
        [role](const BattleMusicDefinition& music) {
            return music.role == role;
        });
    if (found == impl_->catalog.battle_music.end()) {
        error = "battle has no imported music dispatch";
        return false;
    }
    return impl_->request_music(
        found->audio_bank, found->sound_id, error);
}

bool AudioSystem::request_scene_music(AudioScene scene,
                                      std::string& error) {
    if (!impl_) return false;
    const std::size_t index = static_cast<std::size_t>(scene);
    if (index >= impl_->scenes.size() ||
        !impl_->scenes[index].present) {
        error = "scene has no imported music dispatch";
        return false;
    }
    const SceneMusicDispatch& dispatch = impl_->scenes[index];
    return impl_->request_music(
        dispatch.bank, dispatch.sound, error);
}

bool AudioSystem::play_sound(std::uint8_t audio_bank,
                             std::uint8_t sound_id,
                             std::string& error) {
    return impl_ &&
           impl_->spawn_effect(audio_bank, sound_id, error);
}

bool AudioSystem::play_move_sound(std::uint8_t move_id,
                                  std::string& error) {
    if (!impl_ || move_id == 0U ||
        move_id > impl_->move_sounds.size()) {
        error = "requested move sound has an invalid move ID";
        return false;
    }
    const MoveSoundDispatch& move =
        impl_->move_sounds[move_id - 1U];
    auto voice = std::make_unique<Impl::EffectVoice>();
    if (!voice->driver.attach(impl_->catalog, error) ||
        !voice->driver.play_modified_sound(
            2U, move.sound, move.frequency_modifier,
            move.tempo_modifier, error))
        return false;
    impl_->effects.push_back(std::move(voice));
    error.clear();
    return true;
}

bool AudioSystem::play_cry(std::uint8_t internal_species_id,
                           std::string& error) {
    return impl_ &&
           impl_->spawn_cry(internal_species_id, error);
}

void AudioSystem::play_menu_open() {
    if (impl_)
        (void)impl_->spawn_semantic(
            0U, impl_->preferred_bank);
}

void AudioSystem::play_menu_press() {
    if (impl_)
        (void)impl_->spawn_semantic(
            1U, impl_->preferred_bank);
}

void AudioSystem::play_map_transition(
    std::uint8_t source_map_id, bool going_inside) {
    if (impl_)
        (void)impl_->spawn_map_semantic(
            going_inside ? 2U : 3U, source_map_id);
}

void AudioSystem::play_ledge(std::uint8_t map_id) {
    if (impl_)
        (void)impl_->spawn_map_semantic(4U, map_id);
}

void AudioSystem::play_get_key_item() {
    if (!impl_) return;
    if (!impl_->spawn_semantic(
            5U, impl_->preferred_bank))
        (void)impl_->spawn_semantic(5U, 1U);
}

void AudioSystem::play_intro_crash() {
    if (impl_)
        (void)impl_->spawn_semantic(6U, 3U);
}

void AudioSystem::play_intro_whoosh() {
    if (impl_)
        (void)impl_->spawn_semantic(7U, 3U);
}

bool AudioSystem::available() const {
    return impl_ && impl_->stream != nullptr;
}

std::uint8_t AudioSystem::preferred_audio_bank() const {
    return impl_ ? impl_->preferred_bank : 1U;
}

std::size_t AudioSystem::active_effects() const {
    return impl_ ? impl_->effects.size() : 0U;
}

} // namespace pokered
