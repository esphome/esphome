"""Tests for the TAS58xx activate switch."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.tas58xx.switch import CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_activate_switch_is_given_to_its_amplifier_only(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The switch's amplifier publishes to it; an amplifier without a switch gets none."""
    main_cpp = generate_main(
        "tests/component_tests/tas58xx/test_tas58xx_activate_switch.yaml"
    )

    # Turning the switch on or off calls its amplifier, which publishes the state itself
    assert "::amp_with_switch->set_active(state);" in main_cpp
    assert "entity->publish_state(state);" not in main_cpp
    assert "amp_with_switch->set_activate_switch(activate);" in main_cpp
    assert "amp_without_switch->set_activate_switch(" not in main_cpp
    # The switch shows the state the amplifier publishes, so its restore mode is disabled
    assert "activate->set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);" in main_cpp
    # A configuration entity by default, as the pcm5122 switch
    assert "category:config" in main_cpp


@pytest.mark.parametrize(
    "option",
    [
        {"restore_mode": "ALWAYS_ON"},
        {"restore_mode": "RESTORE_DEFAULT_OFF"},
        {"restore_state": {"state": "KEEP"}},
        {"initial_state": "OFF"},
    ],
)
def test_activate_switch_rejects_restore_options(
    set_core_config: SetCoreConfigCallable, option: dict
) -> None:
    """The switch shows the amplifier's state, so the restore options have no effect and are rejected."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(
        cv.Invalid, match="the activate switch shows the amplifier's state"
    ):
        cv.Schema(CONFIG_SCHEMA)({"activate": {"name": "Amplifier", **option}})
