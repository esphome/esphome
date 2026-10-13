"""Validation tests for the unpaired access policy.

The hub's `unpaired_access` key and an `unpaired_access` switch both set the
policy, so the switch platform rejects a config that has both.
"""

import logging

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import (
    CONF_UNPAIRED_ACCESS,
    CONFIG_SCHEMA as HUB_CONFIG_SCHEMA,
    DOMAIN,
    FINAL_VALIDATE_SCHEMA as HUB_FINAL_VALIDATE_SCHEMA,
    _get_data,
    request_pairing_code_display_support,
)
from esphome.components.sendspin.switch import (
    CONFIG_SCHEMA as SWITCH_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as SWITCH_FINAL_VALIDATE_SCHEMA,
)
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "sendspin_hub_id"
NO_PAIRING_WARNING = "no new server can pair with this device"


def _switch_config(switch_type: str) -> ConfigType:
    return SWITCH_CONFIG_SCHEMA(
        {"name": "Sendspin Switch", "type": switch_type, "sendspin_id": HUB_ID}
    )


def test_hub_key_absent_by_default(set_core_config: SetCoreConfigCallable) -> None:
    """With no default in the schema, the switch can tell whether the key was set."""
    set_core_config(PlatformFramework.ESP32_IDF)

    assert CONF_UNPAIRED_ACCESS not in HUB_CONFIG_SCHEMA({"id": HUB_ID})


@pytest.mark.parametrize("value", [True, False])
def test_hub_key_accepted(set_core_config: SetCoreConfigCallable, value: bool) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = HUB_CONFIG_SCHEMA({"id": HUB_ID, CONF_UNPAIRED_ACCESS: value})

    assert config[CONF_UNPAIRED_ACCESS] is value


def test_off_without_pairing_method_warns(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """With unpaired access off and no way to pair, no new server could pair."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = HUB_CONFIG_SCHEMA({"id": HUB_ID, CONF_UNPAIRED_ACCESS: False})

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING in caplog.text


@pytest.mark.parametrize("method", ["static_pairing_code", "pairing_code"])
def test_off_with_pairing_method_does_not_warn(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
    method: str,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    hub_config = {"id": HUB_ID, CONF_UNPAIRED_ACCESS: False}
    if method == "static_pairing_code":
        hub_config["static_pairing_code"] = "01234567"
    else:
        request_pairing_code_display_support()
    config = HUB_CONFIG_SCHEMA(hub_config)

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING not in caplog.text


def test_on_does_not_warn(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = HUB_CONFIG_SCHEMA({"id": HUB_ID})

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING not in caplog.text


def test_switch_with_hub_key_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """Both would set the policy, and the hub's value would be silently ignored."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={DOMAIN: {"id": HUB_ID, CONF_UNPAIRED_ACCESS: False}},
    )
    config = _switch_config(CONF_UNPAIRED_ACCESS)

    with pytest.raises(cv.Invalid, match="set the switch's restore_mode instead"):
        SWITCH_FINAL_VALIDATE_SCHEMA(config)


def test_switch_without_hub_key_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={DOMAIN: {"id": HUB_ID}})
    config = _switch_config(CONF_UNPAIRED_ACCESS)

    assert SWITCH_FINAL_VALIDATE_SCHEMA(config) is config


def test_switch_without_pairing_method_warns(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """Turning the switch off would leave no way for a new server to pair."""
    set_core_config(PlatformFramework.ESP32_IDF, full_config={DOMAIN: {"id": HUB_ID}})
    config = _switch_config(CONF_UNPAIRED_ACCESS)

    with caplog.at_level(logging.WARNING):
        SWITCH_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING in caplog.text


def test_switch_with_pairing_method_does_not_warn(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={DOMAIN: {"id": HUB_ID, "static_pairing_code": "01234567"}},
    )
    config = _switch_config(CONF_UNPAIRED_ACCESS)

    with caplog.at_level(logging.WARNING):
        SWITCH_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING not in caplog.text


def test_enabled_switch_ignores_hub_key(set_core_config: SetCoreConfigCallable) -> None:
    """Only the unpaired access switch competes with the hub key."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={DOMAIN: {"id": HUB_ID, CONF_UNPAIRED_ACCESS: False}},
    )
    config = _switch_config("enabled")

    assert SWITCH_FINAL_VALIDATE_SCHEMA(config) is config


@pytest.mark.parametrize("switch_type", ["enabled", CONF_UNPAIRED_ACCESS])
def test_duplicate_switch_type_rejected(
    set_core_config: SetCoreConfigCallable, switch_type: str
) -> None:
    """Two switches of one type would fight over the same hub setting."""
    entry = {"platform": DOMAIN, "type": switch_type}
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={DOMAIN: {"id": HUB_ID}, "switch": [entry, dict(entry)]},
    )
    config = _switch_config(switch_type)

    with pytest.raises(cv.Invalid, match="Only one sendspin"):
        SWITCH_FINAL_VALIDATE_SCHEMA(config)


@pytest.mark.parametrize("switch_type", ["enabled", CONF_UNPAIRED_ACCESS])
def test_switch_requests_its_type(
    set_core_config: SetCoreConfigCallable, switch_type: str
) -> None:
    """The hub skips the codegen value for a setting a switch drives, so the client
    waits for that switch to restore before its first start."""
    set_core_config(PlatformFramework.ESP32_IDF)

    _switch_config(switch_type)

    assert _get_data().switch_types == {switch_type}
