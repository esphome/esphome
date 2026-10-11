"""Tests for the ld6002b validators that reach across platforms.

wake needs a pin on its own hub, apply_area needs a select on its own hub, and
area_config needs both a button and a select on its own hub.  Every one of them
is a same-instance check, which is the half that breaks quietly.
"""

from __future__ import annotations

import pytest

from esphome.components.ld6002b.button import (
    CONFIG_SCHEMA as BUTTON_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as BUTTON_FINAL_VALIDATE_SCHEMA,
)
from esphome.components.ld6002b.const import CONF_AREA_CONFIG, CONF_Z_MIN
from esphome.components.ld6002b.number import (
    CONFIG_SCHEMA as NUMBER_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as NUMBER_FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA_ID,
    CONF_BUTTON,
    CONF_ID,
    CONF_WAKEUP_PIN,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "ld6002b_hub"
OTHER_HUB_ID = "ld6002b_other"


def _full_config(
    hub: ConfigType,
    *,
    selects: list[ConfigType] | None = None,
    buttons: list[ConfigType] | None = None,
) -> Config:
    """A full config carrying one ld6002b hub, as the ID pass leaves it.

    final_validate resolves the hub through get_path_for_id, so the declaring
    path has to be registered the way validate_config registers it: the path of
    the id value itself, whose parent is the hub's own config.

    The platform lists are what the cross-platform validators scan, so a test can
    say which of them exist and which hub each one names.
    """
    full = Config()
    full["ld6002b"] = [hub]
    full.declare_ids.append((hub[CONF_ID], ["ld6002b", 0, CONF_ID]))
    if selects is not None:
        full["select"] = selects
    if buttons is not None:
        full[CONF_BUTTON] = buttons
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


def _select(*, hub_id: str = HUB_ID) -> ConfigType:
    """A select platform config naming area_id on the given hub."""
    return {
        "ld6002b_id": ID(hub_id, is_declaration=False, type="ld6002b"),
        CONF_AREA_ID: {"name": "Area ID"},
    }


def _area_numbers(*, hub_id: str = HUB_ID) -> ConfigType:
    """A number platform config carrying one area_config bound."""
    return {
        "ld6002b_id": ID(hub_id, is_declaration=False, type="ld6002b"),
        CONF_AREA_CONFIG: {CONF_Z_MIN: {"name": "Area Z Min"}},
    }


def _validated(config: ConfigType) -> ConfigType:
    """Run the button schema, then the final validation the hub is checked in."""
    config = BUTTON_CONFIG_SCHEMA(config)
    BUTTON_FINAL_VALIDATE_SCHEMA(config)
    return config


def _validated_numbers(config: ConfigType) -> ConfigType:
    """The same two passes for the number platform."""
    config = NUMBER_CONFIG_SCHEMA(config)
    NUMBER_FINAL_VALIDATE_SCHEMA(config)
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


def test_apply_area_without_select_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """apply_area sends the staged bounds to whichever area the select names."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_hub(wakeup_pin=False))
    )

    with pytest.raises(
        cv.Invalid,
        match=(
            r"^apply_area requires select\.area_id for the same ld6002b instance"
            r" @ data\['apply_area'\]$"
        ),
    ):
        _validated(_buttons(apply_area="Apply Area"))


def test_apply_area_select_on_another_hub_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A select exists, but on a second ld6002b -- which cannot serve this one."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config(
            _hub(wakeup_pin=False), selects=[_select(hub_id=OTHER_HUB_ID)]
        ),
    )

    with pytest.raises(
        cv.Invalid,
        match=(
            r"^apply_area requires select\.area_id for the same ld6002b instance"
            r" @ data\['apply_area'\]$"
        ),
    ):
        _validated(_buttons(apply_area="Apply Area"))


def test_area_config_without_apply_area_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The six numbers only stage a write; apply_area is what sends it."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config(_hub(wakeup_pin=False), selects=[_select()]),
    )

    with pytest.raises(
        cv.Invalid,
        match=(
            r"^area_config requires button\.apply_area for the same ld6002b instance"
            r" @ data\['area_config'\]$"
        ),
    ):
        _validated_numbers(_area_numbers())


def test_area_config_without_select_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The validator's other half: the staged bounds also need an area to land in."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config(
            _hub(wakeup_pin=False), buttons=[_buttons(apply_area="Apply Area")]
        ),
    )

    with pytest.raises(
        cv.Invalid,
        match=(
            r"^area_config requires select\.area_id for the same ld6002b instance"
            r" @ data\['area_config'\]$"
        ),
    ):
        _validated_numbers(_area_numbers())
