"""Tests for the nRF52 native west build command."""

from pathlib import Path

from esphome.components.nrf52 import _west_build_command


def test_west_build_sets_the_cmake_build_type() -> None:
    """The picolibc module used to force MinSizeRel, and with it the -DNDEBUG
    that keeps libc assert() out of the image; the build sets it itself now."""
    python = Path("/penv/python")
    cmd = _west_build_command(
        python,
        "adafruit_feather_nrf52840",
        Path("/build/pio"),
        Path("/build/zephyr"),
    )

    # str(), not a literal: the separator differs on Windows
    assert cmd[:4] == [str(python), "-m", "west", "build"]
    assert "-b" in cmd and "adafruit_feather_nrf52840" in cmd
    # CMake arguments come after west's own
    assert cmd[cmd.index("--") + 1 :] == ["-DCMAKE_BUILD_TYPE=MinSizeRel"]
