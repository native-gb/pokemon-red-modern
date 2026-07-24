#include "render/boot.hpp"

#include "render/frame.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace pokered::render {
namespace {

constexpr std::array<std::array<std::uint8_t, 4>, 4> kBootColors{{
    {250, 238, 246, 255},
    {229, 178, 166, 255},
    {138, 116, 154, 255},
    {39, 25, 39, 255},
}};

SDL_FRect native_rect(const ViewLayout& view, float x, float y, float width,
                      float height) {
    return {
        .x = view.x + x * view.scale,
        .y = view.y + y * view.scale,
        .w = width * view.scale,
        .h = height * view.scale,
    };
}

SDL_Texture* upload_pixels(SDL_Renderer* renderer, std::uint16_t width,
                           std::uint16_t height, bool transparent,
                           const std::vector<std::uint8_t>& shades) {
    if (width == 0U || height == 0U ||
        shades.size() !=
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        return nullptr;
    std::vector<std::uint8_t> rgba(shades.size() * 4U);
    for (std::size_t pixel = 0; pixel < shades.size(); ++pixel) {
        const auto color = kBootColors[shades[pixel] & 0x03U];
        std::copy(color.begin(), color.end(),
                  rgba.begin() + static_cast<std::ptrdiff_t>(pixel * 4U));
        if (transparent && shades[pixel] == 0U) rgba[pixel * 4U + 3U] = 0U;
    }
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height),
                              SDL_PIXELFORMAT_RGBA32, rgba.data(),
                              static_cast<int>(width) * 4);
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr) {
        (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

bool draw_image(SDL_Renderer* renderer, const ViewLayout& view,
                const BootContent& content, const BootRenderResources& resources,
                std::uint16_t image, float x, float y, bool flip = false) {
    if (image >= content.images.size() || image >= resources.images.size() ||
        resources.images[image] == nullptr)
        return false;
    const BootImage& definition = content.images[image];
    const SDL_FRect destination =
        native_rect(view, x, y, static_cast<float>(definition.width),
                    static_cast<float>(definition.height));
    if (flip)
        return SDL_RenderTextureRotated(renderer, resources.images[image], nullptr,
                                        &destination, 0.0, nullptr,
                                        SDL_FLIP_HORIZONTAL);
    return SDL_RenderTexture(renderer, resources.images[image], nullptr, &destination);
}

bool draw_tile(SDL_Renderer* renderer, const ViewLayout& view,
               const BootRenderResources& resources, std::uint8_t tile, float x,
               float y) {
    if (resources.ui_tiles == nullptr) return false;
    const SDL_FRect source{
        static_cast<float>(tile % 16U) * 8.0F,
        static_cast<float>(tile / 16U) * 8.0F,
        8.0F,
        8.0F,
    };
    const SDL_FRect destination = native_rect(view, x, y, 8.0F, 8.0F);
    return SDL_RenderTexture(renderer, resources.ui_tiles, &source, &destination);
}

std::uint8_t ascii_tile(char value) {
    if (value >= 'A' && value <= 'Z')
        return static_cast<std::uint8_t>(0x80 + value - 'A');
    if (value >= 'a' && value <= 'z')
        return static_cast<std::uint8_t>(0xA0 + value - 'a');
    if (value >= '0' && value <= '9')
        return static_cast<std::uint8_t>(0xF6 + value - '0');
    switch (value) {
    case ' ':
        return 0x7F;
    case '(':
        return 0x9A;
    case ')':
        return 0x9B;
    case ':':
        return 0x9C;
    case ';':
        return 0x9D;
    case '[':
        return 0x9E;
    case ']':
        return 0x9F;
    case '\'':
        return 0xE0;
    case '-':
        return 0xE3;
    case '?':
        return 0xE6;
    case '!':
        return 0xE7;
    case '.':
        return 0xE8;
    case '/':
        return 0xF3;
    case ',':
        return 0xF4;
    default:
        return 0xE6;
    }
}

std::uint8_t next_tile(std::string_view text, std::size_t& cursor) {
    const auto consume = [&](std::string_view token, std::uint8_t tile)
        -> std::uint8_t {
        cursor += token.size();
        return tile;
    };
    const std::string_view remaining = text.substr(cursor);
    if (remaining.starts_with("é")) return consume("é", 0xBA);
    if (remaining.starts_with("♂")) return consume("♂", 0xEF);
    if (remaining.starts_with("♀")) return consume("♀", 0xF5);
    if (remaining.starts_with("×")) return consume("×", 0xF1);
    if (remaining.starts_with("▶")) return consume("▶", 0xED);
    if (remaining.starts_with("▼")) return consume("▼", 0xEE);
    const char value = text[cursor++];
    return ascii_tile(value);
}

std::string substitute_names(std::string text, const BootContent& content,
                             const BootState& state) {
    const auto replace_all = [&](std::string_view from, std::string_view to) {
        std::size_t position = 0U;
        while ((position = text.find(from, position)) != std::string::npos) {
            text.replace(position, from.size(), to);
            position += to.size();
        }
    };
    const std::string_view player =
        state.player_name.empty() ? std::string_view(content.oak.player_names[1])
                                  : std::string_view(state.player_name);
    const std::string_view rival =
        state.rival_name.empty() ? std::string_view(content.oak.rival_names[1])
                                 : std::string_view(state.rival_name);
    replace_all("{player_name}", player);
    replace_all("{rival_name}", rival);
    return text;
}

bool draw_text(SDL_Renderer* renderer, const ViewLayout& view,
               const BootRenderResources& resources, std::string_view text,
               std::size_t left, std::size_t top, std::size_t columns,
               std::size_t rows) {
    std::size_t x = 0U;
    std::size_t y = 0U;
    for (std::size_t cursor = 0U; cursor < text.size() && y < rows;) {
        if (text[cursor] == '\n') {
            ++cursor;
            x = 0U;
            ++y;
            continue;
        }
        if (text[cursor] == '{') {
            const std::size_t end = text.find('}', cursor);
            if (end != std::string_view::npos) {
                cursor = end + 1U;
                if (!draw_tile(renderer, view, resources, 0xE6,
                               static_cast<float>((left + x) * 8U),
                               static_cast<float>((top + y) * 8U)))
                    return false;
                if (++x >= columns) {
                    x = 0U;
                    ++y;
                }
                continue;
            }
        }
        const std::uint8_t tile = next_tile(text, cursor);
        if (!draw_tile(renderer, view, resources, tile,
                       static_cast<float>((left + x) * 8U),
                       static_cast<float>((top + y) * 8U)))
            return false;
        if (++x >= columns) {
            x = 0U;
            ++y;
        }
    }
    return true;
}

bool draw_box(SDL_Renderer* renderer, const ViewLayout& view,
              const BootRenderResources& resources, std::size_t left,
              std::size_t top, std::size_t width, std::size_t height) {
    constexpr std::uint8_t top_left = 0x79;
    constexpr std::uint8_t horizontal = 0x7A;
    constexpr std::uint8_t top_right = 0x7B;
    constexpr std::uint8_t vertical = 0x7C;
    constexpr std::uint8_t bottom_left = 0x7D;
    constexpr std::uint8_t bottom_right = 0x7E;
    constexpr std::uint8_t blank = 0x7F;
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            std::uint8_t tile = blank;
            if (y == 0U)
                tile = x == 0U ? top_left
                               : (x + 1U == width ? top_right : horizontal);
            else if (y + 1U == height)
                tile = x == 0U ? bottom_left
                               : (x + 1U == width ? bottom_right : horizontal);
            else if (x == 0U || x + 1U == width)
                tile = vertical;
            if (!draw_tile(renderer, view, resources, tile,
                           static_cast<float>((left + x) * 8U),
                           static_cast<float>((top + y) * 8U)))
                return false;
        }
    }
    return true;
}

