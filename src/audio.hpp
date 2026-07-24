#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace pokered {

enum class AudioScene : std::uint8_t {
    title,
    oak_intro,
};

class AudioSystem {
  public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool initialize(const std::filesystem::path& cache,
                    std::string& error);
    void shutdown();
    void step();

    bool request_map_music(std::uint8_t map_id,
                           std::string& error);
    bool request_battle_music(bool trainer_battle,
                              std::string& error);
    bool request_scene_music(AudioScene scene,
                             std::string& error);
    bool play_sound(std::uint8_t audio_bank,
                    std::uint8_t sound_id,
                    std::string& error);
    bool play_move_sound(std::uint8_t move_id,
                         std::string& error);
    bool play_cry(std::uint8_t internal_species_id,
                  std::string& error);
    void play_menu_open();
    void play_menu_press();
    void play_map_transition(std::uint8_t source_map_id,
                             bool going_inside);
    void play_ledge(std::uint8_t map_id);

    bool available() const;
    std::uint8_t preferred_audio_bank() const;
    std::size_t active_effects() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pokered
