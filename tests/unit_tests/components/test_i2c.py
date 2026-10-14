"""I2C backend source selection."""

from unittest.mock import patch

import pytest

from esphome.components import i2c
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM


@pytest.mark.parametrize(
    "build_os, host_bus_built",
    [
        ("linux", True),
        ("darwin", False),
    ],
)
def test_host_bus_is_only_built_on_linux(build_os: str, host_bus_built: bool) -> None:
    core_data = {KEY_CORE: {KEY_TARGET_PLATFORM: "host", KEY_TARGET_FRAMEWORK: "host"}}
    with (
        patch("esphome.config_helpers.CORE.data", core_data),
        patch.object(i2c.sys, "platform", build_os),
    ):
        excluded = i2c.FILTER_SOURCE_FILES()
    assert ("i2c_bus_host.cpp" not in excluded) is host_bus_built
    assert {"i2c_bus_arduino.cpp", "i2c_bus_esp_idf.cpp", "i2c_bus_zephyr.cpp"} <= set(
        excluded
    )
