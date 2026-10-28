"""Tests for the TAS58xx mixer mode select."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.tas58xx.select import FINAL_VALIDATE_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_mixer_select_options_follow_dac_mode(
    generate_main: Callable[[str | Path], str],
) -> None:
    """BTL offers every mixer mode, PBTL only mono, left and right, starting at its first mode."""
    main_cpp = generate_main(
        "tests/component_tests/tas58xx/test_tas58xx_mixer_select.yaml"
    )

    assert (
        'select_options[] PROGMEM = {"Stereo", "Stereo inverse", "Mono", "Left", "Right"};'
        in main_cpp
    )
    assert (
        "apply_mixer_mode(static_cast<tas58xx::MixerMode>(tas58xx::MIXER_MODE_STEREO + index))"
        in main_cpp
    )
    assert "mixer_btl->traits.set_options_static(select_options, 5);" in main_cpp
    assert 'select_options_2[] PROGMEM = {"Mono", "Left", "Right"};' in main_cpp
    assert (
        "apply_mixer_mode(static_cast<tas58xx::MixerMode>(tas58xx::MIXER_MODE_MONO + index))"
        in main_cpp
    )
    assert "mixer_pbtl->traits.set_options_static(select_options_2, 3);" in main_cpp


def test_mixer_select_is_given_its_amplifier(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each select is handed to its own amplifier, which applies the mode."""
    main_cpp = generate_main(
        "tests/component_tests/tas58xx/test_tas58xx_mixer_select.yaml"
    )

    # The amplifier publishes the select itself, after it accepts a mode
    assert "publish_state(index)" not in main_cpp
    assert "amp_btl->set_mixer_mode_select(mixer_btl);" in main_cpp
    assert "amp_pbtl->set_mixer_mode_select(mixer_pbtl);" in main_cpp
    assert "App.register_component_(mixer_btl" not in main_cpp


def test_second_select_entry_for_one_amplifier_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The amplifier keeps one select, so a second entry for it would take over the first."""
    entry = {"platform": "tas58xx", "tas58xx_id": "amp"}
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config={"select": [entry, dict(entry)]}
    )

    with pytest.raises(cv.Invalid, match="Only one tas58xx select entry"):
        FINAL_VALIDATE_SCHEMA(entry)


def test_each_amplifier_may_have_its_own_select(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Selects of different amplifiers do not compete."""
    entries = [
        {"platform": "tas58xx", "tas58xx_id": "amp_a"},
        {"platform": "tas58xx", "tas58xx_id": "amp_b"},
    ]
    set_core_config(PlatformFramework.ESP32_IDF, full_config={"select": entries})

    for entry in entries:
        assert FINAL_VALIDATE_SCHEMA(entry) is entry