int title_logo_offset(const BootTitleDefinition& title, std::uint32_t elapsed) {
    if (elapsed < title.setup_frames) return -64;
    std::uint32_t remaining = elapsed - title.setup_frames;
    int offset = -64;
    for (const BootTitleBounce& bounce : title.logo_bounce) {
        const std::uint32_t frames =
            std::min<std::uint32_t>(remaining, bounce.frame_count);
        offset -= static_cast<int>(bounce.pixels_per_frame) *
                  static_cast<int>(frames);
        remaining -= frames;
        if (frames != bounce.frame_count) break;
    }
    return offset;
}

std::uint32_t bounce_frames(const BootTitleDefinition& title) {
    std::uint32_t result = 0U;
    for (const BootTitleBounce& bounce : title.logo_bounce)
        result += bounce.frame_count;
    return result;
}

bool draw_title(SDL_Renderer* renderer, const ViewLayout& view,
                const BootContent& content, const BootState& state,
                const BootRenderResources& resources) {
    const BootTitleDefinition& title = content.title;
    if (state.title_species >= title.species.size()) return false;
    const std::uint32_t visual_elapsed =
        state.title_elapsed <= title.setup_frames
            ? 0U
            : state.title_elapsed - title.setup_frames;
    if (!draw_image(renderer, view, content, resources, title.logo_image, 16.0F,
                    8.0F + static_cast<float>(
                               title_logo_offset(title, state.title_elapsed))) ||
        !draw_image(renderer, view, content, resources, title.copyright_image,
                    16.0F, 136.0F))
        return false;

    if (visual_elapsed >=
        bounce_frames(title) + title.after_logo_delay_frames) {
        const std::uint32_t version_frame = std::min<std::uint32_t>(
            title.version_scroll_frames,
            visual_elapsed - bounce_frames(title) -
                title.after_logo_delay_frames);
        const float x =
            56.0F + static_cast<float>(title.version_scroll_frames - version_frame) *
                        4.0F;
        if (!draw_image(renderer, view, content, resources, title.version_image, x,
                        64.0F))
            return false;
    }

    const std::uint16_t species_image =
        title.species[state.title_species].image;
    const BootImage& pokemon = content.images[species_image];
    const float pokemon_left =
        40.0F +
        static_cast<float>((64U - static_cast<std::size_t>(pokemon.width)) / 2U) +
        static_cast<float>(state.title_pokemon_offset);
    const float pokemon_top =
        80.0F + static_cast<float>(56U - pokemon.height);
    return draw_image(renderer, view, content, resources, species_image,
                      pokemon_left, pokemon_top) &&
           draw_image(renderer, view, content, resources, title.player_image,
                      82.0F, 80.0F) &&
           draw_image(renderer, view, content, resources, title.ball_image,
                      82.0F, static_cast<float>(state.title_ball_y) - 16.0F);
}

