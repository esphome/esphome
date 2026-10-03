"""Validation tests for the ESPectre SDK adapter."""

import pytest

from esphome.components import espectre
from esphome.components.wifi import POWER_SAVE_OFF_REASONS_KEY
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_ESP32,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
)
from esphome.core import CORE
from esphome.types import ConfigType


@pytest.fixture(autouse=True)
def esp32_core() -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "esp32",
        KEY_TARGET_FRAMEWORK: "esp-idf",
        KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 3),
    }
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32"}


@pytest.mark.parametrize("variant", ["ESP32C2", "ESP32H2", "ESP32P4"])
def test_unsupported_targets(variant: str) -> None:
    CORE.data[KEY_ESP32][KEY_VARIANT] = variant
    with pytest.raises(cv.Invalid, match="only available"):
        espectre.CONFIG_SCHEMA({})


def test_rejects_idf_below_minimum() -> None:
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(5, 5, 2)
    with pytest.raises(cv.Invalid, match="5.5.3"):
        espectre.CONFIG_SCHEMA({})


@pytest.mark.parametrize(
    ("config", "message"),
    [
        (
            {"traffic_generator_mode": "wifi_raw", "csi_capture_profile": "ht_vht"},
            "ht_vht CSI capture profile",
        ),
        (
            {
                "traffic_generator_mode": "wifi_raw",
                "traffic_generator_target_ip": "192.168.1.1",
            },
            "does not use",
        ),
        (
            {"csi_traffic_multicast_group": "239.255.0.1"},
            "requires traffic_generator_mode: external",
        ),
    ],
)
def test_incompatible_traffic_options(config: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        espectre.CONFIG_SCHEMA(config)


def test_c6_raw_traffic() -> None:
    CORE.data[KEY_ESP32][KEY_VARIANT] = "ESP32C6"
    with pytest.raises(cv.Invalid, match="not supported on ESP32-C6"):
        espectre.CONFIG_SCHEMA({"traffic_generator_mode": "wifi_raw"})


@pytest.mark.parametrize(
    "address",
    [
        "0.0.0.0",
        "127.0.0.1",
        "224.0.0.1",
        "255.255.255.255",
    ],
)
def test_invalid_traffic_targets(address: str) -> None:
    with pytest.raises(cv.Invalid, match="unicast"):
        espectre.CONFIG_SCHEMA({"traffic_generator_target_ip": address})


def test_multicast_group_must_be_multicast() -> None:
    with pytest.raises(cv.Invalid, match="multicast address"):
        espectre.CONFIG_SCHEMA(
            {
                "traffic_generator_mode": "external",
                "csi_traffic_multicast_group": "192.168.1.1",
            }
        )


def test_empty_multicast_group_skips_joining() -> None:
    config = espectre.CONFIG_SCHEMA(
        {"traffic_generator_mode": "external", "csi_traffic_multicast_group": " "}
    )
    assert config["csi_traffic_multicast_group"] == ""


def test_keeps_wifi_power_save_off() -> None:
    espectre.final_validate({})
    assert any("ESPectre" in reason for reason in CORE.data[POWER_SAVE_OFF_REASONS_KEY])
