"""Tests for the tcp_uart encryption key; the key check itself is tested in noise."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.automation import ACTION_REGISTRY
from esphome.components import tcp_uart
from esphome.components.const import CONF_HOST, CONF_RECONNECT_INTERVAL
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
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
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={CONF_ESPHOME: {}, **(full_config or {})},
    )
    config = tcp_uart.CONFIG_SCHEMA({CONF_HOST: "192.0.2.10", CONF_PORT: 6638, **link})
    return tcp_uart.FINAL_VALIDATE_SCHEMA(config)


@pytest.mark.parametrize("block", [None, {}], ids=["bare", "empty"])
def test_encryption_without_a_key_is_accepted(
    set_core_config: SetCoreConfigCallable, block: dict[str, Any] | None
) -> None:
    # The key is then set at runtime
    config = _validate(
        set_core_config,
        {CONF_ENCRYPTION: block},
        {CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}},
    )
    assert config[CONF_ENCRYPTION] == {}


def test_encryption_rejects_anything_but_a_block(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="expected a dictionary"):
        _validate(set_core_config, {CONF_ENCRYPTION: False})


def test_encryption_needs_a_reconnect_interval_of_one_loop_pass(
    set_core_config: SetCoreConfigCallable,
) -> None:
    link = {CONF_RECONNECT_INTERVAL: "0s", CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}
    with pytest.raises(cv.Invalid, match="must last at least one main loop pass"):
        _validate(set_core_config, link)


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
    # A bare block, whose key is set at runtime
    CORE.raw_config = {tcp_uart.DOMAIN: [{**plain, CONF_ENCRYPTION: None}]}
    assert "noise" in tcp_uart.AUTO_LOAD()
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


def test_the_runtime_key_action(generate_main: Callable[[Path], str]) -> None:
    main_cpp = generate_main(Path(__file__).parent / "test_runtime_key.yaml")
    # The api action and a constant key both reach the entry
    assert main_cpp.count("->set_encryption_key(") == 2
    # The key is kept apart from the api key, also one set at runtime, and from the ota key
    assert "remote_link_noise->set_api_context(" in main_cpp
    assert "remote_link_noise->set_ota_key(" in main_cpp
    # No key from yaml is compiled in
    assert "set_noise_stream(remote_link_noise)" in main_cpp


def test_the_action_schema_checks_the_key(
    set_core_config: SetCoreConfigCallable,
) -> None:
    schema = ACTION_REGISTRY["tcp_uart.set_encryption_key"].schema
    with pytest.raises(cv.Invalid, match="32 bytes"):
        schema({CONF_ID: "remote_link", CONF_KEY: "AAECAw=="})
    with pytest.raises(cv.Invalid, match="Invalid key format"):
        schema({CONF_ID: "remote_link", CONF_KEY: "not base64!"})
    with pytest.raises(cv.Invalid, match="all-zeros key is reserved") as err:
        schema(
            {
                CONF_ID: "remote_link",
                CONF_KEY: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
            }
        )
    # The action needs its key, so the yaml advice to omit it does not apply
    assert "omit" not in str(err.value)
    assert "openssl rand -base64 32" in str(err.value)
    # A lambda is checked on the device
    conf = schema({CONF_ID: "remote_link", CONF_KEY: cv.Lambda("return key;")})
    assert isinstance(conf[CONF_KEY], cv.Lambda)