bool draw_main_menu(SDL_Renderer* renderer, const ViewLayout& view,
                    const BootContent& content, const BootState& state,
                    const BootRenderResources& resources) {
    if (!draw_box(renderer, view, resources, 0U, 0U, 12U, 7U)) return false;
    const std::array<std::string_view, 2> labels{
        content.menu.new_game_label,
        content.menu.option_label,
    };
    for (std::size_t index = 0U; index < labels.size(); ++index) {
        if (!draw_text(renderer, view, resources, labels[index], 2U,
                       2U + index * 2U, 9U, 1U))
            return false;
    }
    return draw_tile(renderer, view, resources, 0xED,
                     8.0F, static_cast<float>((2U + state.menu_selection * 2U) * 8U));
}

bool draw_options(SDL_Renderer* renderer, const ViewLayout& view,
                  const BootContent& content, const BootState& state,
                  const BootRenderResources& resources) {
    if (!draw_box(renderer, view, resources, 0U, 0U, 20U, 18U)) return false;
    for (std::size_t row = 0U; row < content.menu.option_rows.size(); ++row) {
        std::string text = content.menu.option_rows[row];
        const std::size_t choices = text.find("  ");
        if (choices != std::string::npos) {
            text.replace(choices, 2U, "\n");
            while (choices + 1U < text.size() &&
                   text[choices + 1U] == ' ')
                text.erase(choices + 1U, 1U);
        }
        if (!draw_text(renderer, view, resources, text, 2U,
                       2U + row * 4U, 17U, 2U))
            return false;
    }
    if (!draw_text(renderer, view, resources, content.menu.option_cancel, 2U, 14U,
                   16U, 1U))
        return false;
    return draw_tile(renderer, view, resources, 0xED, 8.0F,
                     static_cast<float>((2U + state.option_selection * 4U) * 8U));
}

