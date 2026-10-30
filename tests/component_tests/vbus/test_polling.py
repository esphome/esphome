"""The optional P2 scheduler validates its bounds without changing legacy RX/TX."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize(
    "settings",
    [
        {"value_ids": []},
        {"value_ids": [1], "update_interval": "499ms"},
        {"value_ids": [-1]},
        {"value_ids": [65536]},
        {"value_ids": [1, 1]},
        {"value_ids": [1], "rx_value_ids": [2]},
        {"value_ids": [1], "rx_value_ids": [1, 1]},
        {"value_ids": [1], "batch_size": 0},
        {"value_ids": [1], "batch_size": 17},
    ],
)
def test_invalid_polling(settings: dict) -> None:
    with pytest.raises(cv.Invalid):
        vbus.POLLING_SCHEMA(settings)


def test_polling_defaults_and_rx_exclusion() -> None:
    conf = vbus.POLLING_SCHEMA({"value_ids": [0, 65535], "rx_value_ids": [0]})
    assert conf["batch_size"] == 16
    assert conf["update_interval"].total_milliseconds == 60000
    assert conf["rx_value_ids"] == [0]


@pytest.mark.parametrize("lease", ["200ms", "440ms"])
def test_polling_lease_must_fit_a_confirmed_write(lease: str) -> None:
    with pytest.raises(cv.Invalid, match="must exceed"):
        vbus.TRANSMIT_SCHEMA(
            {
                "destination": 0x2211,
                "lease_duration": lease,
                "transactions": {"polling": {"value_ids": [1]}},
            }
        )


@pytest.mark.parametrize("command", [0, 0x0100, 0x0300, 0x0500, 0xFFFF])
def test_queued_write_rejects_non_write_commands(command: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.QUEUE_WRITE_SCHEMA(
            {"id": "bus", "value_id": 1, "value": 123, "command": command}
        )


def test_polling_owns_grant_automations() -> None:
    with pytest.raises(cv.Invalid, match="owns bus grants"):
        vbus.TRANSMIT_SCHEMA(
            {
                "destination": 0x2211,
                "transactions": {"polling": {"value_ids": [1]}},
                "on_bus_grant": {"then": []},
            }
        )


def test_polling_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("polling.yaml"))
    defines = {define.name for define in CORE.defines}
    assert {"USE_VBUS_TX", "USE_VBUS_TRANSACTIONS", "USE_VBUS_POLLING"} <= defines
    assert "VBUS_POLL_VALUE_COUNT" in defines
    assert "vbus::VBusPolling(" in code
    assert "test_bus->set_polling(" in code
    assert "legacy_bus->set_polling(" not in code
    assert "register_value(3, true)" in code
    assert "add_on_value_callback(" in code
    assert "queue_read(" in code
    assert "queue_write(" in code
    assert "suspend_polling()" in code
    assert "resume_polling()" in code


@pytest.mark.parametrize(
    "fixture",
    ["rx_only.yaml", "transmit.yaml", "datagram.yaml", "transactions.yaml"],
)
def test_polling_is_not_generated_for_previous_configurations(
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path(fixture))
    assert all(define.name != "USE_VBUS_POLLING" for define in CORE.defines)
    assert "VBusPolling" not in code


@pytest.mark.parametrize(
    "action", ["queue_read", "queue_write", "suspend_polling", "resume_polling"]
)
def test_queue_actions_require_polling_on_the_referenced_hub(
    action: str, tmp_path: Path
) -> None:
    path = tmp_path / "invalid.yaml"
    value = "\n          value: 42" if action == "queue_write" else ""
    value_id = (
        "\n          value_id: 1" if action in ("queue_read", "queue_write") else ""
    )
    path.write_text(
        "esphome:\n  name: queue-validation\nesp8266:\n  board: d1_mini\n"
        "uart:\n  baud_rate: 9600\n  rx_pin: GPIO13\n  tx_pin: GPIO12\n"
        "vbus:\n  id: bus\n  transmit:\n    destination: 0x2211\n    transactions: {}\n"
        "button:\n  - platform: template\n    name: Queued\n    on_press:\n"
        f"      - vbus.{action}:\n          id: bus{value_id}{value}\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any(
        "require transmit.transactions.polling" in str(err) for err in result.errors
    )


@pytest.mark.parametrize(
    "action", ["request_bus", "read_value", "send_datagram", "release_bus"]
)
def test_manual_actions_are_rejected_on_scheduler_owned_hub(
    action: str, tmp_path: Path
) -> None:
    extra = {
        "request_bus": "",
        "release_bus": "",
        "read_value": "\n          grant_id: 1\n          value_id: 1",
        "send_datagram": "\n          grant_id: 1\n          command: 0x0300",
    }[action]
    path = tmp_path / "invalid.yaml"
    path.write_text(
        "esphome:\n  name: mixed-validation\nesp8266:\n  board: d1_mini\n"
        "uart:\n  baud_rate: 9600\n  rx_pin: GPIO13\n  tx_pin: GPIO12\n"
        "vbus:\n  id: bus\n  transmit:\n    destination: 0x2211\n    transactions:\n"
        "      polling:\n        value_ids: [1]\n"
        "button:\n  - platform: template\n    name: Manual\n    on_press:\n"
        f"      - vbus.{action}:\n          id: bus{extra}\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("Polling owns TX" in str(err) for err in result.errors)


@pytest.mark.parametrize("schema", [vbus.QUEUE_READ_SCHEMA, vbus.QUEUE_WRITE_SCHEMA])
@pytest.mark.parametrize("value_id", [-1, 65536])
def test_queued_id_bounds(schema: cv.Schema, value_id: int) -> None:
    request = {"id": "bus", "value_id": value_id}
    if schema is vbus.QUEUE_WRITE_SCHEMA:
        request["value"] = 123
    with pytest.raises(cv.Invalid):
        schema(request)


@pytest.mark.parametrize("value", [-1, 0x100000000])
def test_queued_write_value_bounds(value: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus.QUEUE_WRITE_SCHEMA({"id": "bus", "value_id": 1, "value": value})


def test_multiple_hubs_allocate_maximum_per_hub_not_sum(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("polling_multiple.yaml"))
    assert "test_bus->set_polling(" in code
    assert "legacy_bus->set_polling(" in code
    count = next(
        define for define in CORE.defines if define.name == "VBUS_POLL_VALUE_COUNT"
    )
    assert str(count.value) == "3"
