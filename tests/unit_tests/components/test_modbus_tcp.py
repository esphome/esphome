"""A modbus_tcp link is the master side. A server hub cannot sit on it."""

import pytest

from esphome.components.modbus_tcp import _final_validate
import esphome.config_validation as cv
from esphome.core import ID
import esphome.final_validate as fv


def _run(config, full):
    token = fv.full_config.set(full)
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_server_hub_on_the_link_is_rejected() -> None:
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


def test_server_hub_on_another_uart_passes() -> None:
    link = ID("mb_link", is_declaration=True)
    config = {"id": link, "tcp_uart_id": ID("sock")}
    full = {"modbus": [{"id": ID("hub"), "uart_id": ID("pins"), "role": "server"}]}
    assert _run(config, full) is None
