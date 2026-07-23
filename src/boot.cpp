#include "boot.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    return static_cast<bool>(
        input.read(reinterpret_cast<char*>(&result), static_cast<std::streamsize>(1)));
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::array<unsigned char, 2> bytes{};
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    result = static_cast<std::uint16_t>(bytes[0]) |
             static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& result) {
    std::array<unsigned char, 4> bytes{};
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    result = static_cast<std::uint32_t>(bytes[0]) |
             static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_string(std::istream& input, std::string& result) {
    std::uint16_t size = 0;
    if (!read_u16(input, size) || size == 0U || size > 8192U) return false;
    result.resize(size);
    return static_cast<bool>(
        input.read(result.data(), static_cast<std::streamsize>(result.size())));
}

bool read_image(std::istream& input, BootImage& image) {
    std::uint8_t transparent = 0;
    std::uint32_t pixel_count = 0;
    if (!read_string(input, image.key) || !read_u16(input, image.width) ||
        !read_u16(input, image.height) || !read_u8(input, transparent) ||
        transparent > 1U || !read_u32(input, pixel_count))
        return false;
    const std::size_t expected =
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    if (image.width == 0U || image.height == 0U || expected != pixel_count ||
        expected > 1024U * 1024U)
        return false;
    image.transparent = transparent != 0U;
    image.pixels.resize(expected);
    return static_cast<bool>(input.read(reinterpret_cast<char*>(image.pixels.data()),
                                        static_cast<std::streamsize>(image.pixels.size())));
}

bool read_text(std::istream& input, BootTextProgram& text) {
    std::uint16_t page_count = 0;
    if (!read_string(input, text.key) || !read_u16(input, page_count) ||
        page_count == 0U || page_count > 128U)
        return false;
    text.pages.resize(page_count);
    return std::all_of(text.pages.begin(), text.pages.end(),
                       [&](std::string& page) { return read_string(input, page); });
}

bool read_byte_vector(std::istream& input, std::vector<std::uint8_t>& values,
                      std::size_t maximum) {
    std::uint8_t size = 0;
    if (!read_u8(input, size) || size == 0U || size > maximum) return false;
    values.resize(size);
    return static_cast<bool>(input.read(reinterpret_cast<char*>(values.data()),
                                        static_cast<std::streamsize>(values.size())));
}

bool valid_image(const BootContent& content, std::uint16_t image) {
    return image < content.images.size();
}

std::uint16_t title_startup_frames(const BootTitleDefinition& title) {
    std::uint32_t frames =
        static_cast<std::uint32_t>(title.setup_frames) + title.after_logo_delay_frames +
        title.version_scroll_frames + 6U;
    for (const BootTitleBounce& bounce : title.logo_bounce)
        frames += bounce.frame_count;
    return static_cast<std::uint16_t>(
        std::min<std::uint32_t>(frames, std::numeric_limits<std::uint16_t>::max()));
}

void reset_title(const BootContent& content, BootState& state) {
    state.screen = BootScreen::title;
    state.title_phase = BootTitlePhase::wait;
    state.title_elapsed = 0U;
    state.title_startup_frames = title_startup_frames(content.title);
    state.title_wait_frames = content.title.interruption_wait_frames;
    state.title_table_index = 0U;
    state.title_repeat = 0U;
    state.title_species = 0U;
    state.title_pokemon_offset = 0;
    state.title_ball_y = 0x74U;
}

void begin_oak_text(BootState& state, BootOakStage stage, std::int8_t left,
                    bool slide) {
    state.screen = BootScreen::oak_text;
    state.oak_stage = stage;
    state.text_page = 0U;
    state.picture_left_tiles = left;
    state.picture_sliding = slide;
    state.slide_steps_remaining = slide ? 8U : 0U;
    state.slide_delay = 0U;
    state.slide_direction = slide ? -1 : 0;
}

void begin_name_menu(BootState& state, bool player) {
    state.screen = BootScreen::name_menu;
    state.oak_stage = player ? BootOakStage::player_name : BootOakStage::rival_name;
    state.name_selection = 0U;
    state.picture_left_tiles = 6;
    state.picture_sliding = true;
    state.slide_steps_remaining = 6U;
    state.slide_delay = 0U;
    state.slide_direction = 1;
}

void begin_confirmation(BootState& state, bool player) {
    begin_oak_text(state,
                   player ? BootOakStage::player_confirmation
                          : BootOakStage::rival_confirmation,
                   6, false);
}

void begin_naming(BootState& state, bool player) {
    state.screen = BootScreen::naming;
    state.naming_player = player;
    state.naming_row = 0U;
    state.naming_column = 0U;
    state.naming_lowercase = false;
    state.naming_value.clear();
    state.picture_sliding = false;
}

void finish_naming(BootState& state) {
    const bool player = state.naming_player;
    if (player)
        state.player_name = state.naming_value;
    else
        state.rival_name = state.naming_value;
    begin_confirmation(state, player);
}

template <typename Table>
bool advance_scroll(const Table& table, BootState& state) {
    if (state.title_table_index >= table.size()) return true;
    const std::uint8_t packed = table[state.title_table_index];
    const std::uint8_t speed = static_cast<std::uint8_t>(packed >> 4U);
    if (state.title_repeat == 0U)
        state.title_repeat = static_cast<std::uint8_t>(packed & 0x0FU);
    state.title_pokemon_offset =
        static_cast<std::int16_t>(state.title_pokemon_offset - speed);
    if (state.title_repeat != 0U) --state.title_repeat;
    if (state.title_repeat == 0U) ++state.title_table_index;
    return state.title_table_index >= table.size();
}

void begin_title_scroll_in(const BootContent& content, std::uint8_t random,
                           BootState& state) {
    if (content.title.species.size() <= 1U) return;
    const std::size_t offset =
        1U + static_cast<std::size_t>(random) % (content.title.species.size() - 1U);
    state.title_species = (state.title_species + offset) % content.title.species.size();
    state.title_phase = BootTitlePhase::scroll_in;
    state.title_table_index = 0U;
    state.title_repeat = 0U;
    state.title_pokemon_offset = 120;
    state.title_ball_y = 0x74U;
}

void advance_title_animation(const BootContent& content, std::uint8_t random,
                             BootState& state) {
    switch (state.title_phase) {
    case BootTitlePhase::wait:
        if (state.title_wait_frames != 0U) {
            --state.title_wait_frames;
            return;
        }
        state.title_phase = BootTitlePhase::scroll_out;
        state.title_table_index = 0U;
        state.title_repeat = 0U;
        state.title_pokemon_offset = 0;
        return;
    case BootTitlePhase::scroll_out:
        if (!advance_scroll(content.title.pokemon_scroll_out, state)) return;
        if (state.title_species < 3U) {
            state.title_phase = BootTitlePhase::ball_hop;
            state.title_table_index = 1U;
            return;
        }
        begin_title_scroll_in(content, random, state);
        return;
    case BootTitlePhase::ball_hop:
        if (state.title_table_index < content.title.ball_y_positions.size()) {
            state.title_ball_y =
                content.title.ball_y_positions[state.title_table_index++];
            return;
        }
        begin_title_scroll_in(content, random, state);
        return;
    case BootTitlePhase::scroll_in:
        if (!advance_scroll(content.title.pokemon_scroll_in, state)) return;
        state.title_phase = BootTitlePhase::wait;
        state.title_wait_frames = content.title.interruption_wait_frames;
        state.title_pokemon_offset = 0;
        return;
    }
}

bool title_input(const BootInput& input) {
    return input.up_pressed || input.down_pressed || input.left_pressed ||
           input.right_pressed || input.confirm_pressed || input.cancel_pressed ||
           input.start_pressed || input.select_pressed;
}

bool step_sliding_picture(const BootContent& content, BootState& state) {
    if (!state.picture_sliding) return false;
    if (state.slide_delay != 0U) {
        --state.slide_delay;
        return true;
    }
    if (state.slide_steps_remaining != 0U) {
        state.picture_left_tiles = static_cast<std::int8_t>(
            state.picture_left_tiles + state.slide_direction);
        --state.slide_steps_remaining;
        state.slide_delay = content.oak.slide_step_delay_frames;
    }
    if (state.slide_steps_remaining == 0U) {
        state.picture_sliding = false;
        state.slide_direction = 0;
        state.slide_delay = 0U;
    }
    return true;
}

bool finish_oak_text(const BootContent& content, BootState& state,
                     std::string& error) {
    switch (state.oak_stage) {
    case BootOakStage::greeting:
        begin_oak_text(state, BootOakStage::creature_introduction, 14, true);
        return true;
    case BootOakStage::creature_introduction:
        begin_oak_text(state, BootOakStage::creature_explanation, 0, false);
        return true;
    case BootOakStage::creature_explanation:
        begin_oak_text(state, BootOakStage::player_introduction, 14, true);
        return true;
    case BootOakStage::player_introduction:
        begin_name_menu(state, true);
        return true;
    case BootOakStage::player_confirmation:
        begin_oak_text(state, BootOakStage::rival_introduction, 6, false);
        return true;
    case BootOakStage::rival_introduction:
        begin_name_menu(state, false);
        return true;
    case BootOakStage::rival_confirmation:
        begin_oak_text(state, BootOakStage::final_speech, 6, false);
        return true;
    case BootOakStage::final_speech:
        state.screen = BootScreen::ending;
        state.oak_stage = BootOakStage::first_shrink_delay;
        state.delay_frames = content.oak.ending_delay_frames[0];
        state.picture_left_tiles = 6;
        return true;
    case BootOakStage::player_name:
    case BootOakStage::rival_name:
    case BootOakStage::first_shrink_delay:
    case BootOakStage::second_shrink_delay:
    case BootOakStage::music_fade_delay:
    case BootOakStage::final_delay:
        error = "boot text completed in a non-text Oak stage";
        return false;
    }
    error = "boot text completed in an unknown Oak stage";
    return false;
}

void step_ending(const BootContent& content, BootState& state,
                 BootStepResult& result) {
    if (state.delay_frames != 0U) {
        --state.delay_frames;
        return;
    }
    switch (state.oak_stage) {
    case BootOakStage::first_shrink_delay:
        state.oak_stage = BootOakStage::second_shrink_delay;
        state.delay_frames = content.oak.ending_delay_frames[1];
        break;
    case BootOakStage::second_shrink_delay:
        state.oak_stage = BootOakStage::music_fade_delay;
        state.delay_frames = content.oak.ending_delay_frames[2];
        break;
    case BootOakStage::music_fade_delay:
        state.oak_stage = BootOakStage::final_delay;
        state.delay_frames = content.oak.ending_delay_frames[3];
        break;
    case BootOakStage::final_delay:
        state.active = false;
        result.new_game_requested = true;
        break;
    default:
        break;
    }
}

} // namespace

