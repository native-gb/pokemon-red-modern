#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

struct BootImage {
    std::string key;
    std::uint16_t width{};
    std::uint16_t height{};
    bool transparent{};
    std::vector<std::uint8_t> pixels;
};

struct BootTextProgram {
    std::string key;
    std::vector<std::string> pages;
};

struct BootTitleBounce {
    std::int8_t pixels_per_frame{};
    std::uint8_t frame_count{};
};

struct BootTitleSpecies {
    std::uint8_t internal_id{};
    std::uint16_t image{};
};

struct BootTitleDefinition {
    std::uint16_t logo_image{};
    std::uint16_t copyright_image{};
    std::uint16_t version_image{};
    std::uint16_t player_image{};
    std::uint16_t ball_image{};
    std::vector<BootTitleSpecies> species;
    std::vector<BootTitleBounce> logo_bounce;
    std::vector<std::uint8_t> pokemon_scroll_in;
    std::vector<std::uint8_t> pokemon_scroll_out;
    std::vector<std::uint8_t> ball_y_positions;
    std::uint16_t setup_frames{};
    std::uint16_t after_logo_delay_frames{};
    std::uint16_t version_scroll_frames{};
    std::uint16_t interruption_wait_frames{};
};

struct BootMenuDefinition {
    std::string continue_label;
    std::string new_game_label;
    std::string option_label;
    std::array<std::string, 3> option_rows;
    std::string option_cancel;
    std::uint8_t before_input_delay_frames{};
};

struct BootOakDefinition {
    std::array<std::uint16_t, 6> picture_images{};
    std::array<BootTextProgram, 8> texts;
    std::array<std::string, 4> player_names;
    std::array<std::string, 4> rival_names;
    std::array<std::uint8_t, 6> fade_palettes{};
    std::uint8_t fade_step_delay_frames{};
    std::uint8_t slide_step_delay_frames{};
    std::uint8_t slide_steps{};
    std::array<std::uint8_t, 4> ending_delay_frames{};
};

struct BootContent {
    std::filesystem::path source;
    std::vector<BootImage> images;
    std::vector<std::uint8_t> ui_tiles;
    BootTitleDefinition title;
    BootMenuDefinition menu;
    BootOakDefinition oak;
    std::uint8_t new_game_map_id{};
    std::uint8_t new_game_x{};
    std::uint8_t new_game_y{};
    bool loaded{};
};

enum class BootScreen : std::uint8_t {
    title,
    main_menu,
    options,
    oak_text,
    name_menu,
    naming,
    ending,
};

enum class BootTitlePhase : std::uint8_t {
    wait,
    scroll_out,
    ball_hop,
    scroll_in,
};

enum class BootOakStage : std::uint8_t {
    greeting,
    creature_introduction,
    creature_explanation,
    player_introduction,
    player_name,
    player_confirmation,
    rival_introduction,
    rival_name,
    rival_confirmation,
    final_speech,
    first_shrink_delay,
    second_shrink_delay,
    music_fade_delay,
    final_delay,
};

struct BootInput {
    bool up_pressed{};
    bool down_pressed{};
    bool left_pressed{};
    bool right_pressed{};
    bool confirm_pressed{};
    bool cancel_pressed{};
    bool start_pressed{};
    bool select_pressed{};
    std::uint8_t random{};
};

struct BootState {
    BootScreen screen{BootScreen::title};
    BootTitlePhase title_phase{BootTitlePhase::wait};
    BootOakStage oak_stage{BootOakStage::greeting};
    std::uint32_t title_elapsed{};
    std::uint16_t title_startup_frames{};
    std::uint16_t title_wait_frames{};
    std::uint8_t title_table_index{};
    std::uint8_t title_repeat{};
    std::size_t title_species{};
    std::int16_t title_pokemon_offset{};
    std::uint8_t title_ball_y{0x74};
    std::uint8_t menu_selection{};
    std::uint8_t option_selection{};
    std::array<std::uint8_t, 3> option_values{};
    std::uint8_t name_selection{};
    std::uint8_t naming_row{};
    std::uint8_t naming_column{};
    bool naming_lowercase{};
    bool naming_player{};
    std::string naming_value;
    std::string player_name;
    std::string rival_name;
    std::size_t text_page{};
    std::int8_t picture_left_tiles{6};
    bool picture_sliding{};
    std::uint8_t slide_steps_remaining{};
    std::uint8_t slide_delay{};
    std::int8_t slide_direction{};
    std::uint8_t delay_frames{};
    bool active{};
};

struct BootStepResult {
    bool new_game_requested{};
    bool continue_requested{};
};

bool load_boot_content(const std::filesystem::path& path, BootContent& result,
                       std::string& error);
bool begin_boot(const BootContent& content, BootState& state, std::string& error);
bool step_boot(const BootContent& content, const BootInput& input, BootState& state,
               BootStepResult& result, std::string& error);
std::uint16_t boot_picture_image(const BootContent& content, const BootState& state);
const BootTextProgram* boot_text(const BootContent& content, const BootState& state);
std::string boot_naming_cell(const BootState& state, std::uint8_t row,
                             std::uint8_t column);
const char* label(BootScreen screen);

} // namespace pokered
