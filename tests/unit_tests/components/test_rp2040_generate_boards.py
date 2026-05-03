"""Tests for rp2040 generate_boards.py."""

from __future__ import annotations

import json
from pathlib import Path
import textwrap

import pytest

from esphome.components.rp2040.generate_boards import load_boards, parse_variant_pins

PICO_PINS_HEADER = textwrap.dedent("""\
    #pragma once
    #define PIN_LED        (25u)
    #define PIN_SERIAL1_TX (0u)
    #define PIN_SERIAL1_RX (1u)
    #define PIN_WIRE0_SDA  (4u)
    #define PIN_WIRE0_SCL  (5u)
    #define PIN_WIRE1_SDA  (26u)
    #define PIN_WIRE1_SCL  (27u)
    #define PIN_SPI0_MISO  (16u)
    #define PIN_SPI0_MOSI  (19u)
    #define PIN_SPI0_SCK   (18u)
    #define PIN_SPI0_SS    (17u)
    #include "../generic/common.h"
""")

PICOW_PINS_HEADER = textwrap.dedent("""\
    #pragma once
    #include <cyw43_wrappers.h>
    #define PIN_LED        (64u)
    #define PIN_WIRE0_SDA  (4u)
    #define PIN_WIRE0_SCL  (5u)
    #include "../generic/common.h"
""")


@pytest.fixture()
def arduino_pico(tmp_path: Path) -> Path:
    """Create a minimal arduino-pico directory structure."""
    json_dir = tmp_path / "tools" / "json"
    json_dir.mkdir(parents=True)
    variants_dir = tmp_path / "variants"
    variants_dir.mkdir()

    generic_dir = variants_dir / "generic"
    generic_dir.mkdir()
    (generic_dir / "common.h").write_text("#pragma once\n")

    return tmp_path


def _add_board(
    arduino_pico: Path,
    board_name: str,
    mcu: str = "rp2040",
    variant: str | None = None,
    vendor: str = "",
    name: str | None = None,
    pins_header: str | None = None,
    extra_flags: str = "",
) -> None:
    """Add a board JSON and variant to the fake arduino-pico tree."""
    if variant is None:
        variant = board_name
    if name is None:
        name = board_name

    json_dir = arduino_pico / "tools" / "json"
    variants_dir = arduino_pico / "variants"

    build: dict = {
        "mcu": mcu,
        "variant": variant,
    }
    if extra_flags:
        build["extra_flags"] = extra_flags

    board_json = {
        "build": build,
        "name": name,
        "vendor": vendor,
    }
    (json_dir / f"{board_name}.json").write_text(json.dumps(board_json))

    variant_dir = variants_dir / variant
    variant_dir.mkdir(exist_ok=True)
    if pins_header is not None:
        (variant_dir / "pins_arduino.h").write_text(pins_header)


def test_parse_basic_pins(tmp_path: Path) -> None:
    variant_dir = tmp_path / "rpipico"
    variant_dir.mkdir()
    (variant_dir / "pins_arduino.h").write_text(PICO_PINS_HEADER)

    pins = parse_variant_pins(variant_dir)
    assert pins["LED"] == 25
    assert pins["SDA"] == 4
    assert pins["SCL"] == 5
    assert pins["SDA1"] == 26
    assert pins["SCL1"] == 27
    assert pins["MISO"] == 16
    assert pins["MOSI"] == 19
    assert pins["SCK"] == 18
    assert pins["SS"] == 17
    assert pins["TX"] == 0
    assert pins["RX"] == 1


def test_parse_cyw43_led_pin(tmp_path: Path) -> None:
    variant_dir = tmp_path / "rpipicow"
    variant_dir.mkdir()
    (variant_dir / "pins_arduino.h").write_text(PICOW_PINS_HEADER)

    pins = parse_variant_pins(variant_dir)
    assert pins["LED"] == 64


def test_parse_missing_header(tmp_path: Path) -> None:
    variant_dir = tmp_path / "noheader"
    variant_dir.mkdir()
    assert parse_variant_pins(variant_dir) == {}


def test_parse_unmapped_defines_ignored(tmp_path: Path) -> None:
    variant_dir = tmp_path / "custom"
    variant_dir.mkdir()
    (variant_dir / "pins_arduino.h").write_text(
        "#define PIN_NEOPIXEL (16u)\n#define PIN_LED (25u)\n"
    )

    pins = parse_variant_pins(variant_dir)
    assert "NEOPIXEL" not in pins
    assert pins["LED"] == 25


def test_load_basic_board(arduino_pico: Path) -> None:
    _add_board(
        arduino_pico,
        "rpipico",
        vendor="Raspberry Pi",
        name="Pico",
        pins_header=PICO_PINS_HEADER,
    )

    board_pins, boards = load_boards(arduino_pico)

    assert "rpipico" in boards
    assert boards["rpipico"]["name"] == "Raspberry Pi Pico"
    assert boards["rpipico"]["mcu"] == "rp2040"
    assert boards["rpipico"]["max_pin"] == 29

    assert "rpipico" in board_pins
    assert board_pins["rpipico"]["LED"] == 25
    assert board_pins["rpipico"]["SDA"] == 4


