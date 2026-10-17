"""Tests for the KSZ8851SNL SPI ethernet type."""

from collections.abc import Callable
from pathlib import Path

import pytest
from voluptuous import Invalid

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    VARIANT_ESP32S3,
)
from esphome.components.esp32.const import (
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_REF,
    KEY_SDKCONFIG_OPTIONS,
)
from esphome.components.ethernet import CONF_CLOCK_SPEED, CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.core import CORE

from ..types import SetCoreConfigCallable

_BASE_CONFIG = {
    "type": "KSZ8851SNL",
    "clk_pin": 47,
    "mosi_pin": 48,
    "miso_pin": 14,
    "cs_pin": 21,
}

_SDKCONFIG_OPTION = "CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL"
_IDF_COMPONENT = "espressif/ksz8851snl"


def _sdkconfig() -> dict[str, object]:
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]


def _idf_components() -> dict[str, dict[str, str | None]]:
    return CORE.data[KEY_ESP32][KEY_COMPONENTS]


def test_codegen_on_idf5_wires_the_spi_driver(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """On IDF 5.x the driver is built into esp_eth and enabled by its Kconfig option."""
    main_cpp = generate_main(component_config_path("ksz8851snl_idf5.yaml"))

    assert "eth_component->set_type(ethernet::ETHERNET_TYPE_KSZ8851SNL);" in main_cpp
    assert "eth_component->set_cs_pin(5);" in main_cpp
    assert "eth_component->set_interrupt_pin(36);" in main_cpp
    assert "USE_ETHERNET_KSZ8851SNL" in {d.name for d in CORE.defines}
    assert _sdkconfig()[_SDKCONFIG_OPTION] is True
    # Built into IDF 5.x, so the registry component must not be pulled in.
    assert _IDF_COMPONENT not in _idf_components()


def test_codegen_on_idf6_pulls_the_registry_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """IDF 6.0 dropped the built-in driver, so the managed component is required."""
    generate_main(component_config_path("ksz8851snl_idf6.yaml"))

    assert _idf_components()[_IDF_COMPONENT][KEY_REF] == "1.2.0"
    # The Kconfig option no longer exists on IDF 6.0.
    assert _SDKCONFIG_OPTION not in _sdkconfig()


def _set_esp32_s3(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    # _validate derives use_address from the node name, which has no default here.
    CORE.name = "ksz8851snl-test"


@pytest.mark.parametrize("clock_speed", ["26.67MHz", "40MHz"])
def test_accepts_clock_speed_up_to_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable, clock_speed: str
) -> None:
    """The datasheet rates fSCLK to 40MHz, so the whole range must be accepted."""
    _set_esp32_s3(set_core_config)
    config = CONFIG_SCHEMA({**_BASE_CONFIG, CONF_CLOCK_SPEED: clock_speed})
    assert config[CONF_CLOCK_SPEED] == cv.frequency(clock_speed)


def test_rejects_clock_speed_above_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The shared 80MHz ceiling is out of spec for this part."""
    _set_esp32_s3(set_core_config)
    with pytest.raises(Invalid, match="value must be at most 40000000"):
        CONFIG_SCHEMA({**_BASE_CONFIG, CONF_CLOCK_SPEED: "80MHz"})
