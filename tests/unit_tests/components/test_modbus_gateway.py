"""Direction checks for a gateway port."""

import pytest

from esphome.components.modbus_gateway import _reject_port_direction
import esphome.config_validation as cv
from esphome.core import ID
import esphome.final_validate as fv


def _run(config, full):
    token = fv.full_config.set(full)
    try:
        return _reject_port_direction(config)
    finally:
        fv.full_config.reset(token)


def test_server_hub_on_local_port_is_rejected() -> None:
    local = ID("local_bus", is_declaration=True)
    config = {"ports": [{"id": local}]}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("local_bus"), "role": "server"}]}
    with pytest.raises(cv.Invalid, match="role: client"):
        _run(config, full)


def test_client_hub_on_local_port_passes() -> None:
    local = ID("local_bus", is_declaration=True)
    config = {"ports": [{"id": local}]}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("local_bus"), "role": "client"}]}
    assert _run(config, full) is config


def test_modbus_tcp_link_cannot_be_a_port() -> None:
    link = ID("shelly_link")
    config = {"ports": [{"uart_id": link}]}
    full = {
        "modbus_tcp": [
            {"id": ID("shelly_link", is_declaration=True), "tcp_uart_id": ID("sock")}
        ]
    }
    with pytest.raises(cv.Invalid, match="modbus_tcp link"):
        _run(config, full)


def test_modbus_tcp_link_elsewhere_does_not_reject_a_uart_port() -> None:
    """The bus may be a modbus_tcp link. Only a port is rejected."""
    port = ID("inverter_bus")
    config = {"ports": [{"uart_id": port}]}
    full = {
        "modbus_tcp": [
            {"id": ID("shelly_link", is_declaration=True), "tcp_uart_id": ID("sock")}
        ]
    }
    assert _run(config, full) is config
