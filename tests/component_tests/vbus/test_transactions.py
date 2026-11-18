"""Managed P2 transactions require an explicitly configured transmitter and manager."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("timeout", ["0ms", "19ms", "1001ms"])
def test_invalid_response_timeout(timeout: str) -> None:
    with pytest.raises(cv.Invalid):
        vbus.TRANSACTIONS_SCHEMA({"response_timeout": timeout})


@pytest.mark.parametrize("command", [0, 0x0100, 0x0300, 0x0500, 0x0600, 0xFFFF])
def test_write_command_is_limited_to_standard_write_requests(command: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.WRITE_VALUE_SCHEMA(
            {
                "id": "bus",
                "grant_id": 1,
                "value_id": 2,
                "value": 123,
                "command": command,
            }
        )


@pytest.mark.parametrize("schema", [vbus.READ_VALUE_SCHEMA, vbus.WRITE_VALUE_SCHEMA])
@pytest.mark.parametrize(
    "field,value",
    [
        ("grant_id", 0),
        ("grant_id", -1),
        ("grant_id", 0x100000000),
        ("value_id", -1),
        ("value_id", 65536),
    ],
)
def test_request_bounds(schema: cv.Schema, field: str, value: int) -> None:
    request = {"id": "bus", "grant_id": 1, "value_id": 2}
    if schema is vbus.WRITE_VALUE_SCHEMA:
        request["value"] = 123
    request[field] = value
    with pytest.raises(cv.Invalid):
        schema(request)


@pytest.mark.parametrize("value", [-1, 0x100000000])
def test_write_value_has_a_32_bit_raw_payload(value: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.WRITE_VALUE_SCHEMA(
            {"id": "bus", "grant_id": 1, "value_id": 2, "value": value}
        )


def test_defaults_and_full_raw_value_range() -> None:
    conf = vbus.WRITE_VALUE_SCHEMA(
        {"id": "bus", "grant_id": 0xFFFFFFFF, "value_id": 0xFFFF, "value": 0xFFFFFFFF}
    )
    assert conf["command"] == 0x0200
    assert conf["value"] == 0xFFFFFFFF
    assert vbus.TRANSACTIONS_SCHEMA({})["response_timeout"].total_milliseconds == 100


def test_transactions_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("transactions.yaml"))
    defines = {define.name for define in CORE.defines}
    assert {"USE_VBUS_TX", "USE_VBUS_DATAGRAM", "USE_VBUS_TRANSACTIONS"} <= defines
    assert "vbus::VBusTransactions(test_bus," in code
    assert "test_bus->set_transactions(" in code
    assert "legacy_bus->set_transactions(" not in code
    assert "add_on_result_callback(" in code
    assert "vbus::TransactionStatus status" in code
    assert "read_value(" in code
    assert "write_value(" in code


@pytest.mark.parametrize(
    "fixture",
    ["rx_only.yaml", "transmit_rx_only.yaml", "transmit.yaml", "datagram.yaml"],
)
def test_manager_is_not_generated_without_transactions(
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path(fixture))
    assert all(define.name != "USE_VBUS_TRANSACTIONS" for define in CORE.defines)
    assert "VBusTransactions" not in code


@pytest.mark.parametrize("settings", ["", "transmit:\n    destination: 0x2211"])
def test_value_actions_require_transactions_on_the_referenced_hub(
    settings: str, tmp_path: Path
) -> None:
    path = tmp_path / "invalid.yaml"
    path.write_text(
        "esphome:\n  name: transactions-validation\nesp8266:\n  board: d1_mini\n"
        "uart:\n  baud_rate: 9600\n  rx_pin: GPIO13\n  tx_pin: GPIO12\n"
        f"vbus:\n  id: bus\n  {settings}\n"
        "button:\n  - platform: template\n    name: Read\n    on_press:\n"
        "      - vbus.read_value:\n          id: bus\n          grant_id: 1\n          value_id: 2\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("require transmit.transactions" in str(err) for err in result.errors), (
        result.errors
    )
