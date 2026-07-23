#pragma once

#include "content_ids.hpp"
#include "content_world.hpp"
#include "symbols.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pokered::content {

enum class Filtering {
    nearest,
    linear,
};

enum class AudioChannelKind {
    square_1,
    square_2,
    wave,
    noise,
};

struct ImageDef {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t atlas_page{};
    std::uint32_t atlas_x{};
    std::uint32_t atlas_y{};
    Filtering filtering{Filtering::nearest};
};

struct SpriteDef {
    ImageId image;
    std::int32_t pivot_x{};
    std::int32_t pivot_y{};
    PaletteId palette;
};

struct SpriteClipFrameDef {
    SpriteId sprite;
    std::uint32_t duration{};
};

struct SpriteClipDef {
    std::vector<SpriteClipFrameDef> frames;
    bool loop{};
};

struct PaletteDef {
    std::vector<Color> colors;
};

struct InstrumentDef {
    AudioChannelKind channel{AudioChannelKind::square_1};
    std::uint8_t duty{};
    std::uint8_t envelope{};
    std::uint8_t volume{};
    std::uint8_t sweep{};
};

struct UiStyleDef {
    FontId font;
    SpriteId frame;
    PaletteId palette;
    std::int32_t padding_x{};
    std::int32_t padding_y{};
    std::int32_t line_spacing{};
};

struct UiAnchorDef {
    Symbol name;
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct UiLayoutDef {
    UiStyleId style;
    std::vector<UiAnchorDef> anchors;
};

struct MenuEntryDef {
    TextId text;
    Symbol action;
    PredicateId enabled_when;
};

struct MenuDef {
    UiLayoutId layout;
    std::vector<MenuEntryDef> entries;
    bool cancel_allowed{};
};

struct CreditsEntryDef {
    TextId text;
    std::uint32_t duration{};
    AnimationId animation;
};

struct CreditsDef {
    MusicId music;
    std::vector<CreditsEntryDef> entries;
};

} // namespace pokered::content
