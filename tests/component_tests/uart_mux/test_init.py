"""Tests for the uart_mux component's final validation."""

import pytest

from esphome import config_validation as cv
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

CONF_BRIDGE_ID = "bridge_id"


def _set_esp32_s3(set_core_config: SetCoreConfigCallable) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32S3

    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32S3}
    )


def _mux_conf(mux_id: str, bridge_id: str) -> ConfigType:
    return {CONF_ID: ID(mux_id), CONF_BRIDGE_ID: ID(bridge_id)}


def test_accepts_one_mux_per_bridge(set_core_config: SetCoreConfigCallable) -> None:
    _set_esp32_s3(set_core_config)
    from esphome.components import uart_mux

    uart_mux._final_validate(_mux_conf("mux_0", "bridge_0"))
    uart_mux._final_validate(_mux_conf("mux_1", "bridge_1"))


def test_rejects_two_muxes_on_one_bridge(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config)
    from esphome.components import uart_mux

    uart_mux._final_validate(_mux_conf("mux_0", "bridge_0"))
    with pytest.raises(cv.Invalid, match="already routed by another 'uart_mux'"):
        uart_mux._final_validate(_mux_conf("mux_1", "bridge_0"))
