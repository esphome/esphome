"""Tests for the device information the sendspin hub reports to the server."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import (
    CONF_FIRMWARE_VERSION,
    CONF_MANUFACTURER,
    CONFIG_SCHEMA,
)
from esphome.const import CONF_MODEL, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_explicit_device_info_wins_over_project(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Configured values take precedence over the project information."""
    main_cpp = generate_main(component_config_path("device_info_explicit.yaml"))

    assert 'set_manufacturer("Explicit Manufacturer")' in main_cpp
    assert 'set_model("Explicit Model")' in main_cpp
    assert 'set_firmware_version("1.2.3")' in main_cpp


def test_project_supplies_device_info(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without configured values, the project name splits into manufacturer and model."""
    main_cpp = generate_main(component_config_path("device_info_project.yaml"))

    assert 'set_manufacturer("project_manufacturer")' in main_cpp
    assert 'set_model("project_model")' in main_cpp
    assert 'set_firmware_version("9.9.9")' in main_cpp


def test_no_device_info_leaves_hub_defaults(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """With neither source, nothing is emitted and the hub keeps its own defaults."""
    main_cpp = generate_main(component_config_path("device_info_default.yaml"))

    assert "set_manufacturer(" not in main_cpp
    assert "set_model(" not in main_cpp
    assert "set_firmware_version(" not in main_cpp


@pytest.mark.parametrize(
    "conf_key", [CONF_MANUFACTURER, CONF_MODEL, CONF_FIRMWARE_VERSION]
)
def test_empty_device_info_rejected(
    set_core_config: SetCoreConfigCallable, conf_key: str
) -> None:
    """An empty string would be sent to the server as an empty value, so it is not accepted."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({conf_key: ""})
