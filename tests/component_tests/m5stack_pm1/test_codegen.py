"""Code generation tests for the M5Stack PM1 hub."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config_file", "constructor"),
    [
        ("fast_mode.yaml", "M5StackPM1Component(true)"),
        ("standard_mode.yaml", "M5StackPM1Component(false)"),
    ],
)
def test_fast_mode_reaches_the_constructor(
    request: pytest.FixtureRequest, config_file: str, constructor: str
) -> None:
    """fast_mode is a constructor argument, so the default must generate false."""
    original_path = CORE.config_path
    try:
        CORE.config_path = Path(request.fspath).parent / "config" / config_file
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        assert f"new(pm1) m5stack_pm1::{constructor}" in CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()
