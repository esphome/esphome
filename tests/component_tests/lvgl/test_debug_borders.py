"""Tests for the LVGL ``debug_borders`` option code generation."""

from __future__ import annotations

import re

_BORDER_COLOR_RE = re.compile(
    r"lv_obj_set_style_border_color\(.+?, (lv_color_make\(.+?\)),"
)
_BORDER_WIDTH_RE = re.compile(r"lv_obj_set_style_border_width\(")
_BORDER_POST_RE = re.compile(r"lv_obj_set_style_border_post\(.+?, true,")


class TestDebugBordersCodeGeneration:
    """Verify that ``debug_borders`` adds a distinct border to every widget."""

    def test_debug_borders_styles_every_widget(
        self, generate_main, component_config_path
    ):
        """Four widgets are declared, so four borders with different colours appear."""
        main_cpp = generate_main(component_config_path("debug_borders.yaml"))
        assert len(_BORDER_WIDTH_RE.findall(main_cpp)) == 4
        assert len(_BORDER_POST_RE.findall(main_cpp)) == 4
        colors = _BORDER_COLOR_RE.findall(main_cpp)
        assert len(colors) == 4
        assert len(set(colors)) == 4

    def test_debug_borders_default_emits_nothing(
        self, generate_main, component_config_path
    ):
        """Without ``debug_borders`` no border styles are generated."""
        main_cpp = generate_main(component_config_path("no_debug_borders.yaml"))
        assert "lv_obj_set_style_border_" not in main_cpp
