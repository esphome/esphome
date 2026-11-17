"""Tests for canbus.send payloads and the CAN FD options."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
import yaml

from esphome.components import canbus
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


def test_constant_payloads_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal send payloads share one table; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("payload_tables.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    assert [v for _, v in tables] == [
        "{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}"
    ]
    assert main_cpp.count(f"set_data_static({tables[0][0]}, 8);") == 2
    assert "set_data_template(" in main_cpp
    for setter in (
        "set_default_fd",
        "set_supports_fd",
        "set_default_bit_rate_switch",
        "set_fd",
        "set_bit_rate_switch",
        "set_pad_data",
    ):
        assert f"->{setter}(" not in main_cpp
    assert "USE_CANBUS_FD" not in {define.name for define in CORE.defines}


def _read_send_config(
    send: dict, tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> tuple[bool, str]:
    config = {
        "esphome": {
            "name": "test",
            "on_boot": [{"then": [{"canbus.send": send}]}],
        },
        "esp32": {"board": "esp32dev"},
        "canbus": [
            {
                "platform": "esp32_can",
                "id": "can_bus",
                "can_id": 4,
                "rx_pin": 4,
                "tx_pin": 5,
            }
        ],
    }
    path = tmp_path / "test.yaml"
    path.write_text(yaml.safe_dump(config), encoding="utf-8")
    CORE.config_path = path
    result = read_config({})
    return result is not None, capsys.readouterr().out


@pytest.mark.parametrize("size", [8, 9])
@pytest.mark.parametrize("make", [lambda n: [0x01] * n, lambda n: "a" * n])
def test_send_one_frame_limit(
    size: int,
    make: Callable[[int], list[int] | str],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Final validation limits classic CAN payloads to eight bytes."""
    valid, output = _read_send_config({"data": make(size)}, tmp_path, capsys)
    assert valid is (size <= 8), output
    if size > 8:
        assert "Classic CAN data must be at most 8 bytes" in output
        assert "canbus.send" in output


def test_classic_bus_rejects_fd_options(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """esp32_can does not support CAN FD, so its sends cannot ask for it."""
    valid, output = _read_send_config({"data": [1], "fd": False}, tmp_path, capsys)
    assert not valid
    assert "fd needs a canbus platform that supports CAN FD" in output


def test_only_the_fd_schema_has_bus_defaults() -> None:
    """Classic platforms keep the dev schema; CAN FD platforms extend CANBUS_FD_SCHEMA."""
    config = {"can_id": 4}
    assert canbus.CONF_DEFAULT_FD not in canbus.CANBUS_SCHEMA(config)
    fd_config = canbus.CANBUS_FD_SCHEMA(config)
    assert fd_config[canbus.CONF_DEFAULT_FD] is False
    assert fd_config[canbus.CONF_DEFAULT_PAD_DATA] is False


FD_BUS = {
    "default_fd": True,
    "default_bit_rate_switch": False,
    "default_pad_data": False,
}


@pytest.mark.parametrize(
    ("action", "bus", "error"),
    [
        ({"data": [0] * 64}, FD_BUS, None),
        ({"data": [0] * 13}, FD_BUS, "requires pad_data"),
        ({"data": [0] * 13, "pad_data": True}, FD_BUS, None),
        ({"data": [0] * 13}, {**FD_BUS, "default_pad_data": True}, None),
        ({"data": [0] * 9, "fd": False}, FD_BUS, "at most 8 bytes"),
        ({"data": [0], "fd": False, "bit_rate_switch": True}, FD_BUS, "requires fd"),
        ({"data": [], "remote_transmission_request": True}, FD_BUS, "cannot be used"),
        ({"data": [0] * 9}, {**FD_BUS, "default_fd": False}, "at most 8 bytes"),
    ],
)
def test_fd_send_validation(action: dict, bus: dict, error: str | None) -> None:
    """Bus defaults apply to sends that leave the option out."""
    action = {"remote_transmission_request": False, **action}
    if error is None:
        canbus.validate_send(action, bus)
    else:
        with pytest.raises(cv.Invalid, match=error):
            canbus.validate_send(action, bus)
