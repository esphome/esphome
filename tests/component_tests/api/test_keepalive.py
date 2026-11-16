"""Tests for the api keepalive option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import CONFIG_SCHEMA
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}


def _defines() -> dict[str, str]:
    return {define.name: str(define.value) for define in CORE.defines}


@pytest.mark.parametrize("config_file", ["bare.yaml", "defaults.yaml"])
def test_default_keepalive_emits_no_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Omitted or explicit 60s keeps the HelloResponse field out of the build."""
    generate_main(component_config_path(config_file))

    defines = _defines()
    assert "USE_API_KEEPALIVE_INTERVAL" not in defines
    assert "API_KEEPALIVE_TIMEOUT_MS" not in defines


def test_custom_keepalive_emits_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A non default keepalive enables the HelloResponse field and sets the interval."""
    generate_main(component_config_path("keepalive.yaml"))

    defines = _defines()
    assert "USE_API_KEEPALIVE_INTERVAL" in defines
    assert defines["API_KEEPALIVE_TIMEOUT_MS"] == "300000"


@pytest.mark.parametrize("keepalive", ["10s", "5min", "1h"])
def test_keepalive_bounds_accepted(
    set_core_config: SetCoreConfigCallable, keepalive: str
) -> None:
    """The bounds themselves and values in between are valid."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    CONFIG_SCHEMA({"keepalive": keepalive})


@pytest.mark.parametrize(
    ("keepalive", "match"),
    [
        ("9s", "at least"),
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
