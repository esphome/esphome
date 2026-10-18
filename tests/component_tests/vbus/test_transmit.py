"""Optional TX transport validation and generated code."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("address", [1, 0x20, 0x2211, 0x7F7F])
def test_valid_addresses(address: int) -> None:
    assert vbus.validate_address(address) == address


@pytest.mark.parametrize("address", [0, -1, 0x80, 0x8000, 0xFFFF, 0x10000])
def test_invalid_addresses(address: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.validate_address(address)


@pytest.mark.parametrize("command", [0x80, 0x8000, 0xFFFF, 0x0500, 0x0600, -1, 0x10000])
def test_invalid_commands(command: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.validate_command(command)


@pytest.mark.parametrize("grant", [0, -1, 0x100000000])
def test_send_requires_a_nonzero_32_bit_grant(grant: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.SEND_SCHEMA({"id": "bus", "grant_id": grant, "command": 0x0300})


def test_send_requires_a_grant_identifier() -> None:
    with pytest.raises(cv.Invalid):
        vbus.SEND_SCHEMA({"id": "bus", "command": 0x0300})


def test_transmit_requires_distinct_addresses() -> None:
    with pytest.raises(cv.Invalid, match="must differ"):
        vbus.TRANSMIT_SCHEMA({"source": 0x20, "destination": 0x20})


@pytest.mark.parametrize("duration", ["0ms", "199ms", "1001ms"])
def test_invalid_lease_duration(duration: str) -> None:
    with pytest.raises(cv.Invalid):
        vbus.TRANSMIT_SCHEMA({"destination": 0x2211, "lease_duration": duration})


def test_transmit_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("transmit.yaml"))
    assert any(define.name == "USE_VBUS_TX" for define in CORE.defines)
    assert "vbus::VBusTransmitter(test_bus);" in code
    assert "test_bus->set_transmitter(" in code
    assert "add_on_bus_grant_callback(" in code
    assert "send_datagram(" in code
    assert "request_bus()" in code
    assert "release_bus(" in code
    assert "uint32_t grant_id" in code
    assert "()()" not in code


def test_rx_only_does_not_generate_transmit(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("transmit_rx_only.yaml"))
    assert all(define.name != "USE_VBUS_TX" for define in CORE.defines)
    assert "VBusTransmitter" not in code


@pytest.mark.parametrize(
    ("uart_config", "transmit_config", "error"),
    [
        (
            "rx_pin: GPIO13\n  tx_pin: GPIO12",
            "transmit:\n    destination: 0x2211",
            None,
        ),
        ("rx_pin: GPIO13", "transmit:\n    destination: 0x2211", "declare a tx_pin"),
        ("tx_pin: GPIO12", "transmit:\n    destination: 0x2211", "declare a rx_pin"),
        ("rx_pin: GPIO13", "", "require transmit"),
        (
            "rx_pin: GPIO13\n  tx_pin: GPIO12\n  parity: EVEN",
            "transmit:\n    destination: 0x2211",
            "requires parity NONE",
        ),
        (
            "rx_pin: GPIO13\n  tx_pin: GPIO12\n  stop_bits: 2",
            "transmit:\n    destination: 0x2211",
            "requires 1 stop bits",
        ),
        (
            "rx_pin: GPIO13\n  tx_pin: GPIO12\n  data_bits: 7",
            "transmit:\n    destination: 0x2211",
            "requires 8 data bits",
        ),
    ],
)
def test_tx_final_validation(
    uart_config: str, transmit_config: str, error: str | None, tmp_path: Path
) -> None:
    path = tmp_path / "validate.yaml"
    path.write_text(
        "esphome:\n  name: vbus-validation\nesp8266:\n  board: d1_mini\n"
        f"uart:\n  baud_rate: 9600\n  {uart_config}\nvbus:\n  id: bus\n  {transmit_config}\n"
        "button:\n  - platform: template\n    name: Request\n    on_press:\n"
        "      - vbus.request_bus:\n          id: bus\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(err) for err in result.errors]
    if error is None:
        assert not errors
    else:
        assert any(error in message for message in errors), errors


@pytest.mark.parametrize("tx_baud", [9600, 19200])
def test_separate_tx_uart_is_validated(tx_baud: int, tmp_path: Path) -> None:
    path = tmp_path / "separate.yaml"
    path.write_text(
        "esphome:\n  name: vbus-separate\nesp8266:\n  board: d1_mini\n"
        "uart:\n  - id: receive\n    baud_rate: 9600\n    rx_pin: GPIO13\n"
        f"  - id: transmit\n    baud_rate: {tx_baud}\n    tx_pin: GPIO1\n"
        "vbus:\n  uart_id: receive\n  transmit:\n    uart_id: transmit\n    destination: 0x2211\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(err) for err in result.errors]
    if tx_baud == 9600:
        assert not errors
    else:
        assert any("requires baud rate 9600" in message for message in errors), errors
