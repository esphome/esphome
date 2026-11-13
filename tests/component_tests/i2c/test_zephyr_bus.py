"""The Zephyr I2C bus picks the TWIM instance the board leaves free and routes it to the configured pins."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_OVERLAY


@pytest.mark.parametrize(
    ("config_file", "label", "enabled_here"),
    [
        pytest.param("zephyr_nrf52840.yaml", "i2c0", False, id="nrf52840"),
        pytest.param("zephyr_xiao_ble.yaml", "i2c1", False, id="xiao_ble"),
        # The nRF54L boards leave every TWIM disabled, and TWIM20 is the console UART
        pytest.param("zephyr_nrf54l15dk.yaml", "i2c21", True, id="nrf54l15dk"),
        pytest.param("zephyr_nrf54lm20dk.yaml", "i2c21", True, id="nrf54lm20dk"),
        pytest.param(
            "zephyr_xiao_nrf54lm20a.yaml", "i2c21", True, id="xiao_nrf54lm20a"
        ),
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
