"""Code generation for the input groups of encoders and keypads.

A configuration whose inputs all have a group of their own builds as it did before input groups existed, and
each LVGL instance moves only its own inputs without a group. What inputs without a group do on a device is
covered by the integration tests named test_lvgl_input_groups_*.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


def test_grouped_inputs_generate_no_input_groups(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("input_groups_grouped.yaml"))
    assert not any(define.name == "USE_LVGL_INPUT_GROUPS" for define in CORE.defines)
    # The groups of the instance, the encoder and the keypad. The page and the msgbox get no group.
    assert main_cpp.count("lv_group_create()") == 3
    assert "new(grouped_page) LvPageType(false);" in main_cpp
    for code in (
        "add_ungrouped_input",
        "add_msgbox",
        "set_default_group_for",
        "restore_default_group",
    ):
        assert code not in main_cpp


@pytest.mark.parametrize(
    ("config_file", "lvgl_id", "input_id"),
    [
        ("input_groups_keypad.yaml", "keypad_lvgl", "page_keypad"),
        ("input_groups_encoder.yaml", "encoder_lvgl", "page_encoder"),
    ],
)
def test_input_alone_without_group_uses_input_groups(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    lvgl_id: str,
    input_id: str,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    assert any(define.name == "USE_LVGL_INPUT_GROUPS" for define in CORE.defines)
    assert f"{lvgl_id}->add_ungrouped_input({input_id}->get_drv());" in main_cpp


def test_ungrouped_inputs_register_with_their_own_instance(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("input_groups_ungrouped.yaml"))
    assert any(define.name == "USE_LVGL_INPUT_GROUPS" for define in CORE.defines)
    for lvgl_id, input_id in (
        ("paged_lvgl", "paged_encoder"),
        ("plain_lvgl", "plain_keypad"),
    ):
        assert f"{lvgl_id}->add_ungrouped_input({input_id}->get_drv());" in main_cpp
    assert "set_default_group_for(top_list)" in main_cpp
