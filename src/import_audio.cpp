#include "import_audio.hpp"

#include "catalogue_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

struct SceneMusic {
    std::uint8_t scene{};
    std::uint8_t bank{};
    std::uint8_t sound{};
    std::string_view name;
};

struct SemanticSound {
    std::uint8_t cue{};
    std::uint8_t bank{};
    std::uint8_t sound{};
    std::string_view name;
};

struct MoveSound {
    std::uint8_t sound{};
    std::uint8_t frequency_modifier{};
    std::uint8_t tempo_modifier{};
};

constexpr std::size_t kMoveSoundTableOffset = 0x798BCU;
constexpr std::size_t kMoveSoundCount = 166U;

std::vector<MoveSound> move_sounds(
    std::span<const std::uint8_t> rom) {
    std::vector<MoveSound> result;
    const std::size_t bytes = kMoveSoundCount * 3U;
    if (kMoveSoundTableOffset + bytes > rom.size())
        return result;
    result.reserve(kMoveSoundCount);
    for (std::size_t index = 0U; index < kMoveSoundCount; ++index) {
        const std::size_t offset =
            kMoveSoundTableOffset + index * 3U;
        result.push_back({
            .sound = rom[offset],
            .frequency_modifier = rom[offset + 1U],
            .tempo_modifier = rom[offset + 2U],
        });
    }
    return result;
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<std::uint8_t>(
            (value >> shift) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes,
                  const std::string& value) {
    write_u16(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void write_source(std::vector<std::uint8_t>& bytes,
                  const RomProvenance& source) {
    write_u32(bytes, source.offset);
    write_u32(bytes, source.size);
}

std::vector<SceneMusic> scene_music(
    const ContentCatalogue& catalog) {
    constexpr std::array names{
        std::string_view{"Music_TitleScreen"},
        std::string_view{"Music_MeetProfOak"},
        std::string_view{"Music_IntroBattle"},
    };
    std::vector<SceneMusic> result;
    result.reserve(names.size());
    for (std::size_t index = 0U; index < names.size(); ++index) {
        const auto header = std::ranges::find_if(
            catalog.audio_headers,
            [&](const AudioHeaderDefinition& candidate) {
                return candidate.kind == AudioHeaderKind::Music &&
                       candidate.name == names[index];
            });
        if (header == catalog.audio_headers.end()) continue;
        result.push_back({
            .scene = static_cast<std::uint8_t>(index),
            .bank = header->audio_bank,
            .sound = header->sound_id,
            .name = names[index],
        });
    }
    return result;
}

std::vector<SemanticSound> semantic_sounds(
    const ContentCatalogue& catalog) {
    constexpr std::array stems{
        std::string_view{"SFX_Start_Menu_"},
        std::string_view{"SFX_Press_AB_"},
        std::string_view{"SFX_Go_Inside_"},
        std::string_view{"SFX_Go_Outside_"},
        std::string_view{"SFX_Ledge_"},
        std::string_view{"SFX_Get_Key_Item_"},
    };
    std::vector<SemanticSound> result;
    result.reserve(stems.size() * 3U);
    for (std::size_t cue = 0U; cue < stems.size(); ++cue) {
        for (std::uint8_t bank = 1U; bank <= 3U; ++bank) {
            const std::string name =
                std::string(stems[cue]) +
                static_cast<char>('0' + bank);
            const auto header = std::ranges::find_if(
                catalog.audio_headers,
                [&](const AudioHeaderDefinition& candidate) {
                    return candidate.kind ==
                               AudioHeaderKind::SoundEffect &&
                           candidate.audio_bank == bank &&
                           candidate.name == name;
                });
            if (header == catalog.audio_headers.end()) continue;
            result.push_back({
                .cue = static_cast<std::uint8_t>(cue),
                .bank = bank,
                .sound = header->sound_id,
                .name = stems[cue],
            });
        }
    }
    constexpr std::array exact_names{
        std::string_view{"SFX_Intro_Crash"},
        std::string_view{"SFX_Intro_Whoosh"},
    };
    for (std::size_t index = 0U;
         index < exact_names.size(); ++index) {
        const auto header = std::ranges::find_if(
            catalog.audio_headers,
            [&](const AudioHeaderDefinition& candidate) {
                return candidate.kind ==
                           AudioHeaderKind::SoundEffect &&
                       candidate.name == exact_names[index];
            });
        if (header == catalog.audio_headers.end()) continue;
        result.push_back({
            .cue = static_cast<std::uint8_t>(
                stems.size() + index),
            .bank = header->audio_bank,
            .sound = header->sound_id,
            .name = exact_names[index],
        });
    }
    return result;
}

void emit_cache(const ContentCatalogue& catalog,
                const std::vector<SceneMusic>& scenes,
                const std::vector<SemanticSound>& sounds,
                const std::vector<MoveSound>& moves,
                AudioImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'R', 'A', '6'};

    write_u16(bytes, catalog.audio_banks.size());
    for (const AudioBankDefinition& bank : catalog.audio_banks) {
        bytes.push_back(bank.id);
        bytes.push_back(bank.rom_bank);
        write_string(bytes, bank.name);
    }

    write_u16(bytes, catalog.audio_headers.size());
    for (const AudioHeaderDefinition& header :
         catalog.audio_headers) {
        write_u16(bytes, header.id);
        write_string(bytes, header.name);
        bytes.push_back(header.audio_bank);
        bytes.push_back(header.sound_id);
        bytes.push_back(static_cast<std::uint8_t>(header.kind));
        bytes.push_back(header.unused ? 1U : 0U);
        write_u16(bytes, header.channels.size());
        for (const AudioHeaderChannelDefinition& channel :
             header.channels) {
            bytes.push_back(channel.channel);
            write_u32(bytes, channel.target_offset);
            write_u16(bytes, channel.program_id);
        }
    }

    write_u16(bytes, catalog.audio_programs.size());
    for (const AudioProgramDefinition& program :
         catalog.audio_programs) {
        write_u16(bytes, program.id);
        write_string(bytes, program.name);
        bytes.push_back(program.audio_bank);
        bytes.push_back(program.channel);
        bytes.push_back(program.header_referenced ? 1U : 0U);
        write_source(bytes, program.source);
        write_u32(bytes, program.commands.size());
        for (const AudioCommandDefinition& command :
             program.commands) {
            write_u16(bytes, command.id);
            bytes.push_back(
                static_cast<std::uint8_t>(command.kind));
            bytes.push_back(command.opcode);
            bytes.push_back(command.encoded_size);
            bytes.push_back(command.music_mode ? 1U : 0U);
            write_u16(bytes, command.length_parameter);
            bytes.push_back(command.pitch);
            bytes.push_back(command.octave);
            bytes.push_back(command.instrument);
            bytes.push_back(command.speed);
            bytes.push_back(command.volume);
            bytes.push_back(static_cast<std::uint8_t>(command.fade));
            write_u16(bytes, command.frequency);
            bytes.push_back(command.sweep_length);
            bytes.push_back(
                static_cast<std::uint8_t>(command.sweep_change));
            bytes.push_back(command.vibrato_delay);
            bytes.push_back(command.vibrato_depth);
            bytes.push_back(command.vibrato_rate);
            bytes.push_back(command.pitch_slide_octave);
            bytes.push_back(command.duty_cycle);
            bytes.insert(bytes.end(), command.duty_cycle_pattern.begin(),
                         command.duty_cycle_pattern.end());
            write_u16(bytes, command.tempo);
            bytes.push_back(command.left_mask);
            bytes.push_back(command.right_mask);
            bytes.push_back(command.left_master_volume);
            bytes.push_back(command.right_master_volume);
            bytes.push_back(command.unknown_value);
            bytes.push_back(command.loop_count);
            write_u16(bytes, command.target_address);
            write_u32(bytes, command.target_offset);
            write_source(bytes, command.source);
        }
    }

    write_u16(bytes, catalog.audio_waves.size());
    for (const AudioWaveDefinition& wave : catalog.audio_waves) {
        bytes.push_back(wave.audio_bank);
        bytes.push_back(wave.id);
        bytes.insert(bytes.end(), wave.packed_samples.begin(),
                     wave.packed_samples.end());
        bytes.insert(bytes.end(), wave.samples.begin(),
                     wave.samples.end());
    }

    write_u16(bytes, catalog.audio_pitches.size());
    for (const AudioPitchDefinition& pitch :
         catalog.audio_pitches) {
        bytes.push_back(pitch.audio_bank);
        bytes.push_back(pitch.pitch);
        write_u16(bytes, pitch.frequency_word);
    }

    write_u16(bytes, catalog.pokemon_cries.size());
    for (const PokemonCryDefinition& cry : catalog.pokemon_cries) {
        bytes.push_back(cry.internal_species_id);
        bytes.push_back(cry.base_cry);
        bytes.push_back(cry.pitch_modifier);
        bytes.push_back(cry.length_modifier);
    }

    write_u16(bytes, catalog.map_music.size());
    for (const MapMusicDefinition& music : catalog.map_music) {
        bytes.push_back(music.map_id);
        bytes.push_back(music.audio_bank);
        bytes.push_back(music.sound_id);
        write_u16(bytes, music.header_definition_id);
    }

    write_u16(bytes, catalog.battle_music.size());
    for (const BattleMusicDefinition& music :
         catalog.battle_music) {
        bytes.push_back(static_cast<std::uint8_t>(music.role));
        bytes.push_back(music.audio_bank);
        bytes.push_back(music.sound_id);
        write_u16(bytes, music.header_definition_id);
    }

    write_u16(bytes, scenes.size());
    for (const SceneMusic& music : scenes) {
        bytes.push_back(music.scene);
        bytes.push_back(music.bank);
        bytes.push_back(music.sound);
    }

    write_u16(bytes, sounds.size());
    for (const SemanticSound& sound : sounds) {
        bytes.push_back(sound.cue);
        bytes.push_back(sound.bank);
        bytes.push_back(sound.sound);
    }

    write_u16(bytes, moves.size());
    for (const MoveSound& move : moves) {
        bytes.push_back(move.sound);
        bytes.push_back(move.frequency_modifier);
        bytes.push_back(move.tempo_modifier);
    }

    result.files.push_back(
        {"compiled/audio_content.bin", std::move(bytes)});
}

void emit_source(const ContentCatalogue& catalog,
                 const std::vector<SceneMusic>& scenes,
                 const std::vector<SemanticSound>& sounds,
                 const std::vector<MoveSound>& moves,
                 AudioImport& result) {
    std::ostringstream index;
    index << "; ROM-decoded Gen 1 audio ownership and dispatch.\n";
    for (const AudioHeaderDefinition& header :
         catalog.audio_headers) {
        index << (header.kind == AudioHeaderKind::Music
                      ? "music "
                      : header.kind == AudioHeaderKind::Cry
                            ? "cry "
                            : "sound ")
              << header.name << '\n'
              << "    bank "
              << static_cast<unsigned>(header.audio_bank) << '\n'
              << "    sound_id "
              << static_cast<unsigned>(header.sound_id) << '\n'
              << "    channels";
        for (const AudioHeaderChannelDefinition& channel :
             header.channels)
            index << ' ' << static_cast<unsigned>(channel.channel);
        index << '\n';
    }
    const std::string index_text = index.str();
    result.files.push_back({
        "source/audio/audio_index.sexpr",
        std::vector<std::uint8_t>(index_text.begin(),
                                  index_text.end()),
    });

    std::ostringstream maps;
    maps << "; Map music changes are content dispatch, not engine switches.\n";
    for (const MapMusicDefinition& music : catalog.map_music)
        maps << "map_music " << static_cast<unsigned>(music.map_id)
             << '\n'
             << "    bank "
             << static_cast<unsigned>(music.audio_bank) << '\n'
             << "    sound_id "
             << static_cast<unsigned>(music.sound_id) << '\n';
    const std::string map_text = maps.str();
    result.files.push_back({
        "source/audio/map_music.sexpr",
        std::vector<std::uint8_t>(map_text.begin(), map_text.end()),
    });

    std::ostringstream scene_source;
    scene_source
        << "; Semantic scenes resolve through imported content dispatch.\n";
    for (const SceneMusic& music : scenes)
        scene_source << "scene_music " << music.name << '\n'
                     << "    scene "
                     << static_cast<unsigned>(music.scene) << '\n'
                     << "    bank "
                     << static_cast<unsigned>(music.bank) << '\n'
                     << "    sound_id "
                     << static_cast<unsigned>(music.sound) << '\n';
    const std::string scene_text = scene_source.str();
    result.files.push_back({
        "source/audio/scene_music.sexpr",
        std::vector<std::uint8_t>(
            scene_text.begin(), scene_text.end()),
    });

    std::ostringstream sound_source;
    sound_source
        << "; Engine events resolve through bank-aware content cues.\n";
    for (const SemanticSound& sound : sounds)
        sound_source << "semantic_sound " << sound.name << '\n'
                     << "    cue "
                     << static_cast<unsigned>(sound.cue) << '\n'
                     << "    bank "
                     << static_cast<unsigned>(sound.bank) << '\n'
                     << "    sound_id "
                     << static_cast<unsigned>(sound.sound) << '\n';
    const std::string sound_text = sound_source.str();
    result.files.push_back({
        "source/audio/semantic_sounds.sexpr",
        std::vector<std::uint8_t>(
            sound_text.begin(), sound_text.end()),
    });

    std::ostringstream move_source;
    move_source
        << "; ROM-decoded move-to-sound dispatch. IDs are one-based.\n";
    for (std::size_t move_index = 0U;
         move_index < moves.size(); ++move_index) {
        const MoveSound& move = moves[move_index];
        move_source << "move_sound " << move_index + 1U << '\n'
                    << "    sound_id "
                    << static_cast<unsigned>(move.sound) << '\n'
                    << "    frequency_modifier "
                    << static_cast<unsigned>(
                           move.frequency_modifier)
                    << '\n'
                    << "    tempo_modifier "
                    << static_cast<unsigned>(move.tempo_modifier)
                    << '\n';
    }
    const std::string move_text = move_source.str();
    result.files.push_back({
        "source/audio/move_sounds.sexpr",
        std::vector<std::uint8_t>(
            move_text.begin(), move_text.end()),
    });
}

} // namespace

bool decode_audio_import(std::span<const std::uint8_t> rom,
                         AudioImport& result, std::string& error) {
    result = {};
    RomImage image;
    image.bytes.assign(rom.begin(), rom.end());
    ContentCatalogue catalog;
    // The reference decoder verifies that every cry record can attach to one
    // internal species slot. This domain importer owns only audio, so provide
    // the already-known Gen 1 slot census without decoding unrelated names,
    // stats, or progression content.
    catalog.internal_species.resize(190U);
    if (!decode_catalogue_audio(image, catalog, error))
        return false;

    const std::vector<SceneMusic> scenes = scene_music(catalog);
    const std::vector<SemanticSound> sounds =
        semantic_sounds(catalog);
    const std::vector<MoveSound> moves = move_sounds(rom);
    if (scenes.size() != 3U) {
        error = "audio import did not resolve every semantic scene";
        return false;
    }
    if (sounds.size() != 16U) {
        error = "audio import did not resolve every semantic sound";
        return false;
    }
    if (moves.size() != kMoveSoundCount) {
        error = "audio import did not resolve every move sound";
        return false;
    }
    emit_cache(catalog, scenes, sounds, moves, result);
    emit_source(catalog, scenes, sounds, moves, result);
    result.banks = catalog.audio_banks.size();
    result.headers = catalog.audio_headers.size();
    result.programs = catalog.audio_programs.size();
    for (const AudioProgramDefinition& program :
         catalog.audio_programs)
        result.commands += program.commands.size();
    result.waves = catalog.audio_waves.size();
    result.pitches = catalog.audio_pitches.size();
    result.cries = catalog.pokemon_cries.size();
    result.map_music = catalog.map_music.size();
    result.scene_music = scenes.size();
    error.clear();
    return true;
}

} // namespace pokered::import
