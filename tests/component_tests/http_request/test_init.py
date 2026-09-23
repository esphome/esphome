"""Tests for the http_request watchdog timeout default."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.const import CONF_WATCHDOG_TIMEOUT
from esphome.core import CORE, TimePeriodMilliseconds


@pytest.mark.parametrize(
    ("yaml_file", "expected_ms"),
    [
        # stock 4.5s timeout: 3 x 4.5s plus 1s margin
        ("test_esp32_stock.yaml", 14500),
        # 3 x 10s plus 1s margin
        ("test_esp32_default.yaml", 31000),
        # esp32.watchdog_timeout: 60s is wider than the derived value and wins
        ("test_esp32_platform_wider.yaml", 60000),
        # explicit value is kept as is
        ("test_esp32_explicit.yaml", 20000),
    ],
)
def test_esp32_watchdog_timeout(
    component_config_path: Callable[[str], Path], yaml_file: str, expected_ms: int
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    assert config["http_request"][CONF_WATCHDOG_TIMEOUT] == TimePeriodMilliseconds(
        milliseconds=expected_ms
    )


@pytest.mark.parametrize("yaml_file", ["test_esp8266.yaml", "test_rp2040.yaml"])
def test_other_platforms_leave_watchdog_unset(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    assert CONF_WATCHDOG_TIMEOUT not in config["http_request"]
