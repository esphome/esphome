"""Tests for the PCA9554 device table and its pin validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.pca9554 import (
    CONFIG_SCHEMA,
    PCA9554_PIN_SCHEMA,
    pca9554_pin_final_validate,
)
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _hub(**overrides: object) -> dict:
    config: dict = {"id": "hub", "address": 0x20}
    config.update(overrides)
    return CONFIG_SCHEMA(config)


def _pin(number: int, mode: dict, **extra: object) -> dict:
    config: dict = {"pca9554": "hub", "number": number, "mode": mode}
    config.update(extra)
    return PCA9554_PIN_SCHEMA(config)


def test_pin_count_must_match_the_device() -> None:
    """A stale pin_count next to a device would address every register wrongly."""
    with pytest.raises(
        cv.Invalid, match="does not match the 8 pins of device PCAL9538"
    ):
        _hub(device="PCAL9538", pin_count=16)


def test_pin_count_alone_keeps_the_old_behaviour() -> None:
    assert _hub(pin_count=16)["device"] == "NONE"


def test_pin_number_follows_the_device(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="range 0-3"):
        pca9554_pin_final_validate(_pin(4, {"input": True}), _hub(device="PCA9536"))
    pca9554_pin_final_validate(_pin(15, {"input": True}), _hub(device="PCAL9555"))


def test_pcal_modes_are_rejected_on_a_plain_part() -> None:
    with pytest.raises(cv.Invalid, match="'pullup' is not valid for device PCA9554"):
        pca9554_pin_final_validate(
            _pin(1, {"input": True, "pullup": True}), _hub(device="PCA9554")
        )


def test_drive_strength_is_rejected_on_a_plain_part(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="'drive_strength' is not supported"):
        pca9554_pin_final_validate(
            _pin(1, {"output": True}, drive_strength=0), _hub(device="PCA9554")
        )
    # The default is the power-on value, so it is not a request
    pca9554_pin_final_validate(_pin(1, {"output": True}), _hub(device="PCA9554"))


def test_pcal_input_with_interrupt_pin_must_opt_in(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    """The mask register powers up fully masked, so a silent input never updates."""
    # The final validation only looks at the keys, so the hub config can stay a plain dict
    hub = {"device": "PCAL9538", "interrupt_pin": {"number": 4}}
    with pytest.raises(cv.Invalid, match="needs 'interrupt: true'"):
        pca9554_pin_final_validate(_pin(1, {"input": True}), hub)
    pca9554_pin_final_validate(_pin(1, {"input": True, "interrupt": True}), hub)
    # Outputs and hubs without an interrupt pin are not affected
    pca9554_pin_final_validate(_pin(2, {"output": True}), hub)
    pca9554_pin_final_validate(_pin(1, {"input": True}), _hub(device="PCAL9538"))


def test_pcal_code_is_only_generated_for_pcal_hubs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The define and the per-pin setters exist only where a PCAL part is configured."""
    main_cpp = generate_main(component_config_path("pca9554.yaml"))

    assert "pcal_hub->set_pcal(true);" in main_cpp
    assert "plain_hub->set_pcal(" not in main_cpp
    assert "set_latch(true);" in main_cpp
    assert "set_interrupt(true);" in main_cpp
    assert "set_drive_strength(2);" in main_cpp
    assert main_cpp.count("set_latch(") == 1
    assert main_cpp.count("set_drive_strength(") == 1
