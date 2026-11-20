"""Tests for ``use_coprocessor``: Wi-Fi on the esp32_hosted co-processor of a host with its own radio."""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32C5,
    VARIANT_ESP32P4,
)
from esphome.components.wifi import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

_NETWORK: ConfigType = {"ssid": "test", "password": "testtest"}
_HOSTED: ConfigType = {"esp32_hosted": {}}


@pytest.fixture
def validate(
    set_core_config: SetCoreConfigCallable,
) -> Callable[..., None]:
    """Run the wifi schema and its final validation on an ESP-IDF target."""

    def _validate(
        wifi: ConfigType,
        variant: str = VARIANT_ESP32C5,
        full_config: ConfigType | None = None,
    ) -> None:
        set_core_config(
            PlatformFramework.ESP32_IDF,
            core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
            platform_data={KEY_BOARD: "esp32-c5-devkitc-1", KEY_VARIANT: variant},
            full_config=_HOSTED if full_config is None else full_config,
        )
        FINAL_VALIDATE_SCHEMA(
            CONFIG_SCHEMA(
                {"use_address": "192.168.1.1", "use_coprocessor": True, **wifi}
            )
        )

    return _validate


def test_use_coprocessor_valid(validate: Callable[..., None]) -> None:
    validate({"networks": [_NETWORK]})


@pytest.mark.parametrize(
    ("wifi", "variant", "full_config", "match"),
    [
        ({"networks": [_NETWORK]}, VARIANT_ESP32C5, {}, "needs esp32_hosted"),
        ({"networks": [_NETWORK]}, VARIANT_ESP32P4, None, "radio of their own"),
        (
            {"networks": [{"ssid": "test", "eap": {"username": "u", "password": "p"}}]},
            VARIANT_ESP32C5,
            None,
            "eap cannot be combined",
        ),
        (
            {"networks": [_NETWORK], "dpp": {}},
            VARIANT_ESP32C5,
            None,
            "dpp cannot be combined",
        ),
        (
            {"networks": [_NETWORK]},
            VARIANT_ESP32C5,
            {**_HOSTED, "espnow": {}},
            "espnow cannot be combined",
        ),
    ],
)
def test_use_coprocessor_invalid(
    validate: Callable[..., None],
    wifi: ConfigType,
    variant: str,
    full_config: ConfigType | None,
    match: str,
) -> None:
    with pytest.raises(cv.Invalid, match=match):
        validate(wifi, variant, full_config)
