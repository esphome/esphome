"""Tests for the uart_tcp timeout against the main loop interval."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart_tcp
from esphome.components.const import CONF_HOST
from esphome.config import Config
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_PORT,
    CONF_TIMEOUT,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import ID
from esphome.core.config import CONF_LOOP_INTERVAL
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validate(
    set_core_config: SetCoreConfigCallable,
    timeout: str,
    loop_interval: str | None = None,
) -> ConfigType:
    esphome_conf = {}
    if loop_interval is not None:
        esphome_conf[CONF_LOOP_INTERVAL] = cv.positive_time_period_milliseconds(
            loop_interval
        )
    full = Config()
    full[CONF_ESPHOME] = esphome_conf
    full["uart"] = [{CONF_ID: ID("bus")}]
    full.declare_ids.append((full["uart"][0][CONF_ID], ["uart", 0, CONF_ID]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    config = uart_tcp.CONFIG_SCHEMA(
        {
            CONF_UART_ID: "bus",
            CONF_HOST: "192.0.2.10",
            CONF_PORT: 502,
            CONF_TIMEOUT: timeout,
        }
    )
    return uart_tcp.FINAL_VALIDATE_SCHEMA(config)


def test_zero_turns_the_timeout_off(set_core_config: SetCoreConfigCallable) -> None:
    assert _validate(set_core_config, "0s")[CONF_TIMEOUT].total_milliseconds == 0


@pytest.mark.parametrize("timeout", ["16ms", "30s"])
def test_accepts_at_least_the_default_loop_interval(
    set_core_config: SetCoreConfigCallable, timeout: str
) -> None:
    _validate(set_core_config, timeout)


def test_rejects_less_than_the_default_loop_interval(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(
        cv.Invalid, match=r"timeout of 15ms .*\(loop_interval of 16ms\)"
    ) as err:
        _validate(set_core_config, "15ms")
    assert err.value.path == [CONF_TIMEOUT]


def test_follows_a_custom_loop_interval(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _validate(set_core_config, "100ms", loop_interval="100ms")
    _validate(set_core_config, "0s", loop_interval="100ms")
    with pytest.raises(
        cv.Invalid, match=r"timeout of 99ms .*\(loop_interval of 100ms\)"
    ):
        _validate(set_core_config, "99ms", loop_interval="100ms")
