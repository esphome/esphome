"""The hub on a modbus_tcp_uart link has to use the same role as the link."""

import pytest

from esphome.components.modbus_tcp_uart import (
    CONFIG_SCHEMA,
    _final_validate,
    _reply_timeout_ms,
    _served_units,
)
import esphome.config_validation as cv
from esphome.core import ID, TimePeriod
import esphome.final_validate as fv


def _run(config, full):
    token = fv.full_config.set(full)
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_server_hub_on_a_client_link_is_rejected() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock")}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("mb_link"), "role": "server"}]}
    with pytest.raises(cv.Invalid, match="role: client"):
        _run(config, full)


def test_client_hub_on_the_link_passes() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock")}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("mb_link"), "role": "client"}]}
    assert _run(config, full) is None


def test_server_hub_on_a_server_link_passes() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock"), "role": "server"}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("mb_link"), "role": "server"}]}
    assert _run(config, full) is None


def test_client_hub_on_a_server_link_is_rejected() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock"), "role": "server"}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("mb_link"), "role": "client"}]}
    with pytest.raises(cv.Invalid, match="role: server"):
        _run(config, full)


def test_server_hub_on_another_uart_passes() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock")}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("pins"), "role": "server"}]}
    assert _run(config, full) is None


def test_served_units_are_those_on_the_link_hubs() -> None:
    full = {
        "modbus": [
            {"id": ID("hub"), "uart_id": ID("mb_link"), "role": "server"},
            {"id": ID("other"), "uart_id": ID("pins"), "role": "server"},
        ],
        "modbus_server": [
            {"id": ID("a"), "modbus_id": ID("hub"), "address": 255},
            {"id": ID("b"), "modbus_id": ID("other"), "address": 2},
        ],
        "hoermann_hcp": [{"id": ID("c"), "modbus_id": ID("hub"), "address": 1}],
    }
    assert _served_units(full, "mb_link") == [1, 255]
    assert _served_units(full, "bridge_link") == []


def test_a_block_without_address_is_ignored() -> None:
    # A block that names the hub but serves no unit.
    full = {
        "modbus": [{"id": ID("hub"), "uart_id": ID("mb_link"), "role": "server"}],
        "other_block": [{"modbus_id": ID("hub")}],
    }
    assert _served_units(full, "mb_link") == []
    full["modbus_server"] = [{"id": ID("a"), "modbus_id": ID("hub"), "address": 1}]
    assert _served_units(full, "mb_link") == [1]


def test_reply_timeout_outlasts_two_transactions_on_a_client_hub() -> None:
    full = {
        "modbus": [
            {
                "id": ID("rtu"),
                "role": "client",
                "send_wait_time": TimePeriod(seconds=2),
                "turnaround_time": TimePeriod(milliseconds=600),
            },
            {"id": ID("tcp"), "role": "server"},
        ],
    }
    # 1 s for the reply, plus 2 x (0.6 s + 2 s): the transaction ahead and this one
    assert _reply_timeout_ms({}, full) == 6200
    assert (
        _reply_timeout_ms({}, {"modbus": [{"id": ID("tcp"), "role": "server"}]}) == 1000
    )


def test_reply_timeout_replaces_the_computed_wait() -> None:
    full = {
        "modbus": [
            {
                "id": ID("rtu"),
                "role": "client",
                "send_wait_time": TimePeriod(seconds=2),
                "turnaround_time": TimePeriod(milliseconds=600),
            },
        ],
    }
    config = {"reply_timeout": TimePeriod(milliseconds=3000)}
    # Without the key this client hub gives 6200 ms, as in the test above.
    assert _reply_timeout_ms(config, full) == 3000


def test_reply_timeout_is_accepted_for_a_server_link() -> None:
    config = CONFIG_SCHEMA(
        {"tcp_uart_id": "sock", "role": "server", "reply_timeout": "3s"}
    )
    assert config["reply_timeout"].total_milliseconds == 3000


def test_reply_timeout_is_rejected_for_a_client_link() -> None:
    with pytest.raises(cv.Invalid, match="only used with role: server"):
        CONFIG_SCHEMA({"tcp_uart_id": "sock", "reply_timeout": "3s"})
