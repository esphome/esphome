"""Tests for the uart_tcp encryption key."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart_tcp
from esphome.components.const import CONF_ROLE
from esphome.config import Config
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ID,
    CONF_KEY,
    CONF_PORT,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

LINK_KEY = "MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY="
OTHER_KEY = "ZmVkY2JhOTg3NjU0MzIxMGZlZGNiYTk4NzY1NDMyMTA="


def _validate(
    set_core_config: SetCoreConfigCallable, link: ConfigType, api_key: str | None
) -> ConfigType:
    full = Config()
    full["uart"] = [{CONF_ID: ID("uart_0")}]
    full.declare_ids.append((ID("uart_0"), ["uart", 0, CONF_ID]))
    if api_key is not None:
        full[CONF_API] = {CONF_ENCRYPTION: {CONF_KEY: api_key}}
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    config = uart_tcp.CONFIG_SCHEMA(
        {CONF_UART_ID: "uart_0", CONF_ROLE: "server", CONF_PORT: 6638, **link}
    )
    return uart_tcp.FINAL_VALIDATE_SCHEMA(config)


def test_encryption_needs_a_key(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(cv.Invalid, match="required key not provided"):
        _validate(set_core_config, {CONF_ENCRYPTION: {}}, None)


def test_only_an_encrypted_bridge_loads_noise() -> None:
    plain = {CONF_UART_ID: "uart_0", CONF_ROLE: "server", CONF_PORT: 6638}
    encrypted = {**plain, CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}
    CORE.raw_config = {uart_tcp.DOMAIN: [plain]}
    assert "noise" not in uart_tcp.AUTO_LOAD()
    CORE.raw_config = {uart_tcp.DOMAIN: [plain, encrypted]}
    assert "noise" in uart_tcp.AUTO_LOAD()
    # A single entry is not a list before validation
    CORE.raw_config = {uart_tcp.DOMAIN: encrypted}
    assert "noise" in uart_tcp.AUTO_LOAD()
    # Tooling asks without a config
    CORE.raw_config = None
    assert "noise" in uart_tcp.AUTO_LOAD()


def test_key_differing_from_api_is_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _validate(set_core_config, {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}, OTHER_KEY)


def test_api_key_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(
        cv.Invalid,
        match=r"'uart_tcp' encryption key must differ from the 'api' encryption key",
    ) as err:
        _validate(set_core_config, {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}, LINK_KEY)
    assert err.value.path == [CONF_ENCRYPTION, CONF_KEY]
