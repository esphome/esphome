"""Tests for the esp32_hosted ESP-IDF version gate and esp_hosted line selection."""

import pytest

from esphome import config_validation as cv
from esphome.components import esp32_hosted
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_hosted import (
    CONF_ACTIVE_HIGH,
    CONF_BUS_WIDTH,
    _final_validate,
    user_esp_hosted_major,
    uses_esp_hosted_3x,
)
from esphome.const import (
    CONF_COMPONENTS,
    CONF_FRAMEWORK,
    CONF_NAME,
    CONF_REF,
    CONF_SOURCE,
    CONF_TYPE,
    KEY_ESP32,
    PlatformFramework,
)

from ..types import SetCoreConfigCallable


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


SDIO_4BIT = {CONF_TYPE: "sdio", CONF_BUS_WIDTH: 4, CONF_ACTIVE_HIGH: True}
SDIO_1BIT = {CONF_TYPE: "sdio", CONF_BUS_WIDTH: 1, CONF_ACTIVE_HIGH: True}
SPI = {CONF_TYPE: "spi", CONF_ACTIVE_HIGH: True}
SPI_ACTIVE_LOW = {CONF_TYPE: "spi", CONF_ACTIVE_HIGH: False}


@pytest.mark.parametrize(
    ("idf", "config", "expected"),
    [
        ("5.3.0", SDIO_4BIT, False),
        ("5.4.2", SPI, False),
        ("5.5.0", SDIO_4BIT, True),
        ("5.5.5", SPI, True),
        ("5.5.5", SDIO_1BIT, False),
        ("5.5.5", SPI_ACTIVE_LOW, False),
    ],
)
def test_uses_esp_hosted_3x(
    set_core_config: SetCoreConfigCallable,
    monkeypatch: pytest.MonkeyPatch,
    idf: str,
    config: dict,
    expected: bool,
) -> None:
    """3.x needs ESP-IDF 5.5, a 4-bit SDIO or SPI bus, and an active-high reset."""
    monkeypatch.setattr(esp32_hosted, "ESP_HOSTED_ENABLE_3X", True)
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    assert uses_esp_hosted_3x(config) is expected


@pytest.mark.parametrize("config", [SDIO_4BIT, SDIO_1BIT, SPI, SPI_ACTIVE_LOW])
def test_uses_esp_hosted_3x_disabled_by_default(
    set_core_config: SetCoreConfigCallable, config: dict
) -> None:
    """Every configuration stays on 2.x while the 3.x line is not enabled."""
    assert esp32_hosted.ESP_HOSTED_ENABLE_3X is False
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse("5.5.5")},
    )
    assert uses_esp_hosted_3x(config) is False


def _pinned(ref: str, source: str | None = None) -> dict:
    """A full config with esp_hosted pinned under esp32.framework.components."""
    component = {CONF_NAME: "espressif/esp_hosted", CONF_REF: ref}
    if source is not None:
        component[CONF_SOURCE] = source
    return {KEY_ESP32: {CONF_FRAMEWORK: {CONF_COMPONENTS: [component]}}}


@pytest.mark.parametrize(
    ("full_config", "expected"),
    [
        ({}, None),
        ({KEY_ESP32: {CONF_FRAMEWORK: {CONF_COMPONENTS: []}}}, None),
        (_pinned("3.0.9"), 3),
        (_pinned("==3.0.9"), 3),
        (_pinned("^3"), 3),
        (_pinned("2.12.13"), 2),
        (_pinned("~2.12"), 2),
        (_pinned("main", source="https://github.com/espressif/esp-hosted-mcu"), None),
    ],
)
def test_user_esp_hosted_major(
    set_core_config: SetCoreConfigCallable, full_config: dict, expected: int | None
) -> None:
    """A registry version pin decides the line; git sources and no pin do not."""
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)
    assert user_esp_hosted_major() == expected


@pytest.mark.parametrize(
    ("ref", "config", "expected"),
    [
        ("3.0.9", SDIO_4BIT, True),
        ("3.0.9", SDIO_1BIT, True),
        ("2.12.13", SDIO_4BIT, False),
    ],
)
def test_user_pin_selects_line(
    set_core_config: SetCoreConfigCallable,
    monkeypatch: pytest.MonkeyPatch,
    ref: str,
    config: dict,
    expected: bool,
) -> None:
    """A user pin wins over the default line and the bus heuristics."""
    monkeypatch.setattr(esp32_hosted, "ESP_HOSTED_ENABLE_3X", not expected)
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse("5.5.5")},
        full_config=_pinned(ref),
    )
    assert uses_esp_hosted_3x(config) is expected


@pytest.mark.parametrize("config", [SDIO_4BIT, SPI])
def test_final_validate_accepts_3x_pin(
    set_core_config: SetCoreConfigCallable, config: dict
) -> None:
    """A 3.x pin passes validation on ESP-IDF 5.5 with a supported bus."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse("5.5.5")},
        full_config=_pinned("3.0.9"),
    )
    _final_validate(config)


@pytest.mark.parametrize(
    ("idf", "config", "match"),
    [
        ("5.4.2", SDIO_4BIT, "requires ESP-IDF 5.5 or newer"),
        ("5.5.5", SDIO_1BIT, "1-bit SDIO"),
        ("5.5.5", SPI_ACTIVE_LOW, "active_high: false"),
    ],
)
def test_final_validate_rejects_unsupported_3x_pin(
    set_core_config: SetCoreConfigCallable, idf: str, config: dict, match: str
) -> None:
    """A 3.x pin is rejected where the 3.x line cannot build the configuration."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
        full_config=_pinned("3.0.9"),
    )
    with pytest.raises(cv.Invalid, match=match):
        _final_validate(config)
