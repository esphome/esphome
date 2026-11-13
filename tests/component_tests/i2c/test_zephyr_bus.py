"""The Zephyr I2C bus picks the TWIM instance the board leaves free and routes it to the configured pins."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_OVERLAY
from esphome.config import read_config
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config_file", "label", "enabled_here"),
    [
        pytest.param("zephyr_nrf52840.yaml", "i2c0", False, id="nrf52840"),
        pytest.param("zephyr_xiao_ble.yaml", "i2c1", False, id="xiao_ble"),
        # The nRF54L boards leave every TWIM disabled, and TWIM20 is the console UART
        pytest.param("zephyr_nrf54l15dk.yaml", "i2c21", True, id="nrf54l15dk"),
        pytest.param("zephyr_nrf54lm20dk.yaml", "i2c21", True, id="nrf54lm20dk"),
    ],
)
def test_zephyr_i2c_instance(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    label: str,
    enabled_here: bool,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    overlay = zephyr_data()[KEY_OVERLAY][""]
    assert f"DEVICE_DT_GET(DT_NODELABEL({label}))" in main_cpp
    assert "<NRF_PSEL(TWIM_SDA, 1, 9)>" in overlay
    assert "<NRF_PSEL(TWIM_SCL, 1, 8)>" in overlay
    assert f"{label}_default: {label}_default {{" in overlay
    assert (f"pinctrl-0 = <&{label}_default>;" in overlay) is enabled_here


@pytest.mark.parametrize(
    ("config_file", "error"),
    [
        # Port 3 shares the bus's power domain on the nRF54LM20, port 0 does not
        pytest.param(
            "zephyr_nrf54lm20dk_port0.yaml",
            "P0.04 is not in the power domain of the I2C bus; use a pin on P1 or P3",
            id="port0",
        ),
        # Both ports are in the domain, but a peripheral's pins must share one port
        pytest.param(
            "zephyr_nrf54lm20dk_mixed_ports.yaml",
            "SDA P1.09 and SCL P3.01 must be on the same GPIO port",
            id="mixed_p1_p3",
        ),
        # The nRF54L15 has no port 3; P0 and P2 exist but are outside the bus's power domain
        pytest.param(
            "zephyr_nrf54l15dk_port2.yaml",
            "P2.01 is not in the power domain of the I2C bus; use a pin on P1",
            id="nrf54l15_port2",
        ),
        pytest.param(
            "zephyr_nrf54l15dk_port0.yaml",
            "P0.04 is not in the power domain of the I2C bus; use a pin on P1",
            id="nrf54l15_port0",
        ),
    ],
)
def test_zephyr_i2c_rejects_pins(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    config_file: str,
    error: str,
) -> None:
    CORE.config_path = component_config_path(config_file)
    assert read_config({}) is None
    assert error in capsys.readouterr().out
