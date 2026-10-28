"""Tests for the socket component's idle timeout check against the main loop interval."""

import pytest

from esphome.components import socket
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_TIMEOUT
from esphome.core.config import CONF_LOOP_INTERVAL
import esphome.final_validate as fv
from esphome.types import ConfigType


def _validate(timeout: str, loop_interval: str | None = None) -> ConfigType:
    esphome_config = {}
    if loop_interval is not None:
        esphome_config[CONF_LOOP_INTERVAL] = cv.positive_time_period_milliseconds(
            loop_interval
        )
    token = fv.full_config.set({CONF_ESPHOME: esphome_config})
    try:
        return socket.final_validate_idle_timeout(
            {CONF_TIMEOUT: cv.positive_time_period_milliseconds(timeout)}
        )
    finally:
        fv.full_config.reset(token)


def test_zero_turns_the_timeout_off() -> None:
    assert _validate("0s")[CONF_TIMEOUT].total_milliseconds == 0


@pytest.mark.parametrize("timeout", ["16ms", "30s"])
def test_accepts_at_least_the_default_loop_interval(timeout: str) -> None:
    """16 ms is the main loop interval when loop_interval is not set."""
    _validate(timeout)


def test_rejects_less_than_the_default_loop_interval() -> None:
    with pytest.raises(
        cv.Invalid, match=r"timeout of 15ms .*\(loop_interval of 16ms\)"
    ) as err:
        _validate("15ms")
    assert err.value.path == [CONF_TIMEOUT]


def test_follows_a_custom_loop_interval() -> None:
    _validate("100ms", loop_interval="100ms")
    _validate("0s", loop_interval="100ms")
    with pytest.raises(
        cv.Invalid, match=r"timeout of 99ms .*\(loop_interval of 100ms\)"
    ):
        _validate("99ms", loop_interval="100ms")
