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
import esphome.config_validation as cv
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
@pytest.mark.parametrize("requested", [False, True], ids=["unrequested", "requested"])
def test_role_enabled_only_on_request(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    request_support: Callable[[], None],
    define: str,
    sdkconfig_option: str,
    requested: bool,
) -> None:
    """A request compiles the role in and leaves the library's default, enabled, alone."""
    if requested:
        request_support()
    generate_main(component_config_path("device_info_default.yaml"))

    assert (define in _defines()) is requested
    assert _sdkconfig().get(sdkconfig_option, True) is requested


def test_visualizer_role_rejects_a_second_consumer() -> None:
    """The visualizer role has a single consumer, so a second request fails validation."""
    request_visualizer_support()

    with pytest.raises(
        cv.Invalid,
        match="Only one component can consume the sendspin visualizer role",
    ):
        request_visualizer_support()


def test_color_role_accepts_several_consumers() -> None:
    """The hub fans the color role out, so any number of components may request it."""
    request_color_support()
    request_color_support()
