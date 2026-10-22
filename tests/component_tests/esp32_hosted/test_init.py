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
    ("idf", "active_high", "pin", "expected"),
    [
        ("5.5.6", True, None, True),
        ("5.4.2", True, None, False),
        ("5.5.6", False, None, False),
        (
            "5.5.6",
            True,
            {CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: "==2.12.13"},
            False,
        ),
        ("5.5.6", False, {CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: "==3.0.10"}, True),
        ("5.5.6", True, {CONF_NAME: ESP_HOSTED_COMPONENT, CONF_REF: ">=2.12"}, True),
        ("5.5.6", True, {CONF_NAME: ESP_HOSTED_COMPONENT}, False),
    ],
)
def test_uses_esp_hosted_3x(
    set_core_config: SetCoreConfigCallable,
    idf: str,
    active_high: bool,
    pin: ConfigType | None,
    expected: bool,
) -> None:
    """3.x by default on ESP-IDF 5.5 with active_high; a user pin always wins."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
        full_config={
            "esp32": {CONF_FRAMEWORK: {CONF_COMPONENTS: [pin] if pin else []}},
            "esp32_hosted": {CONF_ACTIVE_HIGH: active_high},
        },
    )
    assert uses_esp_hosted_3x() is expected


@pytest.mark.parametrize("idf", ["5.3.0", "5.4.2", "5.5.5"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.3 and newer passes validation unchanged."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    _final_validate({})


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
