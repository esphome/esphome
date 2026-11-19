"""Tests for the LVGL configuration defines written to lv_conf.h."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.lvgl import defines as df
from esphome.components.lvgl.schemas import WIDGET_TYPES

# LVGL 9.6 stops the build with #error when an enabled option "selects" another
# option that is disabled. ESPHome sets every unused LV_USE_* to 0, so each widget
# must list these in get_uses().
LVGL_SELECTS = (
    ("SLIDER", "BAR"),
    ("BARCODE", "CANVAS"),
    ("QRCODE", "CANVAS"),
    ("CANVAS", "IMAGE"),
    ("DROPDOWN", "LABEL"),
    ("MSGBOX", "LABEL"),
    ("ROLLER", "LABEL"),
    ("TEXTAREA", "LABEL"),
)


def test_default_lv_conf_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("lv_conf_defaults.yaml"))
    defines = df.get_defines()
    assert defines["LV_COLOR_FORMAT_DEFAULT"] == "LV_COLOR_FORMAT_RGB565_SWAPPED"
    assert defines["LV_DRAW_SW_SUPPORT_RGB565_SWAPPED"] == "1"
    assert defines["LV_OBJ_STYLE_CACHE"] == "1"
    assert defines["LV_USE_CHECK_ARG"] == "0"
    assert "LV_CHECK_ARG_LOG_MODE" not in defines
    # Both are deprecated in LVGL 9.6 and produce build warnings
    assert "LV_COLOR_DEPTH" not in defines
    assert "LV_COLOR_16_SWAP" not in defines


@pytest.mark.parametrize(
    ("yaml_file", "color_format", "log_mode"),
    [
        ("lv_conf_options.yaml", "RGB565", "VERBOSE"),
        ("lv_conf_check_warn.yaml", "RGB565_SWAPPED", "MINIMAL"),
        # Only the second LVGL instance sets check_args
        ("lv_conf_check_second.yaml", "RGB565_SWAPPED", "MINIMAL"),
    ],
)
def test_lv_conf_options(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    yaml_file: str,
    color_format: str,
    log_mode: str,
) -> None:
    generate_main(component_config_path(yaml_file))
    defines = df.get_defines()
    assert defines["LV_COLOR_FORMAT_DEFAULT"] == f"LV_COLOR_FORMAT_{color_format}"
    assert defines["LV_USE_CHECK_ARG"] == "1"
    assert defines["LV_CHECK_ARG_LOG_MODE"] == f"LV_CHECK_ARG_LOG_MODE_{log_mode}"


def test_check_arg_log_modes_cover_all_log_levels() -> None:
    assert df.LV_CHECK_ARG_LOG_MODES.keys() == df.LV_LOG_LEVELS.keys()


@pytest.mark.parametrize("widget_name", sorted(WIDGET_TYPES))
def test_widget_uses_satisfy_lvgl_selects(widget_name: str) -> None:
    widget_type = WIDGET_TYPES[widget_name]
    df.add_lv_use(widget_type.name, *widget_type.get_uses())
    enabled = {use.upper() for use in df.get_lv_uses()}
    for option, required in LVGL_SELECTS:
        if option in enabled:
            assert required in enabled, (
                f"'{widget_name}' enables LV_USE_{option}, which needs LV_USE_{required}"
            )
