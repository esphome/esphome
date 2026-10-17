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


@pytest.mark.parametrize(
    ("pin_count", "device"), [(4, "PCA9536"), (8, "PCA9554"), (16, "PCA9555")]
)
def test_legacy_pin_count_maps_to_a_device(pin_count: int, device: str) -> None:
    hub = _hub(pin_count=pin_count)
    assert hub["device"] == device
    assert hub["pin_count"] == pin_count


def test_device_defaults_to_pca9554() -> None:
    hub = _hub()
    assert hub["device"] == "PCA9554"
    assert hub["pin_count"] == 8


@pytest.mark.parametrize(
    ("device", "address"),
    [("PCA9554", 0x20), ("PCA9554A", 0x38), ("PCA9536", 0x41), ("PCAL9538", 0x70)],
)
def test_address_defaults_to_the_device_base(device: str, address: int) -> None:
    hub = CONFIG_SCHEMA({"id": "hub", "device": device})
    assert hub["address"] == address
    assert _hub(device=device, address=0x27)["address"] == 0x27


def test_pin_number_follows_the_device() -> None:
    with pytest.raises(cv.Invalid, match="range 0-3"):
        pca9554_pin_final_validate(_pin(4, {"input": True}), _hub(device="PCA9536"))
    pca9554_pin_final_validate(_pin(15, {"input": True}), _hub(device="PCAL9555"))


def test_pcal_modes_are_rejected_on_a_plain_part() -> None:
    with pytest.raises(cv.Invalid, match="'pullup' is not valid for device PCA9554"):
        pca9554_pin_final_validate(
            _pin(1, {"input": True, "pullup": True}), _hub(device="PCA9554")
        )


def test_drive_strength_is_rejected_on_a_plain_part() -> None:
    with pytest.raises(cv.Invalid, match="'drive_strength' is not supported"):
        pca9554_pin_final_validate(
            _pin(1, {"output": True}, drive_strength=3), _hub(device="PCA9554")
        )
    pca9554_pin_final_validate(_pin(1, {"output": True}), _hub(device="PCA9554"))


def test_pcal_input_with_interrupt_pin_needs_no_extra_keys() -> None:
    """A PCAL hub with an interrupt pin accepts a plain input pin; there is no per-pin interrupt key."""
    # The final validation only looks at the keys, so the hub config can stay a plain dict
    hub = {"device": "PCAL9538", "pin_count": 8, "interrupt_pin": {"number": 4}}
    pca9554_pin_final_validate(_pin(1, {"input": True}), hub)


def test_pcal_code_is_only_generated_for_pcal_hubs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The define and the hub setters exist only where a PCAL part is configured."""
    main_cpp = generate_main(component_config_path("pca9554.yaml"))

    for call in (
        "pcal_hub->set_pcal(true);",
        "pcal_hub->set_latch(9);",
        "pcal_hub->set_drive_strength(9, 2);",
    ):
        assert main_cpp.count(call) == 1
    for call in ("set_pcal(", "set_latch(", "set_drive_strength("):
        assert f"plain_hub->{call}" not in main_cpp
