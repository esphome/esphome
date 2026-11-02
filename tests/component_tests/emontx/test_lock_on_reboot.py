"""Tests for the emontx lock_on_reboot option: auto (the default), true or false."""

import pytest

from esphome import config_validation as cv
from esphome.components.emontx import (
    CONF_LOCK_ON_REBOOT,
    CONFIG_SCHEMA,
    LOCK_ON_REBOOT_AUTO,
)


def _lock_key() -> cv.Optional:
    return next(key for key in CONFIG_SCHEMA.schema if key == CONF_LOCK_ON_REBOOT)


def test_default_is_auto() -> None:
    assert _lock_key().default() == LOCK_ON_REBOOT_AUTO


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("auto", LOCK_ON_REBOOT_AUTO),
        ("AUTO", LOCK_ON_REBOOT_AUTO),
        (True, True),
        (False, False),
        ("yes", True),
        ("off", False),
    ],
)
def test_accepted_values(value: object, expected: object) -> None:
    assert CONFIG_SCHEMA.schema[_lock_key()](value) == expected


def test_other_values_rejected() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA.schema[_lock_key()]("sometimes")