bool load_boot_content(const std::filesystem::path& path, BootContent& result,
                       std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t image_count = 0;
    if (!input.read(magic.data(), magic.size()) ||
        magic != std::array{'P', 'B', 'T', '1'} ||
        !read_u16(input, image_count) || image_count < 10U || image_count > 128U) {
        error = "boot cache has an invalid header";
        return false;
    }

    BootContent loaded;
    loaded.source = path;
    loaded.images.resize(image_count);
    if (!std::all_of(loaded.images.begin(), loaded.images.end(),
                     [&](BootImage& image) { return read_image(input, image); })) {
        error = "boot cache has an invalid image record";
        return false;
    }
    std::uint32_t ui_pixel_count = 0;
    if (!read_u32(input, ui_pixel_count) || ui_pixel_count != 256U * 64U) {
        error = "boot cache has an invalid UI tile set";
        return false;
    }
    loaded.ui_tiles.resize(ui_pixel_count);
    if (!input.read(reinterpret_cast<char*>(loaded.ui_tiles.data()),
                    static_cast<std::streamsize>(loaded.ui_tiles.size()))) {
        error = "boot cache UI tile set is truncated";
        return false;
    }

    std::array<std::uint16_t*, 5> title_images{
        &loaded.title.logo_image, &loaded.title.copyright_image,
        &loaded.title.version_image, &loaded.title.player_image,
        &loaded.title.ball_image,
    };
    for (std::uint16_t* image : title_images)
        if (!read_u16(input, *image)) {
            error = "boot cache title image bindings are truncated";
            return false;
        }
    std::uint16_t species_count = 0;
    if (!read_u16(input, species_count) || species_count == 0U ||
        species_count > 64U) {
        error = "boot cache has an invalid title species count";
        return false;
    }
    loaded.title.species.resize(species_count);
    for (BootTitleSpecies& species : loaded.title.species)
        if (!read_u8(input, species.internal_id) || species.internal_id == 0U ||
            !read_u16(input, species.image)) {
            error = "boot cache has an invalid title species binding";
            return false;
        }
    std::uint8_t bounce_count = 0;
    if (!read_u8(input, bounce_count) || bounce_count == 0U || bounce_count > 32U) {
        error = "boot cache has an invalid title bounce count";
        return false;
    }
    loaded.title.logo_bounce.resize(bounce_count);
    for (BootTitleBounce& bounce : loaded.title.logo_bounce) {
        std::uint8_t speed = 0;
        if (!read_u8(input, speed) || !read_u8(input, bounce.frame_count) ||
            speed == 0U || bounce.frame_count == 0U) {
            error = "boot cache has an invalid title bounce segment";
            return false;
        }
        bounce.pixels_per_frame = static_cast<std::int8_t>(speed);
    }
    if (!read_byte_vector(input, loaded.title.pokemon_scroll_in, 32U) ||
        !read_byte_vector(input, loaded.title.pokemon_scroll_out, 32U) ||
        !read_byte_vector(input, loaded.title.ball_y_positions, 32U) ||
        !read_u16(input, loaded.title.setup_frames) ||
        !read_u16(input, loaded.title.after_logo_delay_frames) ||
        !read_u16(input, loaded.title.version_scroll_frames) ||
        !read_u16(input, loaded.title.interruption_wait_frames)) {
        error = "boot cache title timing is invalid or truncated";
        return false;
    }

    if (!read_string(input, loaded.menu.continue_label) ||
        !read_string(input, loaded.menu.new_game_label) ||
        !read_string(input, loaded.menu.option_label) ||
        !std::all_of(loaded.menu.option_rows.begin(), loaded.menu.option_rows.end(),
                     [&](std::string& row) { return read_string(input, row); }) ||
        !read_string(input, loaded.menu.option_cancel) ||
        !read_u8(input, loaded.menu.before_input_delay_frames)) {
        error = "boot cache menu definition is invalid or truncated";
        return false;
    }

    for (std::uint16_t& image : loaded.oak.picture_images)
        if (!read_u16(input, image)) {
            error = "boot cache Oak picture bindings are truncated";
            return false;
        }
    if (!std::all_of(loaded.oak.texts.begin(), loaded.oak.texts.end(),
                     [&](BootTextProgram& text) { return read_text(input, text); }) ||
        !std::all_of(loaded.oak.player_names.begin(), loaded.oak.player_names.end(),
                     [&](std::string& name) { return read_string(input, name); }) ||
        !std::all_of(loaded.oak.rival_names.begin(), loaded.oak.rival_names.end(),
                     [&](std::string& name) { return read_string(input, name); }) ||
        !input.read(reinterpret_cast<char*>(loaded.oak.fade_palettes.data()),
                    static_cast<std::streamsize>(loaded.oak.fade_palettes.size())) ||
        !read_u8(input, loaded.oak.fade_step_delay_frames) ||
        !read_u8(input, loaded.oak.slide_step_delay_frames) ||
        !read_u8(input, loaded.oak.slide_steps) ||
        !input.read(reinterpret_cast<char*>(loaded.oak.ending_delay_frames.data()),
                    static_cast<std::streamsize>(loaded.oak.ending_delay_frames.size())) ||
        !read_u8(input, loaded.new_game_map_id) ||
        !read_u8(input, loaded.new_game_x) ||
        !read_u8(input, loaded.new_game_y)) {
        error = "boot cache Oak or New Game definition is invalid or truncated";
        return false;
    }

    const bool title_images_valid =
        std::all_of(title_images.begin(), title_images.end(),
                    [&](const std::uint16_t* image) {
                        return valid_image(loaded, *image);
                    });
    const bool species_images_valid =
        std::all_of(loaded.title.species.begin(), loaded.title.species.end(),
                    [&](const BootTitleSpecies& species) {
                        return valid_image(loaded, species.image);
                    });
    const bool oak_images_valid =
        std::all_of(loaded.oak.picture_images.begin(), loaded.oak.picture_images.end(),
                    [&](std::uint16_t image) { return valid_image(loaded, image); });
    const bool naming_choices_valid =
        std::none_of(loaded.oak.player_names.begin(), loaded.oak.player_names.end(),
                     [](const std::string& name) { return name.empty(); }) &&
        std::none_of(loaded.oak.rival_names.begin(), loaded.oak.rival_names.end(),
                     [](const std::string& name) { return name.empty(); });
    if (!title_images_valid || !species_images_valid || !oak_images_valid ||
        !naming_choices_valid) {
        error = "boot cache has unresolved image or naming references";
        return false;
    }
    input.peek();
    if (!input.eof()) {
        error = "boot cache has trailing bytes";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

bool begin_boot(const BootContent& content, BootState& state, std::string& error) {
    if (!content.loaded || content.title.species.empty()) {
        error = "boot content is not loaded";
        return false;
    }
    BootState started;
    started.active = true;
    reset_title(content, started);
    state = std::move(started);
    error.clear();
    return true;
}

bool step_boot(const BootContent& content, const BootInput& input, BootState& state,
               BootStepResult& result, std::string& error) {
    result = {};
    if (!content.loaded || !state.active) {
        error = "boot owner is not active";
        return false;
    }
    if (step_sliding_picture(content, state)) return true;

    if (state.screen == BootScreen::title) {
        if (state.title_startup_frames != 0U) {
            --state.title_startup_frames;
            ++state.title_elapsed;
        } else if (title_input(input)) {
            state.screen = BootScreen::main_menu;
            state.menu_selection = 0U;
            state.delay_frames = content.menu.before_input_delay_frames;
        } else {
            advance_title_animation(content, input.random, state);
        }
    } else if (state.screen == BootScreen::main_menu) {
        if (state.delay_frames != 0U) {
            --state.delay_frames;
        } else if (input.up_pressed || input.down_pressed) {
            state.menu_selection = static_cast<std::uint8_t>(1U - state.menu_selection);
        } else if (input.cancel_pressed) {
            reset_title(content, state);
        } else if (input.confirm_pressed || input.start_pressed) {
            if (state.menu_selection == 0U) {
                state.player_name.clear();
                state.rival_name.clear();
                begin_oak_text(state, BootOakStage::greeting, 6, false);
            } else {
                state.screen = BootScreen::options;
                state.option_selection = 0U;
            }
        }
    } else if (state.screen == BootScreen::options) {
        if (input.up_pressed) {
            state.option_selection =
                state.option_selection == 0U
                    ? 3U
                    : static_cast<std::uint8_t>(state.option_selection - 1U);
        } else if (input.down_pressed) {
            state.option_selection =
                static_cast<std::uint8_t>((state.option_selection + 1U) % 4U);
        } else if ((input.left_pressed || input.right_pressed) &&
                   state.option_selection < state.option_values.size()) {
            std::uint8_t& value = state.option_values[state.option_selection];
            value = input.right_pressed
                        ? static_cast<std::uint8_t>((value + 1U) % 3U)
                        : static_cast<std::uint8_t>((value + 2U) % 3U);
        } else if (input.cancel_pressed ||
                   ((input.confirm_pressed || input.start_pressed) &&
                    state.option_selection == 3U)) {
            state.screen = BootScreen::main_menu;
            state.delay_frames = 0U;
        }
    } else if (state.screen == BootScreen::oak_text) {
        if (input.confirm_pressed || input.cancel_pressed) {
            const BootTextProgram* text = boot_text(content, state);
            if (text == nullptr || state.text_page >= text->pages.size()) {
                error = "boot owner cannot resolve its current Oak text";
                return false;
            }
            if (state.text_page + 1U < text->pages.size())
                ++state.text_page;
            else if (!finish_oak_text(content, state, error))
                return false;
        }
    } else if (state.screen == BootScreen::name_menu) {
        const bool player = state.oak_stage == BootOakStage::player_name;
        if (input.up_pressed) {
            state.name_selection =
                state.name_selection == 0U
                    ? 3U
                    : static_cast<std::uint8_t>(state.name_selection - 1U);
        } else if (input.down_pressed) {
            state.name_selection =
                static_cast<std::uint8_t>((state.name_selection + 1U) % 4U);
        } else if (input.confirm_pressed || input.start_pressed) {
            if (state.name_selection == 0U) {
                begin_naming(state, player);
            } else {
                const auto& choices =
                    player ? content.oak.player_names : content.oak.rival_names;
                if (player)
                    state.player_name = choices[state.name_selection];
                else
                    state.rival_name = choices[state.name_selection];
                begin_confirmation(state, player);
            }
        }
    } else if (state.screen == BootScreen::naming) {
        if (input.select_pressed) {
            state.naming_lowercase = !state.naming_lowercase;
        } else if (input.cancel_pressed) {
            if (!state.naming_value.empty()) state.naming_value.pop_back();
        } else if (input.start_pressed && !state.naming_value.empty()) {
            finish_naming(state);
        } else if (input.up_pressed) {
            state.naming_row =
                static_cast<std::uint8_t>((state.naming_row + 5U) % 6U);
        } else if (input.down_pressed) {
            state.naming_row =
                static_cast<std::uint8_t>((state.naming_row + 1U) % 6U);
        } else if (input.left_pressed && state.naming_row < 5U) {
            state.naming_column =
                static_cast<std::uint8_t>((state.naming_column + 8U) % 9U);
        } else if (input.right_pressed && state.naming_row < 5U) {
            state.naming_column =
                static_cast<std::uint8_t>((state.naming_column + 1U) % 9U);
        } else if (input.confirm_pressed) {
            if (state.naming_row == 5U) {
                state.naming_lowercase = !state.naming_lowercase;
            } else if (state.naming_row == 4U && state.naming_column == 8U) {
                if (!state.naming_value.empty()) finish_naming(state);
            } else if (state.naming_value.size() < 7U) {
                state.naming_value +=
                    boot_naming_cell(state, state.naming_row, state.naming_column);
            }
        }
    } else if (state.screen == BootScreen::ending) {
        step_ending(content, state, result);
    }
    error.clear();
    return true;
}

std::uint16_t boot_picture_image(const BootContent& content,
                                 const BootState& state) {
    if (!content.loaded) return std::numeric_limits<std::uint16_t>::max();
    if (state.screen == BootScreen::ending) {
        if (state.oak_stage == BootOakStage::first_shrink_delay)
            return content.oak.picture_images[2];
        if (state.oak_stage == BootOakStage::second_shrink_delay)
            return content.oak.picture_images[4];
        if (state.oak_stage == BootOakStage::music_fade_delay)
            return content.oak.picture_images[5];
        return std::numeric_limits<std::uint16_t>::max();
    }
    switch (state.oak_stage) {
    case BootOakStage::greeting:
        return content.oak.picture_images[0];
    case BootOakStage::creature_introduction:
    case BootOakStage::creature_explanation:
        return content.oak.picture_images[1];
    case BootOakStage::player_introduction:
    case BootOakStage::player_name:
    case BootOakStage::player_confirmation:
    case BootOakStage::final_speech:
        return content.oak.picture_images[2];
    case BootOakStage::rival_introduction:
    case BootOakStage::rival_name:
    case BootOakStage::rival_confirmation:
        return content.oak.picture_images[3];
    case BootOakStage::first_shrink_delay:
    case BootOakStage::second_shrink_delay:
    case BootOakStage::music_fade_delay:
    case BootOakStage::final_delay:
        break;
    }
    return std::numeric_limits<std::uint16_t>::max();
}

const BootTextProgram* boot_text(const BootContent& content,
                                const BootState& state) {
    std::size_t index = 0U;
    switch (state.oak_stage) {
    case BootOakStage::greeting:
        index = 0U;
        break;
    case BootOakStage::creature_introduction:
        index = 1U;
        break;
    case BootOakStage::creature_explanation:
        index = 2U;
        break;
    case BootOakStage::player_introduction:
        index = 3U;
        break;
    case BootOakStage::player_confirmation:
        index = 4U;
        break;
    case BootOakStage::rival_introduction:
        index = 5U;
        break;
    case BootOakStage::rival_confirmation:
        index = 6U;
        break;
    case BootOakStage::final_speech:
        index = 7U;
        break;
    default:
        return nullptr;
    }
    return &content.oak.texts[index];
}

std::string boot_naming_cell(const BootState& state, std::uint8_t row,
                             std::uint8_t column) {
    if (row >= 5U || column >= 9U) return {};
    if (row < 3U) {
        const std::size_t index =
            static_cast<std::size_t>(row) * 9U + column;
        if (index < 26U) {
            const char base = state.naming_lowercase ? 'a' : 'A';
            return std::string(1, static_cast<char>(base + static_cast<char>(index)));
        }
        return " ";
    }
    constexpr std::array<std::array<const char*, 9>, 2> punctuation{{
        {"x", "(", ")", ":", ";", "[", "]", "P", "M"},
        {"-", "?", "!", "M", "F", "/", ".", ",", "END"},
    }};
    return punctuation[row - 3U][column];
}

const char* label(BootScreen screen) {
    switch (screen) {
    case BootScreen::title:
        return "Title";
    case BootScreen::main_menu:
        return "Main menu";
    case BootScreen::options:
        return "Options";
    case BootScreen::oak_text:
        return "Oak introduction";
    case BootScreen::name_menu:
        return "Default names";
    case BootScreen::naming:
        return "Naming";
    case BootScreen::ending:
        return "New Game transition";
    }
    return "Unknown";
}

} // namespace pokered
