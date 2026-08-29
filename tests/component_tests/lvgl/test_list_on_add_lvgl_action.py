"""Regression test: on_add:/on_remove: containing an lvgl action must not deadlock.

ListType.to_code() used to build the on_add/on_remove automations directly, during
widget creation. Every lvgl action's to_code awaits wait_for_widgets(), which only
resolves once *all* widgets - including the list itself - have finished being
created. Building an automation containing an lvgl action from inside that same
widget-creation walk therefore could never complete: codegen deadlocked with
"Circular dependency detected!". Fixed by deferring the actual build_automation()
call to finish_list_triggers(), run after set_widgets_completed(True) - and,
critically, before generate_triggers(), which is what processes other widgets'
on_click etc. automations that might reference this list (e.g. via lvgl.list.add),
and which therefore need the list's own on_add/on_remove triggers to already exist.
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
