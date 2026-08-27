"""Tests for the selector a named style is added to a widget with.

A part and a state are combined into one value, but they live in different halves of it:
a state fits in 16 bits, while a part starts at bit 16. Casting the pair to the state type
would throw the part away and quietly move the style to the main part, so these pin the
type each selector is generated with.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest


def _selectors(main_cpp: str, var: str, style: str) -> set[str]:
    """Return every selector a named style was added to a widget with."""
    return set(
        re.findall(
            rf"lv_obj_add_style\({var}, {style}, (.+?)\);",
            main_cpp,
        )
    )


class TestNamedStyleSelectors:
    @pytest.fixture()
    def main_cpp(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> str:
        return generate_main(component_config_path("named_styles.yaml"))

    def test_every_selector_is_generated(self, main_cpp: str) -> None:
        assert _selectors(main_cpp, "plain_slider", "chunky") == {
            # Neither a part nor a state, which names the main part.
            "LV_PART_MAIN",
            # A state on its own, which needs no cast as it is the narrower half.
            "LV_STATE_PRESSED",
            # A part on its own.
            "LV_PART_KNOB",
            # A part with a state, cast to the type LVGL takes, which is wide enough for
            # both. The state type alone would drop the part.
            "(lv_style_selector_t)((int)LV_PART_KNOB|(int)LV_STATE_PRESSED)",
        }

    def test_a_combined_selector_is_not_narrowed_to_a_state(
        self, main_cpp: str
    ) -> None:
        """`lv_state_t` is 16 bits, so it cannot hold a part."""
        assert "(lv_state_t)((int)LV_PART_KNOB" not in main_cpp
