"""Tests for LVGL widget state code generation.

These tests cover the change from the old ``add_state``/``clear_state`` helpers
on :class:`Widget` (and on :class:`MatrixButton`) to a single ``set_state``
method that delegates to the new C++ helpers
``LvglComponent::lv_obj_set_state_value`` and
``LvglComponent::lv_buttonmatrix_set_button_ctrl_value``.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    """Generate the C++ output for the shared widget-state YAML config once."""
    return generate_main(component_config_path("widget_state_test.yaml"))


def test_static_state_emits_set_state_value(main_cpp: str) -> None:
    """A widget with ``state: { checked: true, disabled: false }`` should
    generate one ``lv_obj_set_state_value`` call per entry, with the
    appropriate boolean argument.
    """
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_static, LV_STATE_CHECKED, true)"
        in main_cpp
    )
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_static, LV_STATE_DISABLED, false)"
        in main_cpp
    )


def test_lambda_state_emits_set_state_value_with_lambda(main_cpp: str) -> None:
    """A widget with ``state: { pressed: !lambda return true; }`` should
    generate ``lv_obj_set_state_value(..., LV_STATE_PRESSED, <expr>)`` where
    ``<expr>`` is the lambda's return value (cast or inlined), not a static
    bool.
    """
    # The set_state call is emitted for the templated state.
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_lambda, LV_STATE_PRESSED,"
        in main_cpp
    )
    # And it must NOT have collapsed the lambda to a literal true/false.
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_lambda, LV_STATE_PRESSED, true)"
        not in main_cpp
    )
    # The legacy if/else over add_state/remove_state is gone.
    assert "lv_obj_add_state(btn_lambda, LV_STATE_PRESSED)" not in main_cpp
    assert "lv_obj_remove_state(btn_lambda, LV_STATE_PRESSED)" not in main_cpp


def test_widget_disable_action_uses_set_state_value(main_cpp: str) -> None:
    """``lvgl.widget.disable: btn_actions`` should emit a
    ``set_state_value(..., LV_STATE_DISABLED, true)`` call rather than the
    legacy ``lv_obj_add_state``.
    """
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_actions, LV_STATE_DISABLED, true)"
        in main_cpp
    )
    # No leftover legacy add_state for the DISABLED state of this widget.
    assert "lv_obj_add_state(btn_actions, LV_STATE_DISABLED)" not in main_cpp


def test_widget_enable_action_uses_set_state_value(main_cpp: str) -> None:
    """``lvgl.widget.enable: btn_actions`` should emit a
    ``set_state_value(..., LV_STATE_DISABLED, false)`` call rather than the
    legacy ``lv_obj_remove_state``.
    """
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_actions, LV_STATE_DISABLED, false)"
        in main_cpp
    )
    assert "lv_obj_remove_state(btn_actions, LV_STATE_DISABLED)" not in main_cpp


def test_buttonmatrix_disable_action_uses_helper(main_cpp: str) -> None:
    """``lvgl.widget.disable: matrix_btn_a`` should route through the new
    ``lv_buttonmatrix_set_button_ctrl_value`` helper for button index 0
    with the ``DISABLED`` control bit set to ``true``, instead of the
    legacy ``lv_buttonmatrix_set_button_ctrl``.

    The button matrix obj is the compound's ``obj`` member and the index
    is the position of the button in the row layout.
    """
    assert (
        "LvglComponent::lv_buttonmatrix_set_button_ctrl_value(matrix->obj, 0, "
        "LV_BUTTONMATRIX_CTRL_DISABLED, true)"
    ) in main_cpp


def test_buttonmatrix_enable_action_uses_helper(main_cpp: str) -> None:
    """``lvgl.widget.enable: matrix_btn_a`` should route through the new
    ``lv_buttonmatrix_set_button_ctrl_value`` helper for button index 0
    with the ``DISABLED`` control bit set to ``false``, instead of the
    legacy ``lv_buttonmatrix_clear_button_ctrl``.
    """
    assert (
        "LvglComponent::lv_buttonmatrix_set_button_ctrl_value(matrix->obj, 0, "
        "LV_BUTTONMATRIX_CTRL_DISABLED, false)"
    ) in main_cpp
    # The legacy clear_button_ctrl path is gone for the matrix button enable
    # action.
    assert (
        "lv_buttonmatrix_clear_button_ctrl(matrix->obj, 0, LV_BUTTONMATRIX_CTRL_DISABLED)"
        not in main_cpp
    )


def test_lvgl_switch_control_calls_set_state_value(main_cpp: str) -> None:
    """The LVGL switch platform installs a control lambda that mirrors the
    switch's bool value into ``LV_STATE_CHECKED`` via
    ``lv_obj_set_state_value`` (replacing the previous if/else over
    ``add_state``/``clear_state`` plus an explicit ``send_event`` of
    ``lv_api_event``).
    """
    # The control lambda calls the new helper with the bool ``v`` parameter.
    assert (
        "LvglComponent::lv_obj_set_state_value(switch_widget, LV_STATE_CHECKED, v)"
        in main_cpp
    )
    # The deprecated lv_api_event symbol must no longer appear anywhere.
    assert "lv_api_event" not in main_cpp


def test_default_state_does_not_emit_set_state_value(main_cpp: str) -> None:
    """A widget without a ``state:`` block must not generate any
    ``lv_obj_set_state_value`` calls for it. (Sanity-check that the
    new code path is opt-in driven by the YAML.)
    """
    assert (
        "LvglComponent::lv_obj_set_state_value(switch_widget, LV_STATE_DISABLED"
        not in main_cpp
    )
    assert (
        "LvglComponent::lv_obj_set_state_value(btn_static, LV_STATE_PRESSED"
        not in main_cpp
    )
