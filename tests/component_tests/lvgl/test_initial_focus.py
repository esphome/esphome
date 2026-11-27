"""Regression test: an encoder's or keypad's initial_focus must focus that widget at boot."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = Path(request.fspath).parent / "config" / "initial_focus_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


@pytest.mark.parametrize(
    ("device", "widget"),
    [("test_encoder", "third_button"), ("test_keypad", "second_button")],
)
def test_initial_focus_is_applied(main_cpp: str, device: str, widget: str) -> None:
    focus_index = main_cpp.find(f"lv_group_focus_obj({widget});")
    assert focus_index != -1, f"{device} initial_focus was never applied"
    assert focus_index > main_cpp.index(f"lv_indev_set_group({device}->get_drv(),")
    assert focus_index > main_cpp.index(f"{widget} = lv_button_create(")
    assert "lv_group_focus_obj(first_button);" not in main_cpp
