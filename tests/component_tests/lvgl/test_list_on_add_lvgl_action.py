"""Regression test: on_add:/on_remove: containing an lvgl action must not deadlock."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent / "config" / "list_on_add_lvgl_action_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_on_add_with_lvgl_action_does_not_deadlock(main_cpp: str) -> None:
    assert 'lv_label_set_text(later_label, "changed");' in main_cpp


def test_on_remove_with_lvgl_action_does_not_deadlock(main_cpp: str) -> None:
    assert 'lv_label_set_text(later_label, "removed");' in main_cpp
