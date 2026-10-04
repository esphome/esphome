"""Schema for protocol on the modbus hub."""

from __future__ import annotations

import pytest

from esphome.components.modbus import (
    CONF_PEER_FLOW_CONTROL_PIN,
    CONF_PEER_ID,
    CONF_PEER_PROTOCOL,
    CONF_PROTOCOL,
    CONFIG_SCHEMA as MODBUS_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    _tcp_has_no_flow_control,
)
from esphome.components.uart import KEY_UART_DEVICES
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import CONF_FLOW_CONTROL_PIN, CONF_RX_PIN, CONF_TX_PIN
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


def test_peer_forwards_and_does_not_require_a_typed_role() -> None:
    out = MODBUS_SCHEMA(
        {
            "uart_id": _uart("link"),
            CONF_PROTOCOL: "modbus_tcp",
            CONF_PEER_ID: _uart("meter"),
            CONF_PEER_PROTOCOL: "modbus_rtu",
        }
    )
    assert out["role"] == "forward"
    assert out[CONF_PEER_PROTOCOL] == "modbus_rtu"
    assert out["send_wait_time"].total_milliseconds == 2000


def test_both_sides_may_be_rtu() -> None:
    out = MODBUS_SCHEMA(
        {
            "uart_id": _uart("meter"),
            CONF_PROTOCOL: "modbus_rtu",
            CONF_PEER_ID: _uart("link"),
            CONF_PEER_PROTOCOL: "modbus_rtu",
        }
    )
    assert out["role"] == "forward"


def test_role_and_peer_cannot_both_be_set() -> None:
    with pytest.raises(cv.Invalid, match="role and peer_id"):
        MODBUS_SCHEMA(
            {
                "uart_id": _uart("link"),
                "role": "server",
                CONF_PROTOCOL: "modbus_tcp",
                CONF_PEER_ID: _uart("meter"),
                CONF_PEER_PROTOCOL: "modbus_rtu",
            }
        )


def test_peer_requires_both_protocols() -> None:
    with pytest.raises(cv.Invalid, match="peer_protocol"):
        MODBUS_SCHEMA({"uart_id": _uart("link"), CONF_PEER_ID: _uart("meter")})


def test_peer_must_be_a_different_uart() -> None:
    with pytest.raises(cv.Invalid, match="different UART"):
        MODBUS_SCHEMA(
            {
                "uart_id": _uart("link"),
                CONF_PROTOCOL: "modbus_rtu",
                CONF_PEER_ID: _uart("link"),
                CONF_PEER_PROTOCOL: "modbus_tcp",
            }
        )


def test_peer_flow_control_is_rejected_on_a_tcp_peer() -> None:
    with pytest.raises(cv.Invalid, match="peer_flow_control_pin"):
        _tcp_has_no_flow_control(
            {
                CONF_PROTOCOL: "modbus_rtu",
                CONF_PEER_PROTOCOL: "modbus_tcp",
                CONF_PEER_FLOW_CONTROL_PIN: object(),
            }
        )


def test_forward_takes_a_flow_control_pin_on_each_rtu_side() -> None:
    config = {
        CONF_PROTOCOL: "modbus_rtu",
        CONF_PEER_PROTOCOL: "modbus_rtu",
        CONF_FLOW_CONTROL_PIN: object(),
        CONF_PEER_FLOW_CONTROL_PIN: object(),
    }
    assert _tcp_has_no_flow_control(config) is config


def test_forward_turnaround_belongs_to_the_peer() -> None:
    out = MODBUS_SCHEMA(
        {
            "uart_id": _uart("link"),
            CONF_PROTOCOL: "modbus_tcp",
            CONF_PEER_ID: _uart("meter"),
            CONF_PEER_PROTOCOL: "modbus_rtu",
            "turnaround_time": "150ms",
        }
    )
    assert out["turnaround_time"].total_milliseconds == 150
    with pytest.raises(cv.Invalid, match="peer_protocol: modbus_tcp"):
        MODBUS_SCHEMA(
            {
                "uart_id": _uart("meter"),
                CONF_PROTOCOL: "modbus_rtu",
                CONF_PEER_ID: _uart("link"),
                CONF_PEER_PROTOCOL: "modbus_tcp",
                "turnaround_time": "150ms",
            }
        )


def test_hub_shares_a_uart_with_another_component() -> None:
    full = Config()
    link = _uart("link")
    full.data[KEY_UART_DEVICES] = {link: {CONF_RX_PIN: "gps"}}
    token = fv.full_config.set(full)
    try:
        FINAL_VALIDATE_SCHEMA(MODBUS_SCHEMA({"uart_id": link}))
        assert full.data[KEY_UART_DEVICES][link] == {CONF_RX_PIN: "gps"}
    finally:
        fv.full_config.reset(token)


def _forward(link: ID, meter: ID) -> dict:
    return {
        "uart_id": link,
        CONF_PROTOCOL: "modbus_tcp",
        CONF_PEER_ID: meter,
        CONF_PEER_PROTOCOL: "modbus_rtu",
    }


def test_forward_times_are_capped_at_16_bit_milliseconds() -> None:
    hub = _forward(_uart("link"), _uart("meter")) | {"turnaround_time": "65535ms"}
    assert MODBUS_SCHEMA(hub)["turnaround_time"].total_milliseconds == 65535
    with pytest.raises(cv.Invalid, match="at most 65535ms"):
        MODBUS_SCHEMA(
            _forward(_uart("link"), _uart("meter")) | {"send_wait_time": "70s"}
        )


def test_explicit_role_forward_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="role and peer_id"):
        MODBUS_SCHEMA({**_forward(_uart("link"), _uart("meter")), "role": "forward"})


def test_peer_protocol_needs_peer_id() -> None:
    with pytest.raises(cv.Invalid, match="peer_protocol needs peer_id"):
        MODBUS_SCHEMA({"uart_id": _uart("link"), CONF_PEER_PROTOCOL: "modbus_rtu"})


def test_forward_claims_both_uarts() -> None:
    link, meter = _uart("link"), _uart("meter")
    full = Config()
    hubs = _hubs(_forward(link, meter))
    full["modbus"] = hubs
    token = fv.full_config.set(full)
    try:
        FINAL_VALIDATE_SCHEMA(hubs[0])
        for uart_id in (link, meter):
            assert full.data[KEY_UART_DEVICES][uart_id] == {
                CONF_TX_PIN: "modbus",
                CONF_RX_PIN: "modbus",
            }
    finally:
        fv.full_config.reset(token)


def test_forward_cannot_share_a_uart_with_another_hub() -> None:
    link, meter = _uart("link"), _uart("meter")
    with pytest.raises(cv.Invalid, match="cannot share its UART"):
        _final(_hubs({"uart_id": meter}, _forward(link, meter)))


def test_forward_rejects_a_uart_used_by_another_component() -> None:
    link, meter = _uart("link"), _uart("meter")
    full = Config()
    full.data[KEY_UART_DEVICES] = {meter: {CONF_TX_PIN: "logger"}}
    hubs = _hubs(_forward(link, meter))
    full["modbus"] = hubs
    token = fv.full_config.set(full)
    try:
        with pytest.raises(cv.Invalid, match="logger"):
            FINAL_VALIDATE_SCHEMA(hubs[0])
    finally:
        fv.full_config.reset(token)
