"""Tests for SEGGER RTT channels as uart: ports on Zephyr."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import uart
from esphome.components.zephyr.const import ZEPHYR_VARIANT_NRF52
from esphome.config_validation import Invalid
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_NUMBER,
    CONF_PORT,
    CONF_TX_PIN,
    PlatformFramework,
)
from esphome.core import CORE, EsphomeError
from tests.component_tests.types import SetCoreConfigCallable


def _zephyr(set_core_config: SetCoreConfigCallable, variant: str) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": variant, "family": "nordic"},
    )


@pytest.mark.parametrize("port", ["rtt0", "RTT1", "Rtt2"])
def test_rtt_port_accepted(set_core_config: SetCoreConfigCallable, port: str) -> None:
    _zephyr(set_core_config, ZEPHYR_VARIANT_NRF52)
    assert uart.validate_port(port) == port.upper()


def test_rtt3_port_rejected(set_core_config: SetCoreConfigCallable) -> None:
    _zephyr(set_core_config, ZEPHYR_VARIANT_NRF52)
    with pytest.raises(Invalid, match="not a valid UART port"):
        uart.validate_port("RTT3")


def test_rtt_port_rejected_on_family_without_rtt(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": "ESP32", "family": "esp32"},
    )
    with pytest.raises(Invalid, match="esp32 does not support"):
        uart.validate_port("RTT1")


@pytest.mark.parametrize(
    "family",
    ["nordic", "stm32", "silabs", "silabs_siwx91x", "rpi_pico", "renesas", "nxp"],
)
def test_every_rtt_family_declares_it(family: str) -> None:
    from esphome.components.zephyr import _get_family_module  # noqa: PLC0415
    from esphome.components.zephyr.const import SEGGER_RTT_UART  # noqa: PLC0415

    assert SEGGER_RTT_UART in _get_family_module(family).UART_DRIVERS


def test_rtt_port_rejects_pins(set_core_config: SetCoreConfigCallable) -> None:
    _zephyr(set_core_config, ZEPHYR_VARIANT_NRF52)
    config = {CONF_PORT: "RTT1", CONF_BAUD_RATE: 115200, CONF_TX_PIN: {CONF_NUMBER: 6}}
    with pytest.raises(Invalid, match="debug probe"):
        uart.validate_zephyr_config(config)


def test_logger_and_bus_on_separate_channels(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_OVERLAY, KEY_PRJ_CONF  # noqa: PLC0415

    main_cpp = generate_main(component_config_path("rtt_logger_and_bus.yaml"))
    assert "DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtt1))" in main_cpp
    # RTT has no pins, so dump_config shows none
    assert "rtt_bus->set_tx_pin_name(" not in main_cpp
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["LOGGER_UART_NODE_LABEL"] == "rtt0"
    overlay = "".join(zephyr_data()[KEY_OVERLAY].values())
    assert 'rtt0: rtt0 { compatible = "segger,rtt-uart"; }' in overlay
    assert 'rtt1: rtt1 { compatible = "segger,rtt-uart"; }' in overlay
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    assert prj_conf["CONFIG_USE_SEGGER_RTT"] == (True, True)
    assert prj_conf["CONFIG_LOG_BACKEND_RTT"] == (False, False)


def test_logger_and_bus_on_same_channel_rejected(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    with pytest.raises(EsphomeError, match="RTT1 is used by both"):
        generate_main(component_config_path("rtt_same_channel.yaml"))
