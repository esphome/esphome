"""Tests for the tcp_uart encryption key; the key check itself is tested in noise."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import tcp_uart
from esphome.components.const import CONF_HOST
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_KEY,
    CONF_PORT,
    PlatformFramework,
)
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

LINK_KEY = "MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY="


def _validate(
    set_core_config: SetCoreConfigCallable,
    link: dict[str, Any],
    full_config: dict[str, Any] | None = None,
) -> ConfigType:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config or {})
    config = tcp_uart.CONFIG_SCHEMA({CONF_HOST: "192.0.2.10", CONF_PORT: 6638, **link})
    return tcp_uart.FINAL_VALIDATE_SCHEMA(config)


def test_encryption_needs_a_key(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(cv.Invalid, match="required key not provided"):
        _validate(set_core_config, {CONF_ENCRYPTION: {}})


def test_only_an_encrypted_link_loads_noise() -> None:
    plain = {CONF_HOST: "192.0.2.10", CONF_PORT: 6638}
    encrypted = {**plain, CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}
    CORE.raw_config = {tcp_uart.DOMAIN: [plain]}
    assert "noise" not in tcp_uart.AUTO_LOAD()
    CORE.raw_config = {tcp_uart.DOMAIN: [plain, encrypted]}
    assert "noise" in tcp_uart.AUTO_LOAD()
    # A single entry is not a list before validation
    CORE.raw_config = {tcp_uart.DOMAIN: encrypted}
    assert "noise" in tcp_uart.AUTO_LOAD()
    CORE.raw_config = {tcp_uart.DOMAIN: None}
    assert "noise" not in tcp_uart.AUTO_LOAD()
    # Tooling asks without a config
    CORE.raw_config = None
    assert "noise" in tcp_uart.AUTO_LOAD()


def test_modbus_needs_no_uart_block(generate_main: Callable[[Path], str]) -> None:
    # modbus depends on uart; tcp_uart loads it before that check runs
    main_cpp = generate_main(Path(__file__).parent / "test_modbus_on_tcp_uart.yaml")
    assert "set_noise_stream(" in main_cpp


def test_api_key_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(
        cv.Invalid,
        match=r"'tcp_uart' encryption key must differ from the 'api' encryption key; "
        r"the peer device holds this key",
    ) as err:
        _validate(
            set_core_config,
            {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}},
            {CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}},
        )
    assert err.value.path == [CONF_ENCRYPTION, CONF_KEY]
