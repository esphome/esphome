"""Tests for the LVGL ``debug_outline`` option code generation."""

from __future__ import annotations

import re

_OUTLINE_COLOR_RE = re.compile(
    r"lv_obj_set_style_outline_color\(.+?, (lv_color_make\(.+?\)),"
)
_OUTLINE_WIDTH_RE = re.compile(r"lv_obj_set_style_outline_width\(")
_OUTLINE_PAD_RE = re.compile(r"lv_obj_set_style_outline_pad\(.+?, 0,")


class TestDebugOutlineCodeGeneration:
    """Verify that ``debug_outline`` outlines every widget in a distinct colour."""

    def test_debug_outline_styles_every_widget(
        self, generate_main, component_config_path
    ):
        """Four widgets are declared, so four outlines with different colours appear."""
        main_cpp = generate_main(component_config_path("debug_outline.yaml"))
        assert len(_OUTLINE_WIDTH_RE.findall(main_cpp)) == 4
        assert len(_OUTLINE_PAD_RE.findall(main_cpp)) == 4
        colors = _OUTLINE_COLOR_RE.findall(main_cpp)
        assert len(colors) == 4
        assert len(set(colors)) == 4

    def test_debug_outline_default_emits_nothing(
        self, generate_main, component_config_path
    ):
        """Without ``debug_outline`` no outline styles are generated."""
        main_cpp = generate_main(component_config_path("no_debug_outline.yaml"))
        assert "lv_obj_set_style_outline_" not in main_cpp
