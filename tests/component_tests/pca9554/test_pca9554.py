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
from esphome.types import ConfigType


def _hub(**overrides: object) -> ConfigType:
    return CONFIG_SCHEMA({"id": "hub", **overrides})


def _pin(number: int, mode: dict, **overrides: object) -> ConfigType:
    return PCA9554_PIN_SCHEMA(
        {"pca9554": "hub", "number": number, "mode": mode, **overrides}
    )


def test_pin_count_must_match_the_device() -> None:
    """A stale pin_count next to a device would address every register wrongly."""
    with pytest.raises(
        cv.Invalid, match="does not match the 8 pins of device PCAL9538"
    ):
        _hub(device="PCAL9538", pin_count=16)


@pytest.mark.parametrize(
    ("overrides", "device"),
    [
        ({}, "PCA9554"),
        ({"pin_count": 4}, "PCA9536"),
        ({"pin_count": 8}, "PCA9554"),
        ({"pin_count": 16}, "PCA9555"),
    ],
)
def test_legacy_pin_count_maps_to_a_device(overrides: dict, device: str) -> None:
    assert _hub(**overrides)["device"] == device


@pytest.mark.parametrize(
    ("device", "address"),
    [
        ("PCA9554", 0x20),
        ("PCA9554A", 0x38),
        ("PCA9536", 0x41),
        ("PCA9538", 0x70),
        ("PCAL9538", 0x70),
    ],
)
def test_address_defaults_to_the_device_base(device: str, address: int) -> None:
    assert _hub(device=device)["address"] == address
    assert _hub(device=device, address=0x27)["address"] == 0x27


def test_pin_number_follows_the_device() -> None:
    with pytest.raises(cv.Invalid, match="range 0-3"):
        pca9554_pin_final_validate(_pin(4, {"input": True}), _hub(device="PCA9536"))
    pca9554_pin_final_validate(_pin(15, {"input": True}), _hub(device="PCAL9555"))


@pytest.mark.parametrize(
    ("mode", "overrides", "message"),
    [
        ({"input": True, "pullup": True}, {}, "Pin mode 'pullup' is not valid"),
        ({"input": True, "pulldown": True}, {}, "Pin mode 'pulldown' is not valid"),
        ({"input": True}, {"latch": True}, "'latch' is not supported"),
        ({"output": True}, {"drive_strength": "100%"}, "'drive_strength' is not"),
    ],
)
def test_pcal_only_settings_are_rejected_on_a_plain_part(
    mode: dict, overrides: dict, message: str
) -> None:
    pin = _pin(1, mode, **overrides)
    with pytest.raises(cv.Invalid, match=f"{message}.*device PCA9554"):
        pca9554_pin_final_validate(pin, _hub(device="PCA9554"))
    pca9554_pin_final_validate(pin, _hub(device="PCAL9538"))


def test_outputs_cannot_be_latched() -> None:
    with pytest.raises(cv.Invalid, match="'latch' is not valid for an output pin"):
        pca9554_pin_final_validate(
            _pin(1, {"output": True}, latch=True), _hub(device="PCAL9538")
        )


@pytest.mark.parametrize("value", ["30%", "0%", 2])
def test_drive_strength_must_be_a_datasheet_level(value: object) -> None:
    with pytest.raises(cv.Invalid):
        _pin(1, {"output": True}, drive_strength=value)


def test_pcal_code_is_only_generated_for_pcal_hubs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The define and the hub setters exist only where a PCAL part is configured."""
    main_cpp = generate_main(component_config_path("pca9554.yaml"))

    for call in (
        "pcal_hub->set_pcal();",
        "pcal_hub->set_latch(9);",
        "pcal_hub->set_drive_strength(9, 2);",
        "pcal_hub->set_drive_strength(10, 0);",
        "pcal_hub->set_drive_strength(11, 3);",
    ):
        assert main_cpp.count(call) == 1
    for call in ("set_pcal(", "set_latch(", "set_drive_strength("):
        assert f"plain_hub->{call}" not in main_cpp
        assert f"pca9538_hub->{call}" not in main_cpp
    # 0x70, the PCA9538 default
    assert "pca9538_hub->set_i2c_address(112);" in main_cpp
