"""Regression test: a keyboard: declared before its textarea: sibling must
still get attached to it, and only after both widgets exist.

attach_textareas() runs as a deferred pass, after every widget (across every
LVGL instance) is created, so it must emit the attach call after the
keyboard's own creation statement, not inline during widget creation.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent / "config" / "keyboard_before_textarea_test.yaml"
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


def test_keyboard_and_textarea_are_created(main_cpp: str) -> None:
    assert "lv_keyboard_create(" in main_cpp
    assert "lv_textarea_create(" in main_cpp


def test_attach_call_runs_after_keyboard_and_textarea_are_created(
    main_cpp: str,
) -> None:
    attach_index = main_cpp.find("lv_keyboard_set_textarea(kb->obj, ta);")
    assert attach_index != -1, "keyboard was never attached to its textarea"
    assert attach_index > main_cpp.find("lv_keyboard_create(")
    assert attach_index > main_cpp.find("lv_textarea_create(")
