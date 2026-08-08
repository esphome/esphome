"""Tests for the wake button's wakeup_pin requirement in ld6002b."""

from __future__ import annotations

import pytest

from esphome.components.ld6002b.button import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WAKEUP_PIN, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "ld6002b_hub"


def _full_config(hub: ConfigType) -> Config:
    """A full config carrying one ld6002b hub, as the ID pass leaves it.

    final_validate resolves the hub through get_path_for_id, so the declaring
    path has to be registered the way validate_config registers it: the path of
    the id value itself, whose parent is the hub's own config.
    """
    full = Config()
    full["ld6002b"] = [hub]
    full.declare_ids.append((hub[CONF_ID], ["ld6002b", 0, CONF_ID]))
    return full


def _hub(*, wakeup_pin: bool) -> ConfigType:
    hub: ConfigType = {CONF_ID: ID(HUB_ID, is_declaration=True, type="ld6002b")}
    if wakeup_pin:
        hub[CONF_WAKEUP_PIN] = {"number": 4}
    return hub


def _buttons(**buttons: str) -> ConfigType:
    """A button platform config naming the given buttons on the shared hub."""
    config: ConfigType = {
        "ld6002b_id": ID(HUB_ID, is_declaration=False, type="ld6002b")
    }
    config.update({key: {"name": name} for key, name in buttons.items()})
    return config


def _validated(config: ConfigType) -> ConfigType:
    """Run the button schema, then the final validation the hub is checked in."""
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


def test_wake_without_wakeup_pin_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The wake button drives the pin directly, so a hub without one cannot serve it."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_hub(wakeup_pin=False))
    )

    with pytest.raises(cv.Invalid, match="wake requires wakeup_pin"):
        _validated(_buttons(wake="Wake"))


def test_wake_with_wakeup_pin_passes(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_hub(wakeup_pin=True))
    )

    _validated(_buttons(wake="Wake"))


def test_other_buttons_do_not_need_the_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Only wake drives the pin; the query buttons stay usable without one."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_hub(wakeup_pin=False))
    )

    _validated(_buttons(get_delay="Get Delay"))
