"""Tests for the STM32 Zephyr variants."""

import pytest

from esphome.components.stm32.zephyr_variants import stm32_family, stm32c5
from esphome.components.zephyr import (
    zephyr_data,
    zephyr_framework_type,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_PRJ_CONF
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_FRAMEWORK, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


@pytest.fixture(autouse=True)
def core_data() -> None:
    CORE.data[KEY_CORE] = {}


def _prj_conf() -> dict[str, bool]:
    return {
        name.removeprefix("CONFIG_"): value
        for name, (value, _) in zephyr_data()[KEY_PRJ_CONF][""].items()
    }


def test_config_schema_defaults_to_the_largest_nucleo() -> None:
    config = stm32c5.config_schema({CONF_FRAMEWORK: {}})

    assert config[CONF_BOARD] == "nucleo_c5a3zg"
    assert zephyr_data()[CONF_BOARD] == "nucleo_c5a3zg"
    assert zephyr_data()[KEY_BOOTLOADER] == ""
    assert zephyr_variant() == "STM32C5"
    assert zephyr_variant_family() == "stm32"
    assert zephyr_framework_type() == "zephyr"
    assert CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] == cv.Version(4, 5, 0)


def test_config_schema_keeps_the_board() -> None:
    config = stm32c5.config_schema({CONF_BOARD: "xiao_stm32c5", CONF_FRAMEWORK: {}})

    assert config[CONF_BOARD] == "xiao_stm32c5"
    assert zephyr_data()[CONF_BOARD] == "xiao_stm32c5"


def test_to_code() -> None:
    stm32c5.config_schema({CONF_FRAMEWORK: {}})

    stm32c5.to_code({CONF_BOARD: "nucleo_c5a3zg"})

    defines = {define.name: str(define.value) for define in CORE.defines}
    assert defines["ESPHOME_BOARD"] == '"nucleo_c5a3zg"'
    assert defines["ESPHOME_VARIANT"] == '"STM32C5"'
    assert "ESPHOME_THREAD_MULTI_ATOMICS" in defines
    assert _prj_conf()["REBOOT"] is True
    assert _prj_conf()["SETTINGS"] is True


def test_family_to_code_selects_the_full_cpp_library() -> None:
    stm32c5.config_schema({CONF_FRAMEWORK: {}})

    stm32_family.to_code({})

    assert _prj_conf()["CPP"] is True
    assert _prj_conf()["REQUIRES_FULL_LIBCPP"] is True