def test_load_rp2350_board(arduino_pico: Path) -> None:
    _add_board(
        arduino_pico,
        "rpipico2",
        mcu="rp2350",
        vendor="Raspberry Pi",
        name="Pico 2",
        pins_header=PICO_PINS_HEADER,
    )

    _, boards = load_boards(arduino_pico)

    assert boards["rpipico2"]["mcu"] == "rp2350"
    assert boards["rpipico2"]["max_pin"] == 47


def test_cyw43_board_has_max_virtual_pin(arduino_pico: Path) -> None:
    _add_board(
        arduino_pico,
        "rpipicow",
        vendor="Raspberry Pi",
        name="Pico W",
        pins_header=PICOW_PINS_HEADER,
    )

    _, boards = load_boards(arduino_pico)

    assert boards["rpipicow"]["max_virtual_pin"] == 64


def test_non_cyw43_board_has_no_max_virtual_pin(arduino_pico: Path) -> None:
    _add_board(
        arduino_pico,
        "rpipico",
        vendor="Raspberry Pi",
        name="Pico",
        pins_header=PICO_PINS_HEADER,
    )

    _, boards = load_boards(arduino_pico)

    assert "max_virtual_pin" not in boards["rpipico"]


def test_board_without_variant_header(arduino_pico: Path) -> None:
    _add_board(arduino_pico, "novariant", name="No Variant")

    board_pins, boards = load_boards(arduino_pico)

    assert "novariant" in boards
    assert "novariant" not in board_pins


def test_shared_variant_deduplicates(arduino_pico: Path) -> None:
    """Two boards sharing the same variant should alias."""
    _add_board(arduino_pico, "base_board", pins_header=PICO_PINS_HEADER)
    _add_board(arduino_pico, "alias_board", variant="base_board")

    board_pins, _ = load_boards(arduino_pico)

    assert board_pins["base_board"] == parse_variant_pins(
        arduino_pico / "variants" / "base_board"
    )
    assert board_pins["alias_board"] == "base_board"


def test_display_name_with_vendor(arduino_pico: Path) -> None:
    _add_board(arduino_pico, "testboard", vendor="Acme", name="Widget")
    _, boards = load_boards(arduino_pico)
    assert boards["testboard"]["name"] == "Acme Widget"


def test_display_name_without_vendor(arduino_pico: Path) -> None:
    _add_board(arduino_pico, "testboard", vendor="", name="Widget")
    _, boards = load_boards(arduino_pico)
    assert boards["testboard"]["name"] == "Widget"


def test_unknown_mcu_gets_default_max_pin(arduino_pico: Path) -> None:
    _add_board(arduino_pico, "future", mcu="rp2450", pins_header=PICO_PINS_HEADER)
    _, boards = load_boards(arduino_pico)
    assert boards["future"]["max_pin"] == 29


def test_placeholder_pins_filtered_out(arduino_pico: Path) -> None:
    """Pins with placeholder values like 99 should be filtered out."""
    header = textwrap.dedent("""\
        #pragma once
        #define PIN_LED        (25u)
        #define PIN_WIRE0_SDA  (4u)
        #define PIN_WIRE0_SCL  (5u)
        #define PIN_WIRE1_SDA  (99u)
        #define PIN_WIRE1_SCL  (99u)
    """)
    _add_board(arduino_pico, "placeholder", pins_header=header)

    board_pins, boards = load_boards(arduino_pico)

    assert "SDA1" not in board_pins["placeholder"]
    assert "SCL1" not in board_pins["placeholder"]
    assert board_pins["placeholder"]["LED"] == 25
    assert "max_virtual_pin" not in boards["placeholder"]


def test_placeholder_pins_not_treated_as_virtual(arduino_pico: Path) -> None:
    """Pin 99 should not cause max_virtual_pin to be set."""
    header = textwrap.dedent("""\
        #pragma once
        #define PIN_LED        (64u)
        #define PIN_WIRE0_SDA  (4u)
        #define PIN_WIRE0_SCL  (5u)
        #define PIN_SPI0_MISO  (99u)
    """)
    _add_board(arduino_pico, "badpin", pins_header=header)

    board_pins, boards = load_boards(arduino_pico)

    assert "MISO" not in board_pins["badpin"]
    assert boards["badpin"]["max_virtual_pin"] == 64


def test_cyw43_supported_flag_sets_wifi(arduino_pico: Path) -> None:
    """Boards with PICO_CYW43_SUPPORTED=1 in extra_flags should have wifi=True."""
    _add_board(
        arduino_pico,
        "rpipicow",
        vendor="Raspberry Pi",
        name="Pico W",
        pins_header=PICOW_PINS_HEADER,
        extra_flags="-DARDUINO_RASPBERRY_PI_PICO_W -DPICO_CYW43_SUPPORTED=1 -DCYW43_PIN_WL_DYNAMIC=1",
    )

    _, boards = load_boards(arduino_pico)

    assert boards["rpipicow"]["wifi"] is True


def test_board_without_cyw43_has_no_wifi(arduino_pico: Path) -> None:
    """Boards without PICO_CYW43_SUPPORTED should not have wifi field."""
    _add_board(
        arduino_pico,
        "rpipico",
        vendor="Raspberry Pi",
        name="Pico",
        pins_header=PICO_PINS_HEADER,
        extra_flags="-DARDUINO_RASPBERRY_PI_PICO",
    )

    _, boards = load_boards(arduino_pico)

    assert "wifi" not in boards["rpipico"]
