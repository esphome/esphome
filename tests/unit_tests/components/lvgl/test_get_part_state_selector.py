"""Tests for get_part_state_selector()'s three branches."""

from __future__ import annotations

from esphome.components.lvgl.defines import get_part_state_selector


def test_default_state_returns_bare_part() -> None:
    assert str(get_part_state_selector("main", "default")) == "LV_PART_MAIN"
    assert str(get_part_state_selector("knob", "default")) == "LV_PART_KNOB"


def test_main_part_with_non_default_state_returns_bare_state() -> None:
    assert str(get_part_state_selector("main", "pressed")) == "LV_STATE_PRESSED"


def test_non_main_part_with_non_default_state_combines_both() -> None:
    assert str(get_part_state_selector("knob", "pressed")) == (
        "(static_cast<lv_style_selector_t>(LV_STATE_PRESSED) | "
        "static_cast<lv_style_selector_t>(LV_PART_KNOB))"
    )


def test_accepts_already_prefixed_part_and_state() -> None:
    assert (
        str(get_part_state_selector("LV_PART_MAIN", "LV_STATE_DEFAULT"))
        == "LV_PART_MAIN"
    )
    assert (
        str(get_part_state_selector("LV_PART_KNOB", "LV_STATE_PRESSED"))
        == "(static_cast<lv_style_selector_t>(LV_STATE_PRESSED) | "
        "static_cast<lv_style_selector_t>(LV_PART_KNOB))"
    )
