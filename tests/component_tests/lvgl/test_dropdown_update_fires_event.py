"""Regression test: lvgl.dropdown.update with selected_index must fire on_value/on_update.

LvSelect (backing both dropdown and roller) did not set `value_property`, so the generic
update-action machinery in automation.py never sent the synthetic update event for a
`selected_index:` change made via `lvgl.dropdown.update`/`lvgl.roller.update`, unlike `value:`
on number widgets or `text:` on text widgets. Fixed by setting `LvSelect.value_property` to
`CONF_SELECTED_INDEX`.
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
        Path(request.fspath).parent / "config" / "dropdown_update_fires_event_test.yaml"
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


def test_dropdown_update_sends_update_event(main_cpp: str) -> None:
    assert (
        "lv_obj_send_event(test_dropdown->obj, lvgl::lv_update_event, nullptr)"
        in main_cpp
    )