bool draw_oak_picture(SDL_Renderer* renderer, const ViewLayout& view,
                      const BootContent& content, const BootState& state,
                      const BootRenderResources& resources) {
    const std::uint16_t image = boot_picture_image(content, state);
    if (image == std::numeric_limits<std::uint16_t>::max()) return true;
    const bool nidorino = image == content.oak.picture_images[1];
    float left = static_cast<float>(state.picture_left_pixels);
    float top = 16.0F;
    left +=
        (64.0F - static_cast<float>(content.images[image].width)) *
        0.5F;
    if (nidorino) {
        top += static_cast<float>(56U - content.images[image].height);
    }
    return draw_image(renderer, view, content, resources, image, left, top,
                      nidorino);
}

bool draw_oak_text(SDL_Renderer* renderer, const ViewLayout& view,
                   const BootContent& content, const BootState& state,
                   const BootRenderResources& resources) {
    if (!draw_oak_picture(renderer, view, content, state, resources)) return false;
    const BootTextProgram* text = boot_text(content, state);
    if (text == nullptr || state.text_page >= text->pages.size()) return false;
    if (!draw_box(renderer, view, resources, 0U, 9U, 20U, 9U)) return false;
    const std::string page =
        substitute_names(text->pages[state.text_page], content, state);
    return draw_text(renderer, view, resources, page, 1U, 10U, 18U, 7U) &&
           draw_tile(renderer, view, resources, 0xEE, 18.0F * 8.0F,
                     16.0F * 8.0F);
}

bool draw_name_menu(SDL_Renderer* renderer, const ViewLayout& view,
                    const BootContent& content, const BootState& state,
                    const BootRenderResources& resources) {
    if (!draw_oak_picture(renderer, view, content, state, resources) ||
        !draw_box(renderer, view, resources, 0U, 1U, 12U, 13U) ||
        !draw_text(renderer, view, resources, "NAME", 3U, 2U, 8U, 1U))
        return false;
    const bool player = state.oak_stage == BootOakStage::player_name;
    const auto& names = player ? content.oak.player_names : content.oak.rival_names;
    for (std::size_t index = 0U; index < names.size(); ++index) {
        if (!draw_text(renderer, view, resources, names[index], 2U,
                       4U + index * 2U, 9U, 1U))
            return false;
    }
    return draw_tile(renderer, view, resources, 0xED, 8.0F,
                     static_cast<float>((4U + state.name_selection * 2U) * 8U));
}

bool draw_naming(SDL_Renderer* renderer, const ViewLayout& view,
                 const BootState& state,
                 const BootRenderResources& resources) {
    const std::string heading =
        state.naming_player ? "YOUR NAME?" : "RIVAL's NAME?";
    if (!draw_text(renderer, view, resources, heading, 0U, 1U, 18U, 1U) ||
        !draw_text(renderer, view, resources, state.naming_value, 10U, 2U, 7U, 1U) ||
        !draw_box(renderer, view, resources, 0U, 4U, 20U, 11U))
        return false;
    for (std::uint8_t row = 0U; row < 5U; ++row) {
        const std::uint8_t columns = row == 4U ? 8U : 9U;
        for (std::uint8_t column = 0U; column < columns; ++column) {
            if (!draw_text(renderer, view, resources,
                           boot_naming_cell(state, row, column),
                           2U + static_cast<std::size_t>(column) * 2U,
                           5U + static_cast<std::size_t>(row) * 2U, 2U, 1U))
                return false;
        }
    }
    if (!draw_text(renderer, view, resources,
                   state.naming_lowercase ? "UPPER CASE" : "lower case", 2U, 16U,
                   12U, 1U) ||
        !draw_text(renderer, view, resources, "END", 16U, 16U,
                   3U, 1U))
        return false;
    const std::size_t cursor_x =
        state.naming_row == 5U
            ? (state.naming_column == 0U ? 1U : 15U)
            : 1U + state.naming_column * 2U;
    const std::size_t cursor_y =
        state.naming_row == 5U ? 16U : 5U + state.naming_row * 2U;
    return draw_tile(renderer, view, resources, 0xED,
                     static_cast<float>(cursor_x * 8U),
                     static_cast<float>(cursor_y * 8U));
}

} // namespace

