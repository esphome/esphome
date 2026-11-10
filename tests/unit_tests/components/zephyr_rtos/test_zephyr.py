"""Tests for the zephyr_rtos store."""

from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF, KEY_ZEPHYR, KEY_ZEPHYR_RTOS
from esphome.components.zephyr_rtos import zephyr_rtos_data
from esphome.core import CORE


def test_zephyr_data_without_rtos_store() -> None:
    nrf52_store = {"board": "adafruit_feather_nrf52840"}
    CORE.data[KEY_ZEPHYR] = nrf52_store
    assert zephyr_data() is nrf52_store


def test_zephyr_data_prefers_rtos_store() -> None:
    rtos_store = {"board": "nucleo_c5a3zg"}
    CORE.data[KEY_ZEPHYR] = {"board": "adafruit_feather_nrf52840"}
    CORE.data[KEY_ZEPHYR_RTOS] = rtos_store
    assert zephyr_data() is rtos_store


def test_zephyr_helpers_write_to_rtos_store() -> None:
    nrf52_store = {KEY_PRJ_CONF: {}}
    rtos_store = {KEY_PRJ_CONF: {}}
    CORE.data[KEY_ZEPHYR] = nrf52_store
    CORE.data[KEY_ZEPHYR_RTOS] = rtos_store
    zephyr_add_prj_conf("FOO", True)
    assert rtos_store[KEY_PRJ_CONF] == {"": {"CONFIG_FOO": (True, True)}}
    assert nrf52_store[KEY_PRJ_CONF] == {}


def test_zephyr_rtos_data() -> None:
    rtos_store = {"board": "nucleo_c5a3zg"}
    CORE.data[KEY_ZEPHYR_RTOS] = rtos_store
    assert zephyr_rtos_data() is rtos_store
