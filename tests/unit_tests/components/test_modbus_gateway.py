"""Final validation of modbus_gateway."""

from collections.abc import Callable

import pytest

from esphome.components.modbus_gateway import _require_bus_framing, _require_exclusive
from esphome.config import Config
import esphome.config_validation as cv
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType


def _full(items: ConfigType, declared: dict[str, list] | None = None) -> Config:
    full = Config()
    full.update(items)
    for name, path in (declared or {}).items():
        full.declare_ids.append((ID(name, is_declaration=True), path))
    return full


def _run(check: Callable[[ConfigType], None], config: ConfigType, full: Config) -> None:
    token = fv.full_config.set(full)
    try:
        check(config)
    finally:
        fv.full_config.reset(token)


def _gateway(*ports: ConfigType) -> ConfigType:
    return {"uart_id": ID("bus"), "ports": list(ports)}


def test_bus_used_by_a_hub_is_rejected() -> None:
    gateway = _gateway({"id": ID("local", is_declaration=True)})
    full = _full(
        {
            "modbus_gateway": [gateway],
            "modbus": [{"id": ID("hub"), "uart_id": ID("bus")}],
        }
    )
    with pytest.raises(cv.Invalid, match="also used by 'modbus'"):
        _run(_require_exclusive, gateway, full)


def test_port_used_by_another_gateway_is_rejected() -> None:
    first = _gateway({"uart_id": ID("inverter")})
    second = {"uart_id": ID("other"), "ports": [{"uart_id": ID("inverter")}]}
    full = _full({"modbus_gateway": [first, second]})
    with pytest.raises(cv.Invalid, match="also used by 'modbus_gateway'"):
        _run(_require_exclusive, first, full)


def test_hub_on_a_local_port_passes() -> None:
    gateway = _gateway(
        {"uart_id": ID("inverter")}, {"id": ID("local", is_declaration=True)}
    )
    full = _full(
        {
            "modbus_gateway": [gateway],
            "modbus": [{"id": ID("hub"), "uart_id": ID("local")}],
        }
    )
    _run(_require_exclusive, gateway, full)


def test_shared_bus_passes_in_testing_mode(monkeypatch: pytest.MonkeyPatch) -> None:
    gateway = _gateway({"id": ID("local", is_declaration=True)})
    full = _full(
        {
            "modbus_gateway": [gateway],
            "modbus": [{"id": ID("hub"), "uart_id": ID("bus")}],
        }
    )
    monkeypatch.setattr(CORE, "testing_mode", True)
    _run(_require_exclusive, gateway, full)


def test_bus_without_framing_is_rejected_for_a_local_port() -> None:
    gateway = _gateway({"id": ID("local", is_declaration=True)})
    full = _full(
        {
            "uart_split": [{"outputs": [{"id": ID("bus", is_declaration=True)}]}],
            "modbus_gateway": [gateway],
        },
        {"bus": ["uart_split", 0, "outputs", 0, "id"]},
    )
    with pytest.raises(cv.Invalid, match="does not set them"):
        _run(_require_bus_framing, gateway, full)


def test_bus_without_framing_passes_without_a_local_port() -> None:
    gateway = _gateway({"uart_id": ID("inverter")})
    full = _full(
        {
            "uart_split": [{"outputs": [{"id": ID("bus", is_declaration=True)}]}],
            "modbus_gateway": [gateway],
        },
        {"bus": ["uart_split", 0, "outputs", 0, "id"]},
    )
    _run(_require_bus_framing, gateway, full)


def test_uart_bus_with_framing_passes() -> None:
    gateway = _gateway({"id": ID("local", is_declaration=True)})
    bus = {
        "id": ID("bus", is_declaration=True),
        "baud_rate": 9600,
        "data_bits": 8,
        "parity": "NONE",
        "stop_bits": 1,
    }
    full = _full(
        {"uart": [bus], "modbus_gateway": [gateway]}, {"bus": ["uart", 0, "id"]}
    )
    _run(_require_bus_framing, gateway, full)
