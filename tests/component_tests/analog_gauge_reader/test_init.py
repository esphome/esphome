"""Tests for the analog_gauge_reader configuration checks."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE

DIR = Path("tests/component_tests/analog_gauge_reader")


@pytest.mark.parametrize(
    ("filename", "message"),
    [
        ("test_zero_sweep.yaml", "zero-width angle sweep"),
        ("test_stacked_scales.yaml", "consecutive powers of ten"),
        ("test_decoder_format.yaml", "RGB888 or GRAYSCALE"),
        ("test_unrotated_crop.yaml", "does not fit"),
        ("test_unknown_dial.yaml", "has no dial"),
        ("test_empty_dials.yaml", "at least 1"),
        ("test_duplicate_dial_ids.yaml", "Duplicate dial id"),
        ("test_target_color_channel.yaml", "both target_color and process_channel"),
        ("test_scan_window.yaml", "scan window is empty"),
        ("test_deadzone_window.yaml", "scan window is empty"),
        ("test_truncated_window.yaml", "scan window is empty"),
        ("test_duplicate_sensors.yaml", "only one sensor entry"),
        ("test_two_sensor_entries.yaml", "one analog_gauge_reader sensor entry"),
        ("test_empty_calibration.yaml", "at least 1"),
    ],
)
def test_invalid_config_is_rejected(
    filename: str, message: str, capsys: pytest.CaptureFixture[str]
) -> None:
    """Each rejected option reports the reason it is unusable."""
    CORE.config_path = DIR / filename
    assert read_config({}) is None
    assert message in capsys.readouterr().out


def test_rotation_swaps_the_frame(generate_main: Callable[[str | Path], str]) -> None:
    """A quarter turn makes a tall crop valid that does not fit the raw frame."""
    generate_main(DIR / "test_rotation_crop.yaml")


def test_largest_crop_and_non_finite_calibration(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The reservation is one real crop and non-finite points are C++ literals."""
    main_cpp = generate_main(DIR / "test_buffers_and_calibration.yaml")
    assert "set_max_crop(160, 32)" in main_cpp
    assert "{ NAN, INFINITY }" in main_cpp
