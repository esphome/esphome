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
