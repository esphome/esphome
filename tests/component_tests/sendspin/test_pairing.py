"""Validation tests for the sendspin hub's pairing options.

These cover the rejection branches, which a compile test cannot reach: a
`test*.yaml` can only assert that a configuration is accepted.
"""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import CONF_STATIC_PAIRING_CODE, CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _hub_config(**overrides: Any) -> ConfigType:
    """Build a minimal valid hub config, allowing field overrides."""
    config: ConfigType = {"id": "sendspin_hub_id"}
    config.update(overrides)
    return config


def test_minimal_config_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    """The baseline the rejection tests vary is itself valid."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(_hub_config())

    assert CONF_STATIC_PAIRING_CODE not in config


@pytest.mark.parametrize("code", ["01234567", "00000000", "99999999"])
def test_static_pairing_code_accepted(
    set_core_config: SetCoreConfigCallable, code: str
) -> None:
    """Eight decimal digits, leading zeros included, pass through unchanged."""
    set_core_config(PlatformFramework.ESP32_IDF)

    assert (
        CONFIG_SCHEMA(_hub_config(static_pairing_code=code))[CONF_STATIC_PAIRING_CODE]
        == code
    )


@pytest.mark.parametrize(
    "code",
    [
        "0123456",  # too short
        "012345678",  # too long
        "0123456a",  # not all decimal digits
        "0123 567",  # whitespace is not a digit
        "０１２３４５６７",  # full-width digits are not ASCII
        "",
    ],
)
def test_static_pairing_code_rejected(
    set_core_config: SetCoreConfigCallable, code: str
) -> None:
    """Anything that is not exactly eight decimal digits is refused here, since the
    library would otherwise refuse to start the client."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="exactly 8 decimal digits"):
        CONFIG_SCHEMA(_hub_config(static_pairing_code=code))


def test_unquoted_static_pairing_code_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """An unquoted YAML code arrives as an int, having already lost its leading
    zeros, so it is refused rather than silently pairing with the wrong value."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_hub_config(static_pairing_code=1234567))
