"""Tests for the api keepalive option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import CONFIG_SCHEMA
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.helpers import get_define_value
from tests.component_tests.types import SetCoreConfigCallable

ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}


@pytest.mark.parametrize(
    ("config_file", "advertised", "timeout_ms"),
    [
        ("bare.yaml", False, "60000"),
        ("defaults.yaml", False, "60000"),
        ("keepalive.yaml", True, "300000"),
    ],
)
def test_keepalive_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    advertised: bool,
    timeout_ms: str,
) -> None:
    """The interval is always emitted; only a non default one enables the HelloResponse field."""
    generate_main(component_config_path(config_file))

    assert get_define_value("API_KEEPALIVE_TIMEOUT_MS") == timeout_ms
    defines = {define.name for define in CORE.defines}
    assert ("USE_API_KEEPALIVE_INTERVAL" in defines) is advertised


@pytest.mark.parametrize("keepalive", ["60s", "5min", "1h"])
def test_keepalive_bounds_accepted(
    set_core_config: SetCoreConfigCallable, keepalive: str
) -> None:
    """The bounds themselves and values in between are valid."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    CONFIG_SCHEMA({"keepalive": keepalive})


@pytest.mark.parametrize(
    ("keepalive", "match"),
    [
        ("59s", "at least"),
        ("61min", "at most"),
        ("90500ms", "Maximum precision is seconds"),
    ],
)
def test_keepalive_invalid_rejected(
    set_core_config: SetCoreConfigCallable, keepalive: str, match: str
) -> None:
    """Out of range values and sub-second precision are rejected."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match=match):
        CONFIG_SCHEMA({"keepalive": keepalive})
