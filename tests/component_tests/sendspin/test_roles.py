"""Tests for the roles the sendspin hub enables on request.

A role is compiled into ESPHome (USE_SENDSPIN_* define) and left enabled in the
sendspin-cpp library (CONFIG_SENDSPIN_ENABLE_* sdkconfig option) only when a
component requests it. A `test*.yaml` cannot cover the request itself, since no
in-tree component requests the color or visualizer roles.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.components.sendspin import (
    request_color_support,
    request_visualizer_support,
)
from esphome.core import CORE

ROLES = pytest.mark.parametrize(
    ("request_support", "define", "sdkconfig_option"),
    [
        (
            request_color_support,
            "USE_SENDSPIN_COLOR",
            "CONFIG_SENDSPIN_ENABLE_COLOR",
        ),
        (
            request_visualizer_support,
            "USE_SENDSPIN_VISUALIZER",
            "CONFIG_SENDSPIN_ENABLE_VISUALIZER",
        ),
    ],
    ids=["color", "visualizer"],
)


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def _sdkconfig() -> dict[str, object]:
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]


@ROLES
def test_role_disabled_unless_requested(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    request_support: Callable[[], None],
    define: str,
    sdkconfig_option: str,
) -> None:
    """Without a request the role is neither compiled in nor built in the library."""
    generate_main(component_config_path("hub_only.yaml"))

    assert define not in _defines()
    assert _sdkconfig()[sdkconfig_option] is False


@ROLES
def test_role_enabled_on_request(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    request_support: Callable[[], None],
    define: str,
    sdkconfig_option: str,
) -> None:
    """A request compiles the role in and leaves the library's default, enabled, alone."""
    request_support()
    generate_main(component_config_path("hub_only.yaml"))

    assert define in _defines()
    assert sdkconfig_option not in _sdkconfig()
