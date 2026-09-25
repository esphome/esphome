"""Tests for LVGL's multi-instance config cross-checks."""

from __future__ import annotations

import pytest

from esphome.components.lvgl import defines as df, multi_conf_validate
from esphome.components.lvgl.schemas import theme_schema
from esphome.config_validation import Invalid


def _config(displays: list[str], theme: dict | None = None) -> dict:
    config = {
        df.CONF_DISPLAYS: displays,
        "log_level": "WARN",
        "color_depth": 16,
        "byte_order": "big_endian",
        df.CONF_TRANSPARENCY_KEY: 0x000400,
    }
    if theme is not None:
        config[df.CONF_THEME] = theme
    return config


class TestThemeOnMultipleInstances:
    def test_raises_when_two_instances_have_theme(self) -> None:
        configs = [
            _config(["disp_a"], theme={df.CONF_DARK_MODE: True}),
            _config(["disp_b"], theme={df.CONF_DARK_MODE: False}),
        ]
        with pytest.raises(Invalid, match="'theme' may only be set on one"):
            multi_conf_validate(configs)

    def test_raises_even_with_an_empty_theme_block(self) -> None:
        # `theme: {}` still creates a CONF_THEME key (with dark_mode defaulted
        # by the schema), so it should be treated the same as a populated one.
        # Run it through the real schema rather than hand-building the dict,
        # so this actually pins that defaulting behaviour.
        configs = [
            _config(["disp_a"], theme=theme_schema({})),
            _config(["disp_b"], theme=theme_schema({})),
        ]
        with pytest.raises(Invalid, match="'theme' may only be set on one"):
            multi_conf_validate(configs)

    def test_passes_when_only_one_instance_has_theme(self) -> None:
        configs = [
            _config(["disp_a"], theme={df.CONF_DARK_MODE: True}),
            _config(["disp_b"]),
        ]
        multi_conf_validate(configs)

    def test_passes_when_no_instance_has_theme(self) -> None:
        configs = [_config(["disp_a"]), _config(["disp_b"])]
        multi_conf_validate(configs)
