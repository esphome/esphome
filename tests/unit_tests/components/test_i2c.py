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


@pytest.mark.parametrize("build_os", ["linux", "darwin"])
def test_nothing_excluded_before_the_target_is_known(build_os: str) -> None:
    with (
        patch("esphome.config_helpers.CORE.data", {}),
        patch.object(i2c.sys, "platform", build_os),
    ):
        assert i2c.FILTER_SOURCE_FILES() == []


@pytest.mark.parametrize("build_os", ["linux", "darwin"])
def test_host_bus_left_out_for_other_targets(build_os: str) -> None:
    core_data = {
        KEY_CORE: {KEY_TARGET_PLATFORM: "esp32", KEY_TARGET_FRAMEWORK: "esp-idf"}
    }
    with (
        patch("esphome.config_helpers.CORE.data", core_data),
        patch.object(i2c.sys, "platform", build_os),
    ):
        excluded = i2c.FILTER_SOURCE_FILES()
    assert excluded.count("i2c_bus_host.cpp") == 1
    assert "i2c_bus_esp_idf.cpp" not in excluded