bool upload_boot_textures(SDL_Renderer* renderer, const BootContent& content,
                          BootRenderResources& resources) {
    if (renderer == nullptr || !content.loaded) return false;
    BootRenderResources uploaded;
    uploaded.images.reserve(content.images.size());
    for (const BootImage& image : content.images) {
        SDL_Texture* texture =
            upload_pixels(renderer, image.width, image.height, image.transparent,
                          image.pixels);
        if (texture == nullptr) {
            destroy_boot_textures(uploaded);
            return false;
        }
        uploaded.images.push_back(texture);
    }
    if (content.ui_tiles.size() != 256U * 64U) {
        destroy_boot_textures(uploaded);
        return false;
    }
    std::vector<std::uint8_t> atlas(128U * 128U);
    for (std::size_t tile = 0U; tile < 256U; ++tile) {
        const std::size_t tile_x = tile % 16U;
        const std::size_t tile_y = tile / 16U;
        for (std::size_t y = 0U; y < 8U; ++y)
            std::copy_n(content.ui_tiles.begin() +
                            static_cast<std::ptrdiff_t>(tile * 64U + y * 8U),
                        8U,
                        atlas.begin() + static_cast<std::ptrdiff_t>(
                                            (tile_y * 8U + y) * 128U +
                                            tile_x * 8U));
    }
    uploaded.ui_tiles = upload_pixels(renderer, 128U, 128U, true, atlas);
    if (uploaded.ui_tiles == nullptr) {
        destroy_boot_textures(uploaded);
        return false;
    }
    resources = std::move(uploaded);
    return true;
}

void destroy_boot_textures(BootRenderResources& resources) {
    for (SDL_Texture* texture : resources.images)
        if (texture != nullptr) SDL_DestroyTexture(texture);
    if (resources.ui_tiles != nullptr) SDL_DestroyTexture(resources.ui_tiles);
    resources = {};
}

bool draw_boot(SDL_Renderer* renderer, const ViewLayout& view,
               const BootContent& content, const BootState& state,
               const BootRenderResources& resources) {
    if (renderer == nullptr || !content.loaded || !state.active) return false;
    const SDL_FRect viewport = native_rect(view, 0.0F, 0.0F, 160.0F, 144.0F);
    (void)SDL_SetRenderDrawColor(renderer, kBootColors[0][0], kBootColors[0][1],
                                 kBootColors[0][2], 255);
    if (!SDL_RenderFillRect(renderer, &viewport)) return false;
    switch (state.screen) {
    case BootScreen::title:
        return draw_title(renderer, view, content, state, resources);
    case BootScreen::main_menu:
        return draw_main_menu(renderer, view, content, state, resources);
    case BootScreen::options:
        return draw_options(renderer, view, content, state, resources);
    case BootScreen::oak_text:
        return draw_oak_text(renderer, view, content, state, resources);
    case BootScreen::name_menu:
        return draw_name_menu(renderer, view, content, state, resources);
    case BootScreen::naming:
        return draw_naming(renderer, view, state, resources);
    case BootScreen::ending:
        return draw_oak_picture(renderer, view, content, state, resources);
    }
    return false;
}

} // namespace pokered::render
