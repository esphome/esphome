"""End-to-end tests for the GC9503V driver chip and the boards built on it.

The GC9503V does not use the standard MADCTL (0x36) register, so these tests go
through real codegen and check the bytes that will actually be sent to the panel.
"""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

_YAML = """
esphome:
  name: gc9503v-test
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
psram:
  mode: octal
spi:
  clk_pin:
    number: 45
    allow_other_uses: true
    ignore_strapping_warning: true
  mosi_pin:
    number: 48
    allow_other_uses: true
display:
  - platform: mipi_rgb
    id: panel
    model: ZX3D95CE01S-TR-4848
{extra}
"""


def _commands(
    main_cpp: str, display_id: str
) -> list[tuple[int | str, tuple[int, ...]]]:
    """Decode the flattened init sequence of a display into (command, data) pairs.

    A delay is returned as ("delay", (milliseconds,)).
    """
    match = re.search(rf"{display_id}->set_init_sequence\((\w+), \d+\);", main_cpp)
    assert match is not None
    table = re.search(
        rf"static constexpr uint8_t {match.group(1)}\[\] PROGMEM = \{{([^;]*)\}};",
        main_cpp,
    )
    assert table is not None
    values = [
        int(v, 0) for v in table.group(1).replace("\n", " ").split(",") if v.strip()
    ]
    commands: list[tuple[int | str, tuple[int, ...]]] = []
    index = 0
    while index < len(values):
        command, length = values[index], values[index + 1]
        if length == 0xFF:  # delay flag
            commands.append(("delay", (command,)))
            index += 2
        else:
            count = length & 0x7F
            commands.append((command, tuple(values[index + 2 : index + 2 + count])))
            index += 2 + count
    return commands


def _generate(
    generate_main: Callable[[str | Path], str], tmp_path: Path, extra: str = ""
) -> list[tuple[int | str, tuple[int, ...]]]:
    yaml_file = tmp_path / "gc9503v.yaml"
    yaml_file.write_text(_YAML.format(extra=extra))
    return _commands(generate_main(yaml_file), "panel")


def test_reset_and_init_sequence_framing(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    """No software reset is sent, as in the vendor init; only a settling delay."""
    commands = _generate(generate_main, tmp_path)
    ids = [c for c, _ in commands]

    assert 0x01 not in ids
    assert commands[0] == ("delay", (120,))
    assert commands[1] == (0xF0, (0x55, 0xAA, 0x52, 0x08, 0x00))
    # The driver supplies COLMOD, SLPOUT and DISPON itself; the board's own sequence
    # must not contain them, or they would be sent twice.
    assert ids.count(0x11) == 1
    assert ids.count(0x29) == 1
    assert ids.count(0x3A) == 1
    assert (0x3A, (0x66,)) in commands  # board uses the 18bit pixel mode


def test_madctl_uses_vendor_register_not_0x36(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    """Colour order and mirroring go to register 0xB1; 0x36 is never written."""
    commands = _generate(generate_main, tmp_path)
    ids = [c for c, _ in commands]

    assert 0x36 not in ids
    assert 0xC7 not in ids  # the ST7701S scan direction register
    # Default: RGB order, no mirroring leaves the power-on value of 0x10 untouched,
    # and the write comes straight after COLMOD.
    assert commands[ids.index(0x3A) + 1] == (0xB1, (0x10,))


@pytest.mark.parametrize(
    ("extra", "expected"),
    [
        ("    color_order: BGR", 0x30),
        ("    transform:\n      mirror_x: true\n      mirror_y: false", 0x12),
        ("    transform:\n      mirror_x: false\n      mirror_y: true", 0x11),
        (
            "    color_order: BGR\n    transform:\n      mirror_x: true\n      mirror_y: true",
            0x33,
        ),
    ],
)
def test_madctl_bits(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    extra: str,
    expected: int,
) -> None:
    """BGR is bit 5, mirror_x is GS (bit 1), mirror_y is SS (bit 0)."""
    commands = _generate(generate_main, tmp_path, extra)
    ids = [c for c, _ in commands]
    assert commands[ids.index(0x3A) + 1] == (0xB1, (expected,))


def test_board_init_sequence_is_complete(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    """Guard against a truncated or mangled panel sequence."""
    commands = _generate(generate_main, tmp_path)
    ids = [c for c, _ in commands]

    # Settling delay, 42 panel commands, then COLMOD, MADCTL, INVOFF,
    # delay, SLPOUT, delay, DISPON, delay.
    assert len(commands) == 1 + 41 + 8
    # Gamma tables D1..D6 are 52 bytes each.
    for gamma in range(0xD1, 0xD7):
        assert commands[ids.index(gamma)][1].__len__() == 52
    # GIP timing registers.
    assert commands[ids.index(0x6D)][1].__len__() == 32


def test_spi_runs_at_10mhz(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    """The vendor init clocks the 3-wire SPI at 10MHz."""
    yaml_file = tmp_path / "gc9503v.yaml"
    yaml_file.write_text(_YAML.format(extra=""))

    assert "panel->set_data_rate(10000000.0f);" in generate_main(yaml_file)
