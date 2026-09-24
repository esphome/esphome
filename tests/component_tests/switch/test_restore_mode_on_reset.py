"""Tests for switch restore_mode_on_reset validation and codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.switch import (
    _RESTORE_MODE_ON_RESET_SCHEMA,
    RESET_CAUSES,
    RESTORE_MODES,
)
import esphome.config_validation as cv
from esphome.core import CORE

GATE = "USE_SWITCH_RESTORE_MODE_ON_RESET"


def test_table_is_emitted_sorted_and_paired(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Causes are emitted in sorted order, each followed by its own mode byte."""
    main_cpp = generate_main(component_config_path("restore_mode_on_reset.yaml"))

    assert (
        "static const uint8_t override_switch_reset_overrides[] = {"
        "static_cast<uint8_t>(::ResetCause::RESET_CAUSE_EXTERNAL), "
        "static_cast<uint8_t>(switch_::SWITCH_ALWAYS_ON), "
        "static_cast<uint8_t>(::ResetCause::RESET_CAUSE_SOFTWARE), "
        "static_cast<uint8_t>(switch_::SWITCH_RESTORE_DEFAULT_OFF), "
        "static_cast<uint8_t>(::ResetCause::RESET_CAUSE_WATCHDOG), "
        "static_cast<uint8_t>(switch_::SWITCH_ALWAYS_OFF)};"
    ) in main_cpp
    # The count is in entries (pairs), not bytes.
    assert (
        "override_switch->set_restore_mode_on_reset(override_switch_reset_overrides, 3);"
        in main_cpp
    )
    # A switch in the same config that does not use the option gets no table.
    assert "plain_switch->set_restore_mode_on_reset(" not in main_cpp
    assert GATE in {define.name for define in CORE.defines}


def test_gate_and_table_absent_when_option_unused(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without the option nothing is emitted, so existing configs compile as before."""
    main_cpp = generate_main(component_config_path("restore_mode_on_reset_absent.yaml"))

    assert "set_restore_mode_on_reset(" not in main_cpp
    assert "_reset_overrides[]" not in main_cpp
    assert GATE not in {define.name for define in CORE.defines}


@pytest.mark.parametrize(
    "value",
    [
        {},  # at least one cause is required
        {"power_on": "ALWAYS_ON"},  # deliberately has no YAML spelling
        {"unknown": "ALWAYS_ON"},  # UNKNOWN never has a YAML spelling
        {"bogus": "ALWAYS_ON"},
        {"software": "NOT_A_MODE"},
    ],
)
def test_invalid_mappings_are_rejected(value: dict) -> None:
    with pytest.raises(cv.Invalid):
        _RESTORE_MODE_ON_RESET_SCHEMA(value)


@pytest.mark.parametrize("cause", RESET_CAUSES)
def test_each_cause_accepts_restore_mode_values(cause: str) -> None:
    """Every cause takes restore_mode's own validator, case-insensitively."""
    for mode in ("restore_default_off", "DISABLED"):
        value = _RESTORE_MODE_ON_RESET_SCHEMA({cause: mode})
        assert value[cause].enum_value == RESTORE_MODES[mode.upper()]
