#!/usr/bin/env python3

"""Import Pokemon Red Rev 0 battle-animation programs and sprite pieces."""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import struct
import tempfile
from pathlib import Path


EXPECTED_SHA1 = "ea9bcae617fdf159b045185467ae58b2e4a48b9a"
BANK_OFFSET = 0x78000
BANK_END = 0x7C000
PROGRAM_POINTERS = 0x7A07D
PROGRAM_COUNT = 203
SUBANIMATION_POINTERS = 0x7A76D
SUBANIMATION_COUNT = 86
FRAME_BLOCK_POINTERS = 0x7AF74
FRAME_BLOCK_COUNT = 122
BASE_COORDINATES = 0x7BC85
BASE_COORDINATE_COUNT = 177
MOVE_NAMES_BEGIN = 0xB0000
MOVE_NAMES_END = 0xB060F
MOVE_COUNT = 165
TILE_SET_0 = 0x781FE
TILE_SET_1 = 0x786EE
TILE_COUNT = 79
FIRST_SPECIAL_EFFECT = 0xD8
SPIRAL_COORDINATES = 0x79476

EXTRA_ANIMATION_NAMES = (
    "show_pic",
    "enemy_flash",
    "player_flash",
    "enemy_hud_shake",
    "trade_ball_drop",
    "trade_ball_appear_1",
    "trade_ball_appear_2",
    "trade_ball_poof",
    "player_x_stat_item",
    "enemy_x_stat_item",
    "player_shrink",
    "enemy_shrink",
    "player_x_stat_black",
    "enemy_x_stat_black",
    "player_shrink_black",
    "enemy_shrink_black",
    "player_unused",
    "enemy_unused",
    "player_paralyze",
    "enemy_paralyze",
    "player_poison",
    "enemy_poison",
    "player_sleep",
    "enemy_sleep",
    "player_confused",
    "enemy_confused",
    "slide_down",
    "ball_toss",
    "ball_shake",
    "ball_poof",
    "ball_block",
    "great_ball_toss",
    "ultra_ball_toss",
    "shake_screen",
    "hide_picture",
    "safari_rock",
    "safari_bait",
    "zig_zag_screen",
)

SPECIAL_EFFECT_NAMES = (
    "wavy_screen",
    "substitute_mon",
    "shake_back_and_forth",
    "slide_enemy_mon_off",
    "show_enemy_mon_pic",
    "show_mon_pic",
    "blink_enemy_mon",
    "hide_enemy_mon_pic",
    "flash_enemy_mon_pic",
    "delay_animation_10",
    "spiral_balls_inward",
    "shake_enemy_hud_2",
    "shake_enemy_hud",
    "slide_mon_half_off",
    "petals_falling",
    "leaves_falling",
    "transform_mon",
    "slide_mon_down_and_hide",
    "minimize_mon",
    "bounce_up_and_down",
    "shoot_many_balls_upward",
    "shoot_balls_upward",
    "squish_mon_pic",
    "hide_mon_pic",
    "light_screen_palette",
    "reset_mon_position",
    "move_mon_horizontally",
    "blink_mon",
    "slide_mon_off",
    "flash_mon_pic",
    "slide_mon_down",
    "slide_mon_up",
    "flash_screen_long",
    "darken_mon_palette",
    "water_droplets_everywhere",
    "shake_screen",
    "reset_screen_palette",
    "dark_screen_palette",
    "dark_screen_flash",
)

TEXT_CHARACTERS = {
    0x7F: " ",
    0x9A: "(",
    0x9B: ")",
    0x9C: ":",
    0x9D: ";",
    0x9E: "[",
    0x9F: "]",
    0xBA: "e",
    0xE0: "'",
    0xE3: "-",
    0xE6: "?",
    0xE7: "!",
    0xE8: ".",
    0xF3: "/",
    0xF4: ",",
}


