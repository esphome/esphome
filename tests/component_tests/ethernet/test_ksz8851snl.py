"""Tests for the KSZ8851SNL SPI ethernet type."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.esp32.const import (
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_REF,
    KEY_SDKCONFIG_OPTIONS,
)
from esphome.core import CORE

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
