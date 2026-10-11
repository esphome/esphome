"""Tests for the uart_tcp final validation: one reader per UART, one server per port."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart_tcp
from esphome.components.const import CONF_ROLE
from esphome.config import Config
from esphome.const import (
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_PORT,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

_final_validate = uart_tcp._final_validate


def _full_config(uarts: list[ConfigType] | None = None, **domains) -> Config:
    """A full config declaring uart_0 and uart_1, as the ID pass leaves it."""
    uarts = uarts or [{CONF_ID: ID("uart_0")}, {CONF_ID: ID("uart_1")}]
    full = Config()
    full["uart"] = uarts
    for index, uart_conf in enumerate(uarts):
        full.declare_ids.append((uart_conf[CONF_ID], ["uart", index, CONF_ID]))
    full.update(domains)
    return full


def _entry(uart_id: str, port: int = 8899, role: str = "server") -> ConfigType:
    return {CONF_UART_ID: ID(uart_id), CONF_PORT: port, CONF_ROLE: role}


def _set(set_core_config: SetCoreConfigCallable, full: Config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)


def test_accepts_entries_on_distinct_uarts_and_ports(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry("uart_0", 8899))
    _final_validate(_entry("uart_1", 8900))


def test_rejects_two_entries_on_one_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry("uart_0", 8899))
    with pytest.raises(cv.Invalid, match="already used by another 'uart_tcp'"):
        _final_validate(_entry("uart_0", 8900))


def test_rejects_uart_shared_with_another_component(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        _full_config(modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID("uart_0")}]),
    )
    with pytest.raises(cv.Invalid, match="also used by 'modbus'"):
        _final_validate(_entry("uart_0"))


def test_ignores_other_components_on_other_uarts(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        _full_config(
            modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID("uart_1")}],
            uart_tcp=[_entry("uart_0")],
        ),
    )
    _final_validate(_entry("uart_0"))


def test_testing_mode_allows_a_shared_bus(
    set_core_config: SetCoreConfigCallable, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Grouped CI builds put several components on one bus package.
    monkeypatch.setattr(CORE, "testing_mode", True)
    _set(
        set_core_config,
        _full_config(modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID("uart_0")}]),
    )
    _final_validate(_entry("uart_0"))


def test_rejects_dummy_receiver(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        _full_config(
            uarts=[{CONF_ID: ID("uart_0"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: True}}]
        ),
    )
    with pytest.raises(cv.Invalid, match="dummy_receiver"):
        _final_validate(_entry("uart_0"))


def test_rejects_two_servers_on_one_port(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry("uart_0", 8899))
    with pytest.raises(cv.Invalid, match="Port 8899 is already the listen port"):
        _final_validate(_entry("uart_1", 8899))


def test_rejects_server_on_a_tcp_uart_server_port(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        _full_config(
            tcp_uart=[{CONF_ID: ID("tcp"), CONF_ROLE: "server", CONF_PORT: 502}]
        ),
    )
    with pytest.raises(cv.Invalid, match="Port 502 is already the listen port"):
        _final_validate(_entry("uart_0", 502))


def test_clients_may_share_a_port_number(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # A client port is the remote end; only listeners collide.
    _set(
        set_core_config,
        _full_config(
            tcp_uart=[{CONF_ID: ID("tcp"), CONF_ROLE: "client", CONF_PORT: 502}]
        ),
    )
    _final_validate(_entry("uart_0", 502))
    _final_validate(_entry("uart_1", 502, role="client"))
