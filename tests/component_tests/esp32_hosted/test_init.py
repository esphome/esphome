"""Tests for the esp32_hosted ESP-IDF version gate and esp_hosted line choice."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_hosted import (
    CONF_ACTIVE_HIGH,
    ESP_HOSTED_COMPONENT,
    _final_validate,
    uses_esp_hosted_3x,
)
from esphome.const import (
    CONF_COMPONENTS,
    CONF_FRAMEWORK,
    CONF_NAME,
    CONF_REF,
    PlatformFramework,
)
from esphome.types import ConfigType

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("pin", "expected"),
    [
        (None, True),
        ({CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: "==2.12.13"}, False),
        ({CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: "==3.0.10"}, True),
        ({CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: ">=2.12"}, True),
        ({CONF_NAME: ESP_HOSTED_COMPONENT}, False),
    ],
)
def test_uses_esp_hosted_3x(
    set_core_config: SetCoreConfigCallable, pin: ConfigType | None, expected: bool
) -> None:
    """3.x unless the user pins a 2.x version."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={
            "esp32": {CONF_FRAMEWORK: {CONF_COMPONENTS: [pin] if pin else []}}
        },
    )
    assert uses_esp_hosted_3x() is expected


@pytest.mark.parametrize(
    ("idf", "active_high", "match"),
    [
        ("5.4.2", True, "requires ESP-IDF 5.5"),
        ("5.5.6", False, "active_high: false"),
    ],
)
def test_final_validate_rejects_what_3x_cannot_build(
    set_core_config: SetCoreConfigCallable, idf: str, active_high: bool, match: str
) -> None:
    """Without a 2.x pin, 3.x is used and its limits are errors, not fallbacks."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
        full_config={"esp32": {CONF_FRAMEWORK: {CONF_COMPONENTS: []}}},
    )
    with pytest.raises(cv.Invalid, match=match):
        _final_validate({CONF_ACTIVE_HIGH: active_high})


def test_final_validate_accepts_2x_pin_on_old_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A 2.x pin keeps ESP-IDF 5.4 and active_high: false building."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse("5.4.2")},
        full_config={
            "esp32": {
                CONF_FRAMEWORK: {
                    CONF_COMPONENTS: [
                        {CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: "==2.12.13"}
                    ]
                }
            }
        },
    )
    _final_validate({CONF_ACTIVE_HIGH: False})


@pytest.mark.parametrize("idf", ["5.5.5", "5.5.6"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.5 and newer passes validation unchanged on the 3.x default."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
        full_config={"esp32": {CONF_FRAMEWORK: {CONF_COMPONENTS: []}}},
    )
    _final_validate({CONF_ACTIVE_HIGH: True})


@pytest.mark.parametrize("idf", ["5.0.0", "5.2.2"])
def test_final_validate_rejects_old_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF older than 5.3 is rejected with a clear error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    with pytest.raises(cv.Invalid, match="requires ESP-IDF 5.3 or newer"):
        _final_validate({})
