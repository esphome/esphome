"""Tests for the ble switch platform."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.ble import DOMAIN
from esphome.components.ble.switch import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

GenerateMain = Callable[[str | Path], str]
ConfigPath = Callable[[str], Path]


def test_switch_codegen(
    generate_main: GenerateMain, component_config_path: ConfigPath
) -> None:
    """The switch is handed to the hub, which restores and publishes it."""
    main_cpp = generate_main(component_config_path("switch_default.yaml"))

    assert "->set_enabled_switch(ble_switch);" in main_cpp
    assert "->set_enabled(state);" in main_cpp
    # The hub publishes, so the apply body does not
    assert not re.search(
        r"->set_enabled\(state\);\s*entity->publish_state\(state\);", main_cpp
    )
    assert "ble_switch->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON);" in (
        main_cpp
    )
    assert "// category:config, icon:mdi:bluetooth" in main_cpp
    assert any(d.name == "USE_ESP32_BLE_SWITCH" for d in CORE.defines)


def test_restore_mode_override(
    generate_main: GenerateMain, component_config_path: ConfigPath
) -> None:
    """An explicit restore_mode replaces the default."""
    main_cpp = generate_main(component_config_path("switch_restore_always_off.yaml"))

    assert "ble_switch->set_restore_mode(switch_::SWITCH_ALWAYS_OFF);" in (main_cpp)
    assert "SWITCH_RESTORE_DEFAULT_ON" not in main_cpp


def test_inverted_rejected() -> None:
    """The hub publishes the real state, so an inverted switch makes no sense."""
    with pytest.raises(cv.Invalid, match="inverted"):
        CONFIG_SCHEMA(
            {
                "type": "enabled",
                "id": "ble_switch",
                "name": "Bluetooth",
                "inverted": True,
            }
        )


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({"name": "Bluetooth"}, "type not specified"),
        ({"type": "bogus", "name": "Bluetooth"}, "Unknown value 'bogus'"),
    ],
)
def test_type_validation(config: dict[str, str], message: str) -> None:
    """The type key is required and must name a known switch type."""
    with pytest.raises(cv.Invalid, match=message):
        CONFIG_SCHEMA(config)


def test_duplicate_switch_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """The hub keeps one switch pointer, so a second switch is an error."""
    entry = {"platform": DOMAIN, "type": "enabled", "name": "Bluetooth"}
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"esp32_ble": {}, "switch": [entry, dict(entry)]},
    )

    with pytest.raises(cv.Invalid, match="Only one ble 'enabled' switch is allowed"):
        FINAL_VALIDATE_SCHEMA(entry)


def test_single_switch_accepted(set_core_config: SetCoreConfigCallable) -> None:
    """A lone switch passes final validation."""
    entry = {"platform": DOMAIN, "type": "enabled", "name": "Bluetooth"}
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config={"esp32_ble": {}, "switch": [entry]}
    )

    assert FINAL_VALIDATE_SCHEMA(entry) is entry
