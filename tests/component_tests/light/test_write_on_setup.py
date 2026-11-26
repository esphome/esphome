"""Tests for the setup priorities of lights with write_on_setup."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("write_on_setup.yaml"))


def test_light_takes_its_own_setup_priority(main_cpp: str) -> None:
    assert "backlight->set_setup_priority(1299.0f);" in main_cpp


def test_addressable_light_is_set_up_after_its_output(main_cpp: str) -> None:
    assert "strip->set_setup_priority(1299.0f);" in main_cpp
    assert (
        "esp32_rmt_led_strip_esp32rmtledstriplightoutput_id->set_setup_priority(1300.0f);"
        in main_cpp
    )
