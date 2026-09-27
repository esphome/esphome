"""Tests for the ethernet `spi_id:` option (attach to a shared spi bus)."""

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
from esphome.components.ethernet import CONF_INTERFACE, CONFIG_SCHEMA, _final_validate
from esphome.components.rp2.const import KEY_BOARD as RP2_KEY_BOARD

# Registers the rp2 pin schema so RP2 configs can validate pins.
import esphome.components.rp2.gpio  # noqa: F401
from esphome.components.spi import CONF_INTERFACE_INDEX
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_SPI,
    CONF_SPI_ID,
    CONF_TYPE,
    PlatformFramework,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv

from ..types import SetCoreConfigCallable

_W5500_PIN_CONFIG = {
    "type": "W5500",
    "clk_pin": 47,
    "mosi_pin": 48,
    "miso_pin": 14,
    "cs_pin": 21,
}

_W5500_SPI_ID_CONFIG = {
    "type": "W5500",
    "spi_id": "spi_bus",
    "cs_pin": 21,
}


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
    CORE.name = "spi-id-test"


def test_spi_id_accepted_without_pins_or_interface(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """With spi_id set, the pin options are not required and no interface is defaulted."""
    _set_esp32_s3(set_core_config)
    config = CONFIG_SCHEMA(dict(_W5500_SPI_ID_CONFIG))
    assert config[CONF_SPI_ID] == ID("spi_bus")
    # The interface comes from the referenced bus; no default may be injected.
    assert CONF_INTERFACE not in config


@pytest.mark.parametrize(
    ("key", "value"),
    [
        (CONF_CLK_PIN, 47),
        (CONF_MOSI_PIN, 48),
        (CONF_MISO_PIN, 14),
        (CONF_INTERFACE, "spi2"),
    ],
)
def test_spi_id_rejects_bus_options(
    set_core_config: SetCoreConfigCallable, key: str, value: int | str
) -> None:
    """Options provided by the referenced bus must be rejected alongside spi_id."""
    _set_esp32_s3(set_core_config)
    with pytest.raises(Invalid, match=f"'{key}' cannot be used together with 'spi_id'"):
        CONFIG_SCHEMA({**_W5500_SPI_ID_CONFIG, key: value})


@pytest.mark.parametrize("key", [CONF_CLK_PIN, CONF_MOSI_PIN, CONF_MISO_PIN])
def test_bus_pins_still_required_without_spi_id(
    set_core_config: SetCoreConfigCallable, key: str
) -> None:
    """Without spi_id, the bus pin options stay required."""
    _set_esp32_s3(set_core_config)
    config = {k: v for k, v in _W5500_PIN_CONFIG.items() if k != key}
    with pytest.raises(
        Invalid, match=f"'{key}' is a required option when 'spi_id' is not set"
    ):
        CONFIG_SCHEMA(config)


def test_spi_id_rejected_on_rp2(set_core_config: SetCoreConfigCallable) -> None:
    """spi_id is ESP32-only; the RP2 path is unchanged."""
    set_core_config(
        PlatformFramework.RP2_ARDUINO, platform_data={RP2_KEY_BOARD: "rpipicow"}
    )
    CORE.name = "spi-id-test"
    config = {
        "type": "W5500",
        "spi_id": "spi_bus",
        "clk_pin": 18,
        "mosi_pin": 19,
        "miso_pin": 16,
        "cs_pin": 17,
    }
    with pytest.raises(Invalid, match="only available on"):
        CONFIG_SCHEMA(config)


def _eth_spi_id_final_config() -> dict:
    return {CONF_TYPE: "W5500", CONF_SPI_ID: ID("spi_bus")}


class _FakeFinalConfig(dict):
    """Dict-backed FinalValidateConfig with just enough ID resolution for
    fv.id_declaration_match_schema to find an spi bus fragment."""

    def get_path_for_id(self, id: ID) -> list:
        for index, conf in enumerate(self[CONF_SPI]):
            if conf[CONF_ID] == id:
                return [CONF_SPI, index, CONF_ID]
        raise KeyError(id)

    def get_config_for_path(self, path: list) -> dict:
        return self[path[0]][path[1]]


def _set_spi_buses(*buses: dict) -> None:
    fv.full_config.set(_FakeFinalConfig({CONF_SPI: list(buses)}))


_SHAREABLE_BUS = {
    CONF_ID: ID("spi_bus"),
    CONF_INTERFACE_INDEX: 0,
    CONF_MISO_PIN: {},
    CONF_MOSI_PIN: {},
}


def test_final_validate_accepts_hardware_bus_with_data_pins(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A hardware spi bus that declares miso_pin and mosi_pin may be shared."""
    _set_esp32_s3(set_core_config)
    # An unrelated bus first: the ID lookup must skip past it.
    _set_spi_buses({CONF_ID: ID("other_bus"), CONF_INTERFACE_INDEX: 1}, _SHAREABLE_BUS)
    _final_validate(_eth_spi_id_final_config())


def test_final_validate_rejects_software_bus(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A software spi bus (no hardware interface index) cannot be shared."""
    _set_esp32_s3(set_core_config)
    bus = {k: v for k, v in _SHAREABLE_BUS.items() if k != CONF_INTERFACE_INDEX}
    _set_spi_buses(bus)
    with pytest.raises(Invalid, match="requires this spi bus to use a hardware"):
        _final_validate(_eth_spi_id_final_config())


@pytest.mark.parametrize("pin_key", [CONF_MISO_PIN, CONF_MOSI_PIN])
def test_final_validate_rejects_bus_without_data_pin(
    set_core_config: SetCoreConfigCallable, pin_key: str
) -> None:
    """The shared bus must declare both data pins to drive the ethernet chip."""
    _set_esp32_s3(set_core_config)
    bus = {k: v for k, v in _SHAREABLE_BUS.items() if k != pin_key}
    _set_spi_buses(bus)
    with pytest.raises(Invalid, match=f"requires this spi bus to declare a {pin_key}"):
        _final_validate(_eth_spi_id_final_config())


def test_final_validate_rejects_colliding_host_without_spi_id(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Without spi_id, claiming the same host as an spi bus stays an error."""
    _set_esp32_s3(set_core_config)
    fv.full_config.set({CONF_SPI: [{CONF_ID: ID("spi_bus"), CONF_INTERFACE_INDEX: 0}]})
    config = {CONF_TYPE: "W5500", CONF_INTERFACE: "spi2"}
    with pytest.raises(Invalid, match="both using interface 'SPI2_HOST'"):
        _final_validate(config)


def test_final_validate_accepts_distinct_host_without_spi_id(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Without spi_id, a different host than the spi bus is accepted."""
    _set_esp32_s3(set_core_config)
    fv.full_config.set({CONF_SPI: [{CONF_ID: ID("spi_bus"), CONF_INTERFACE_INDEX: 0}]})
    _final_validate({CONF_TYPE: "W5500", CONF_INTERFACE: "spi3"})


def test_generated_code_uses_spi_parent(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """With spi_id, codegen wires the spi parent and skips the bus options."""
    main_cpp = generate_main(component_config_path("spi_id_shared_bus.yaml"))

    assert "eth_component->set_spi_parent(spi_bus);" in main_cpp
    assert "eth_component->set_cs_pin(5);" in main_cpp
    assert "eth_component->set_clk_pin(" not in main_cpp
    assert "eth_component->set_miso_pin(" not in main_cpp
    assert "eth_component->set_mosi_pin(" not in main_cpp
    assert "eth_component->set_interface(" not in main_cpp


def test_generated_code_without_spi_id_initializes_own_bus(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without spi_id, codegen still emits the pin and interface setters."""
    main_cpp = generate_main(component_config_path("spi_own_bus.yaml"))

    assert "eth_component->set_spi_parent(" not in main_cpp
    assert "eth_component->set_clk_pin(18);" in main_cpp
    assert "eth_component->set_miso_pin(19);" in main_cpp
    assert "eth_component->set_mosi_pin(23);" in main_cpp
    assert "eth_component->set_cs_pin(5);" in main_cpp
    assert "eth_component->set_interface(::SPI3_HOST);" in main_cpp
