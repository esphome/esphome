"""The hub on a modbus_tcp_uart link has to use the same role as the link."""

import pytest

from esphome.components.modbus_tcp_uart import _final_validate, _served_units
import esphome.config_validation as cv
from esphome.core import ID
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