def require_range(rom: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(rom):
        raise ValueError(f"{label} extends outside the verified ROM")


def read_u16(rom: bytes, offset: int) -> int:
    require_range(rom, offset, 2, "16-bit value")
    return rom[offset] | rom[offset + 1] << 8


def bank_offset(pointer: int, label: str) -> int:
    if not 0x4000 <= pointer < 0x8000:
        raise ValueError(f"{label} pointer {pointer:#06x} is outside bank 0x1e")
    return BANK_OFFSET + pointer - 0x4000


def decode_character(value: int) -> str:
    if 0x80 <= value <= 0x99:
        return chr(ord("A") + value - 0x80)
    if 0xA0 <= value <= 0xB9:
        return chr(ord("a") + value - 0xA0)
    if 0xF6 <= value <= 0xFF:
        return chr(ord("0") + value - 0xF6)
    if value in TEXT_CHARACTERS:
        return TEXT_CHARACTERS[value]
    raise ValueError(f"unsupported move-name character byte {value:#04x}")


def decode_move_names(rom: bytes) -> list[str]:
    cursor = MOVE_NAMES_BEGIN
    names = []
    while cursor < MOVE_NAMES_END and len(names) < MOVE_COUNT:
        characters = []
        while cursor < MOVE_NAMES_END and rom[cursor] != 0x50:
            characters.append(decode_character(rom[cursor]))
            cursor += 1
        if cursor >= MOVE_NAMES_END:
            raise ValueError("move-name table is missing a terminator")
        cursor += 1
        names.append("".join(characters))
    if len(names) != MOVE_COUNT:
        raise ValueError(f"expected {MOVE_COUNT} move names, decoded {len(names)}")
    return names


def snake_case(text: str) -> str:
    text = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", text)
    text = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_").lower()
    if not text:
        raise ValueError("cannot make an empty content symbol")
    if text[0].isdigit():
        text = f"animation_{text}"
    return text


def decode_programs(rom: bytes, names: list[str]) -> list[dict]:
    programs = []
    for index in range(PROGRAM_COUNT):
        pointer_source = PROGRAM_POINTERS + index * 2
        pointer = read_u16(rom, pointer_source)
        cursor = bank_offset(pointer, "animation program")
        begin = cursor
        commands = []
        while cursor < BANK_END and rom[cursor] != 0xFF:
            opcode = rom[cursor]
            if opcode >= FIRST_SPECIAL_EFFECT:
                require_range(rom, cursor, 2, "special-effect command")
                commands.append(
                    {
                        "kind": "special",
                        "effect": opcode,
                        "sound": (rom[cursor + 1] + 1) & 0xFF,
                        "offset": cursor,
                    }
                )
                cursor += 2
            else:
                require_range(rom, cursor, 3, "subanimation command")
                tile_set = opcode >> 6
                subanimation = rom[cursor + 2]
                if tile_set >= 3 or subanimation >= SUBANIMATION_COUNT:
                    raise ValueError("animation command references invalid frame data")
                commands.append(
                    {
                        "kind": "subanimation",
                        "tile_set": tile_set,
                        "delay": opcode & 0x3F,
                        "sound": (rom[cursor + 1] + 1) & 0xFF,
                        "subanimation": subanimation,
                        "offset": cursor,
                    }
                )
                cursor += 3
        if cursor >= BANK_END:
            raise ValueError("unterminated animation program")
        name = snake_case(names[index] if index < MOVE_COUNT else EXTRA_ANIMATION_NAMES[index - MOVE_COUNT])
        programs.append(
            {
                "id": index + 1,
                "name": name,
                "pointer": pointer,
                "offset": begin,
                "end": cursor + 1,
                "commands": commands,
            }
        )
    return programs


def decode_subanimations(rom: bytes) -> list[dict]:
    result = []
    for index in range(SUBANIMATION_COUNT):
        pointer = read_u16(rom, SUBANIMATION_POINTERS + index * 2)
        offset = bank_offset(pointer, "subanimation")
        header = rom[offset]
        count = header & 0x1F
        require_range(rom, offset + 1, count * 3, "subanimation frames")
        if count == 0:
            raise ValueError(f"subanimation {index} has no frames")
        frames = []
        for frame in range(count):
            source = offset + 1 + frame * 3
            block, base, mode = rom[source : source + 3]
            if block >= FRAME_BLOCK_COUNT or base >= BASE_COORDINATE_COUNT or mode >= 5:
                raise ValueError(f"subanimation {index} contains an invalid frame")
            frames.append((block, base, mode))
        result.append({"transform": header >> 5, "frames": frames})
    return result


def decode_frame_blocks(rom: bytes) -> list[list[tuple[int, int, int, int]]]:
    result = []
    for index in range(FRAME_BLOCK_COUNT):
        pointer = read_u16(rom, FRAME_BLOCK_POINTERS + index * 2)
        offset = bank_offset(pointer, "frame block")
        count = rom[offset]
        if count > 19:
            raise ValueError(f"frame block {index} has {count} sprite pieces")
        require_range(rom, offset + 1, count * 4, "frame block sprite pieces")
        pieces = []
        for piece in range(count):
            source = offset + 1 + piece * 4
            y, x, tile, attributes = rom[source : source + 4]
            pieces.append(
                (
                    y - 256 if y >= 128 else y,
                    x - 256 if x >= 128 else x,
                    tile,
                    attributes,
                )
            )
        result.append(pieces)
    return result


def decode_base_coordinates(rom: bytes) -> list[tuple[int, int]]:
    require_range(rom, BASE_COORDINATES, BASE_COORDINATE_COUNT * 2, "base coordinates")
    return [
        (rom[BASE_COORDINATES + index * 2], rom[BASE_COORDINATES + index * 2 + 1])
        for index in range(BASE_COORDINATE_COUNT)
    ]


def decode_spiral_coordinates(rom: bytes) -> list[tuple[int, int]]:
    coordinates = []
    cursor = SPIRAL_COORDINATES
    while cursor < BANK_END and rom[cursor] != 0xFF:
        require_range(rom, cursor, 2, "spiral-ball coordinate")
        coordinates.append((rom[cursor], rom[cursor + 1]))
        cursor += 2
    if cursor >= BANK_END or len(coordinates) < 3:
        raise ValueError("spiral-ball coordinate table is invalid")
    return coordinates


def transformed_piece(
    base: tuple[int, int], piece: tuple[int, int, int, int], transform: int
) -> tuple[int, int, int, int]:
    base_y, base_x = base
    y_offset, x_offset, tile, attributes = piece
    if transform == 1:
        y = 136 - (base_y + y_offset)
        x = 168 - (base_x + x_offset)
        attributes ^= 0x60
    elif transform == 2:
        y = base_y + y_offset + 40
        x = 168 - (base_x + x_offset)
        attributes ^= 0x20
    elif transform == 3:
        y = 136 - base_y + y_offset
        x = 168 - base_x + x_offset
    else:
        y = base_y + y_offset
        x = base_x + x_offset
    return x - 8, y - 16, tile, attributes


def resolved_transform(subanimation_type: int, enemy_turn: bool) -> int:
    # Red resolves authored transform types through hWhoseTurn at playback.
    if subanimation_type == 5:
        return 0 if enemy_turn else 2
    return subanimation_type if enemy_turn else 0


def append_special_effect(
    lines: list[str],
    effect: int,
    sound: int,
    enemy_turn: bool,
    screen_palette: str,
    intern_visual,
    unresolved_effects: set[str],
    spiral_coordinates: list[tuple[int, int]],
) -> str:
    name = SPECIAL_EFFECT_NAMES[effect - FIRST_SPECIAL_EFFECT]
    actor = "defender" if enemy_turn else "attacker"
    toward_opponent = -24 if enemy_turn else 24
    offscreen = 64 if enemy_turn else -64
    lines.append(f"    ; special_effect {name} id {effect:#04x} sound {sound}")
    if sound:
        lines.append(f"    signal imported_sound_{sound:03d}")
    if name == "light_screen_palette":
        lines.append("    set_palette battle_screen light")
        return "light"
    if name == "dark_screen_palette":
        lines.append("    set_palette battle_screen dark")
        return "dark"
    if name == "darken_mon_palette":
        lines.append(f"    set_palette {actor} darkened")
        return screen_palette
    if name == "reset_screen_palette":
        lines.extend(
            (
                "    set_palette battle_screen normal",
                f"    set_palette {actor} normal",
            )
        )
        return "normal"
    if name == "dark_screen_flash":
        lines.extend(
            (
                "    set_palette battle_screen inverted",
                "    wait 2",
                "    set_palette battle_screen white",
                "    wait 2",
                f"    set_palette battle_screen {screen_palette}",
            )
        )
        return screen_palette
    if name == "flash_screen_long":
        for delay in (2, 1, 1):
            lines.extend(
                (
                    "    set_palette battle_screen dark",
                    f"    wait {delay * 4}",
                    "    set_palette battle_screen white",
                    f"    wait {delay * 4}",
                    f"    set_palette battle_screen {screen_palette}",
                    f"    wait {delay * 4}",
                )
            )
        return screen_palette
    if name == "spiral_balls_inward":
        base_y = -40 if enemy_turn else 0
        base_x = 80 if enemy_turn else 0
        for frame in range(len(spiral_coordinates) - 2):
            pieces = [
                (x + base_x - 8, y + base_y - 16, 0, 73, 0)
                for y, x in spiral_coordinates[frame : frame + 3]
            ]
            visual = intern_visual(pieces)
            lines.extend(
                (
                    f"    spawn procedural_frame {visual}",
                    "    set_position procedural_frame 0 0 native_canvas",
                    "    wait 5",
                    "    destroy procedural_frame",
                )
            )
        lines.extend(
            (
                "    set_palette battle_screen inverted",
                "    wait 2",
                "    set_palette battle_screen white",
                "    wait 2",
                f"    set_palette battle_screen {screen_palette}",
            )
        )
        return screen_palette
    if name == "delay_animation_10":
        lines.append("    wait 10")
    elif name == "reset_mon_position":
        lines.append(f"    set_offset {actor} 0 0 native_canvas")
    elif name == "move_mon_horizontally":
        lines.extend(
            (
                f"    tween_offset {actor} {toward_opponent} 0 6 ease_in native_canvas",
                f"    tween_offset {actor} 0 0 6 ease_out native_canvas",
            )
        )
    elif name == "blink_mon":
        for _ in range(6):
            lines.extend((f"    hide {actor}", "    wait 5", f"    show {actor}", "    wait 5"))
    elif name == "blink_enemy_mon":
        for _ in range(6):
            lines.extend(("    hide defender", "    wait 5", "    show defender", "    wait 5"))
    elif name == "flash_mon_pic":
        # Despite its disassembly label, this reloads the same species picture.
        lines.append(f"    show {actor}")
    elif name == "flash_enemy_mon_pic":
        lines.append("    show defender")
    elif name == "bounce_up_and_down":
        lines.extend(
            (
                f"    tween_offset {actor} 0 -12 5 ease_out native_canvas",
                f"    tween_offset {actor} 0 0 5 ease_in native_canvas",
            )
        )
    elif name == "slide_enemy_mon_off":
        lines.extend(("    tween_offset defender 64 0 16 linear native_canvas", "    hide defender"))
    elif name == "show_enemy_mon_pic":
        lines.append("    show defender")
    elif name == "show_mon_pic":
        lines.append(f"    show {actor}")
    elif name == "hide_enemy_mon_pic":
        lines.append("    hide defender")
    elif name == "hide_mon_pic":
        lines.append(f"    hide {actor}")
    elif name == "slide_mon_down_and_hide":
        lines.extend(
            (f"    tween_offset {actor} 0 48 12 linear native_canvas", f"    hide {actor}")
        )
    elif name == "slide_mon_half_off":
        lines.append(
            f"    tween_offset {actor} {offscreen // 2} 0 12 linear native_canvas"
        )
    elif name == "slide_mon_off":
        lines.extend(
            (f"    tween_offset {actor} {offscreen} 0 16 linear native_canvas", f"    hide {actor}")
        )
    elif name == "slide_mon_down":
        lines.append(f"    tween_offset {actor} 0 16 8 linear native_canvas")
    elif name == "slide_mon_up":
        lines.append(f"    tween_offset {actor} 0 -16 8 linear native_canvas")
    else:
        unresolved_effects.add(name)
        lines.extend((f"    signal special_{name}", "    wait 1"))
    return screen_palette


def emit_programs(
    source_root: Path,
    programs: list[dict],
    subanimations: list[dict],
    frame_blocks: list[list[tuple[int, int, int, int]]],
    base_coordinates: list[tuple[int, int]],
    spiral_coordinates: list[tuple[int, int]],
) -> tuple[list[tuple[str, list[tuple[int, int, int, int, int]]]], set[str]]:
    visuals: list[tuple[str, list[tuple[int, int, int, int, int]]]] = []
    visual_ids: dict[tuple[tuple[int, int, int, int, int], ...], str] = {}
    unresolved_effects: set[str] = set()

    def intern_visual(pieces: list[tuple[int, int, int, int, int]]) -> str:
        key = tuple(pieces)
        if key in visual_ids:
            return visual_ids[key]
        name = f"red_frame_{len(visuals):05d}"
        visual_ids[key] = name
        visuals.append((name, pieces.copy()))
        return name

    for program in programs:
        enemy_turn = program["name"].startswith("enemy_")
        lines = [
            "; Generated locally from Pokemon Red US Rev 0. Do not distribute.",
            (
                f"; animation_id {program['id']} rom "
                f"0x{program['offset']:05x}..0x{program['end']:05x}"
            ),
            f"; playback_side {'enemy' if enemy_turn else 'player'}",
            f"animation {program['name']}",
        ]
        oam: list[tuple[int, int, int, int, int] | None] = [None] * 40
        active_visual = False
        screen_palette = "normal"
        for command_index, command in enumerate(program["commands"]):
            if command["kind"] == "special":
                if active_visual:
                    lines.append("    destroy imported_frame")
                    active_visual = False
                screen_palette = append_special_effect(
                    lines,
                    command["effect"],
                    command["sound"],
                    enemy_turn,
                    screen_palette,
                    intern_visual,
                    unresolved_effects,
                    spiral_coordinates,
                )
                continue

            tile_set = 0 if command["tile_set"] == 2 else command["tile_set"]
            subanimation = subanimations[command["subanimation"]]
            transform = resolved_transform(subanimation["transform"], enemy_turn)
            lines.append(
                (
                    f"    ; command {command_index} subanimation "
                    f"{command['subanimation']} tileset {command['tile_set']} "
                    f"delay {command['delay']} sound {command['sound']}"
                )
            )
            if command["sound"]:
                lines.append(f"    signal imported_sound_{command['sound']:03d}")
            destination = 0
            for frame_index, (block_id, base_id, mode) in enumerate(subanimation["frames"]):
                pieces = [
                    transformed_piece(base_coordinates[base_id], piece, transform)
                    for piece in frame_blocks[block_id]
                ]
                if destination + len(pieces) > len(oam):
                    raise ValueError(
                        f"{program['name']} writes beyond the 40-entry OAM buffer"
                    )
                for index, (x, y, tile, attributes) in enumerate(pieces):
                    oam[destination + index] = (x, y, tile_set, tile, attributes)

                if mode != 2:
                    visible = [piece for piece in oam if piece is not None]
                    visual = intern_visual(visible)
                    if active_visual:
                        lines.append("    destroy imported_frame")
                    lines.extend(
                        (
                            f"    ; frame {frame_index} block {block_id} base {base_id} mode {mode}",
                            f"    spawn imported_frame {visual}",
                            "    set_position imported_frame 0 0 native_canvas",
                            f"    wait {command['delay']}",
                        )
                    )
                    active_visual = True

                destination += len(pieces)
                if mode not in (2, 3, 4):
                    oam = [None] * 40
                    destination = 0
                elif mode == 4:
                    destination -= len(pieces)

        if active_visual:
            lines.append("    destroy imported_frame")
        lines.append("    signal animation_finished")
        path = source_root / f"{program['id']:03d}_{program['name']}.sexpr"
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return visuals, unresolved_effects


def write_assets(
    path: Path,
    rom: bytes,
    visuals: list[tuple[str, list[tuple[int, int, int, int, int]]]],
) -> None:
    tile_bytes = TILE_COUNT * 16
    require_range(rom, TILE_SET_0, tile_bytes, "battle animation tile set 0")
    require_range(rom, TILE_SET_1, tile_bytes, "battle animation tile set 1")
    with path.open("wb") as output:
        output.write(b"PRA1")
        output.write(struct.pack("<HHI", TILE_COUNT, TILE_COUNT, len(visuals)))
        output.write(rom[TILE_SET_0 : TILE_SET_0 + tile_bytes])
        output.write(rom[TILE_SET_1 : TILE_SET_1 + tile_bytes])
        for name, pieces in visuals:
            encoded_name = name.encode("ascii")
            output.write(struct.pack("<HH", len(encoded_name), len(pieces)))
            output.write(encoded_name)
            for x, y, tile_set, tile, attributes in pieces:
                output.write(struct.pack("<hhBBB", x, y, tile_set, tile, attributes))


def default_rom(repo_root: Path) -> Path:
    return repo_root.parent / "native-gb-pokemon-red" / "roms" / "pokemon_red.gb"


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", nargs="?", type=Path, default=default_rom(repo_root))
    parser.add_argument(
        "--output",
        type=Path,
        default=repo_root / "data" / "runtime" / "imports" / "pokemon_red_us_rev_0",
    )
    arguments = parser.parse_args()

    rom = arguments.rom.read_bytes()
    digest = hashlib.sha1(rom).hexdigest()
    if digest != EXPECTED_SHA1:
        raise SystemExit(
            f"unsupported ROM SHA1 {digest}; expected Pokemon Red US Rev 0 {EXPECTED_SHA1}"
        )

    names = decode_move_names(rom)
    programs = decode_programs(rom, names)
    subanimations = decode_subanimations(rom)
    frame_blocks = decode_frame_blocks(rom)
    base_coordinates = decode_base_coordinates(rom)
    spiral_coordinates = decode_spiral_coordinates(rom)

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(
        tempfile.mkdtemp(prefix=f".{arguments.output.name}.", dir=arguments.output.parent)
    )
    try:
        source_root = temporary / "source" / "animations" / "battle_moves"
        compiled_root = temporary / "compiled"
        reports_root = temporary / "reports"
        source_root.mkdir(parents=True)
        compiled_root.mkdir(parents=True)
        reports_root.mkdir(parents=True)
        visuals, unresolved_effects = emit_programs(
            source_root,
            programs,
            subanimations,
            frame_blocks,
            base_coordinates,
            spiral_coordinates,
        )
        write_assets(compiled_root / "battle_animation_frames.bin", rom, visuals)
        (temporary / "import_manifest").write_text(
            "\n".join(
                (
                    "profile pokemon_red_us_rev_0",
                    f"rom_sha1 {digest}",
                    "importer battle_animations_v1",
                    f"animation_programs {len(programs)}",
                    f"subanimations {len(subanimations)}",
                    f"frame_blocks {len(frame_blocks)}",
                    f"visual_frames {len(visuals)}",
                    "",
                )
            ),
            encoding="utf-8",
        )
        (reports_root / "battle_animation_summary.txt").write_text(
            (
                f"programs: {len(programs)}\n"
                f"subanimations: {len(subanimations)}\n"
                f"frame blocks: {len(frame_blocks)}\n"
                f"base coordinates: {len(base_coordinates)}\n"
                f"deduplicated visual frames: {len(visuals)}\n"
                f"unresolved procedural effect types: {len(unresolved_effects)}\n"
                + "".join(f"  {name}\n" for name in sorted(unresolved_effects))
            ),
            encoding="utf-8",
        )
        if arguments.output.exists():
            shutil.rmtree(arguments.output)
        temporary.replace(arguments.output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise

    print(f"Imported {len(programs)} battle animations from {arguments.rom}")
    print(f"Readable scripts: {arguments.output / 'source' / 'animations' / 'battle_moves'}")
    print(f"Frame assets: {arguments.output / 'compiled' / 'battle_animation_frames.bin'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
