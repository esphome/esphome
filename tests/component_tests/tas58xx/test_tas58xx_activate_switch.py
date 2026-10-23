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
    # The amplifier always starts in play, so the switch state is never saved or restored
    assert "activate->set_restore_mode(switch_::SWITCH_ALWAYS_ON);" in main_cpp
    # A configuration entity by default, as the pcm5122 switch
    assert "category:config" in main_cpp


@pytest.mark.parametrize(
    ("option", "message"),
    [
        ({"restore_mode": "ALWAYS_OFF"}, "so only ALWAYS_ON is allowed"),
        ({"restore_mode": "RESTORE_DEFAULT_ON"}, "so only ALWAYS_ON is allowed"),
        ({"restore_mode": "RESTORE_DEFAULT_OFF"}, "so only ALWAYS_ON is allowed"),
        ({"restore_state": {"state": "KEEP"}}, "the switch state is not restored"),
        ({"restore_state": "none"}, "the switch state is not restored"),
        ({"initial_state": "OFF"}, "the switch state is not restored"),
    ],
)
def test_activate_switch_rejects_restore_options(
    set_core_config: SetCoreConfigCallable, option: dict, message: str
) -> None:
    """Only restore_mode ALWAYS_ON is accepted: the amplifier always starts in play."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match=message):
        cv.Schema(CONFIG_SCHEMA)({"activate": {"name": "Amplifier", **option}})
