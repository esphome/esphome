"""Tests for rfc2217_uart: exclusive use of the UARTs, the ESP32 line change and the client codegen."""

import pytest

from esphome import config_validation as cv
from esphome.components import rfc2217_uart
from esphome.config import Config
from esphome.const import (
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

_final_validate = rfc2217_uart._final_validate

DIR = "tests/component_tests/rfc2217_uart"


def _full_config(uarts: list[ConfigType] | None = None, **domains) -> Config:
    """A full config declaring uart_0, uart_1 and tcp_uart link, as the ID pass leaves it."""
    uarts = uarts or [{CONF_ID: ID("uart_0")}, {CONF_ID: ID("uart_1")}]
    full = Config()
    full["uart"] = uarts
    for index, uart_conf in enumerate(uarts):
        full.declare_ids.append((uart_conf[CONF_ID], ["uart", index, CONF_ID]))
    full["tcp_uart"] = [{CONF_ID: ID("link")}, {CONF_ID: ID("link2")}]
    full.update(domains)
    return full


def _entry(uart_id: str = "uart_0", tcp_uart_id: str = "link") -> ConfigType:
    return {
        "role": "server",
        CONF_UART_ID: ID(uart_id),
        rfc2217_uart.CONF_TCP_UART_ID: ID(tcp_uart_id),
    }


def _set(set_core_config: SetCoreConfigCallable, full: Config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)


def test_accepts_entries_on_distinct_uarts(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry("uart_0", "link"))
    _final_validate(_entry("uart_1", "link2"))


def test_rejects_a_tcp_uart_as_the_hardware_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    with pytest.raises(cv.Invalid, match="not a tcp_uart"):
        _final_validate(_entry("link2", "link"))


@pytest.mark.parametrize(
    ("first", "second"),
    [
        (("uart_0", "link"), ("uart_0", "link2")),
        (("uart_0", "link"), ("uart_1", "link")),
    ],
)
def test_rejects_two_entries_on_one_uart(
    set_core_config: SetCoreConfigCallable,
    first: tuple[str, str],
    second: tuple[str, str],
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry(*first))
    with pytest.raises(cv.Invalid, match="already used by another 'rfc2217_uart'"):
        _final_validate(_entry(*second))


@pytest.mark.parametrize("shared", ["uart_0", "link"])
def test_rejects_a_uart_shared_with_another_component(
    set_core_config: SetCoreConfigCallable, shared: str
) -> None:
    _set(
        set_core_config,
        _full_config(modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID(shared)}]),
    )
    with pytest.raises(cv.Invalid, match="also used by 'modbus'"):
        _final_validate(_entry())


def test_testing_mode_allows_a_shared_bus(
    set_core_config: SetCoreConfigCallable, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Grouped CI builds put several components on one bus package.
    monkeypatch.setattr(CORE, "testing_mode", True)
    _set(
        set_core_config,
        _full_config(modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID("uart_0")}]),
    )
    _final_validate(_entry())


def test_rejects_dummy_receiver(set_core_config: SetCoreConfigCallable) -> None:
    _set(
        set_core_config,
        _full_config(
            uarts=[{CONF_ID: ID("uart_0"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: True}}]
        ),
    )
    with pytest.raises(cv.Invalid, match="dummy_receiver"):
        _final_validate(_entry())


def test_rejects_a_client_as_the_hardware_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        _full_config(
            rfc2217_uart=[
                {CONF_ID: ID("remote"), "role": "client"},
            ]
        ),
    )
    with pytest.raises(cv.Invalid, match="not a tcp_uart or an rfc2217_uart client"):
        _final_validate(_entry("remote", "link2"))


def test_rejects_a_client_on_a_tcp_uart_in_use(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config, _full_config())
    _final_validate(_entry("uart_0", "link"))
    with pytest.raises(cv.Invalid, match="already used by another 'rfc2217_uart'"):
        _final_validate({"role": "client", rfc2217_uart.CONF_TCP_UART_ID: ID("link")})


def test_server_needs_the_hardware_uart() -> None:
    with pytest.raises(cv.Invalid, match="uart_id"):
        rfc2217_uart.CONFIG_SCHEMA({"role": "server", "tcp_uart_id": "link"})


def test_client_needs_a_baud_rate() -> None:
    with pytest.raises(cv.Invalid, match="baud_rate"):
        rfc2217_uart.CONFIG_SCHEMA({"tcp_uart_id": "link"})


def test_client_rejects_one_and_a_half_stop_bits() -> None:
    with pytest.raises(cv.Invalid, match="stop_bits"):
        rfc2217_uart.CONFIG_SCHEMA(
            {"tcp_uart_id": "link", "baud_rate": 9600, "stop_bits": 1.5}
        )


def test_client_sends_its_line_settings(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_client.yaml")
    assert "remote_port->set_tcp_uart(remote_link);" in main_cpp
    assert "remote_port->set_baud_rate(19200);" in main_cpp
    assert "remote_port->set_data_bits(7);" in main_cpp
    assert "remote_port->set_stop_bits(2);" in main_cpp
    assert "remote_port->set_parity(uart::UART_CONFIG_PARITY_EVEN);" in main_cpp
    assert {"USE_RFC2217_UART_CLIENT", "USE_UART_VIRTUAL"} <= {
        define.name for define in CORE.defines
    }


def test_server_alone_needs_no_client(generate_main) -> None:
    generate_main(f"{DIR}/test_rfc2217_uart.yaml")
    defines = {define.name for define in CORE.defines}
    assert not {"USE_RFC2217_UART_CLIENT", "USE_UART_VIRTUAL"} & defines


def test_esp32_hardware_uart_changes_its_line_in_place(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_rfc2217_uart.yaml")
    assert "port_server->set_tcp_uart(inbound);" in main_cpp
    assert "port_server->set_uart_parent(serial_bus);" in main_cpp
    assert "port_server->set_idf_uart(serial_bus);" in main_cpp
