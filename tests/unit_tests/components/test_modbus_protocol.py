"""Schema for protocol on the modbus hub."""

from __future__ import annotations

import pytest

from esphome.components.modbus import (
    CONF_PROTOCOL,
    CONFIG_SCHEMA as MODBUS_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    _tcp_has_no_flow_control,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import CONF_FLOW_CONTROL_PIN
from esphome.core import CORE, ID
import esphome.final_validate as fv


def _uart(name: str) -> ID:
    return ID(name, is_declaration=True)


def test_default_hub_stays_rtu_client() -> None:
    out = MODBUS_SCHEMA({"uart_id": _uart("pins")})
    assert out["role"] == "client"
    assert out[CONF_PROTOCOL] == "modbus_rtu"
    assert "turnaround_time" not in out
    assert "send_wait_time" in out


def test_tcp_on_the_uart() -> None:
    out = MODBUS_SCHEMA({"uart_id": _uart("link"), CONF_PROTOCOL: "modbus_tcp"})
    assert out["role"] == "client"
    assert out[CONF_PROTOCOL] == "modbus_tcp"


def test_server_tcp() -> None:
    out = MODBUS_SCHEMA(
        {"uart_id": _uart("link"), "role": "server", CONF_PROTOCOL: "modbus_tcp"}
    )
    assert out["role"] == "server"
    assert out[CONF_PROTOCOL] == "modbus_tcp"


def test_flow_control_is_rejected_on_tcp() -> None:
    with pytest.raises(cv.Invalid, match="flow_control_pin"):
        _tcp_has_no_flow_control(
            {CONF_PROTOCOL: "modbus_tcp", CONF_FLOW_CONTROL_PIN: object()}
        )


def test_turnaround_stays_on_rtu() -> None:
    out = MODBUS_SCHEMA({"uart_id": _uart("pins"), "turnaround_time": "50ms"})
    assert out["turnaround_time"].total_milliseconds == 50


def test_turnaround_is_rejected_on_tcp() -> None:
    with pytest.raises(cv.Invalid, match="turnaround_time"):
        MODBUS_SCHEMA(
            {
                "uart_id": _uart("link"),
                CONF_PROTOCOL: "modbus_tcp",
                "turnaround_time": "600ms",
            }
        )


def _hubs(*hubs: dict) -> list[dict]:
    return [
        MODBUS_SCHEMA({"id": ID(f"hub_{n}", is_declaration=True), **hub})
        for n, hub in enumerate(hubs)
    ]


def _final(hubs: list[dict]) -> None:
    full = Config()
    full["modbus"] = hubs
    token = fv.full_config.set(full)
    try:
        for hub in hubs:
            FINAL_VALIDATE_SCHEMA(hub)
    finally:
        fv.full_config.reset(token)


def test_tcp_hub_cannot_share_its_uart() -> None:
    link = _uart("link")
    with pytest.raises(cv.Invalid, match="cannot share its UART"):
        _final(_hubs({"uart_id": link}, {"uart_id": link, CONF_PROTOCOL: "modbus_tcp"}))


def test_rtu_hubs_share_a_uart_as_before() -> None:
    bus = _uart("bus")
    _final(_hubs({"uart_id": bus}, {"uart_id": bus, "role": "server"}))


def test_testing_mode_skips_the_tcp_uart_check() -> None:
    link = _uart("link")
    testing = CORE.testing_mode
    CORE.testing_mode = True
    try:
        _final(
            _hubs(
                {"uart_id": link, CONF_PROTOCOL: "modbus_tcp"},
                {"uart_id": link, CONF_PROTOCOL: "modbus_tcp", "role": "server"},
            )
        )
    finally:
        CORE.testing_mode = testing
