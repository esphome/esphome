"""Tests for the canopen component (CANopenNode based)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA, SDO_WRITE_SCHEMA
import esphome.config_validation as cv
from esphome.core import CORE


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The hub configures CANopenNode's SDO client; the actions become SDO action objects with their triggers."""
    main_cpp = generate_main(component_config_path("canopen.yaml"))
    assert "canopen::CANopenComponent()" in main_cpp
    assert "canopen_hub->set_canbus(can_bus);" in main_cpp
    assert "canopen_hub->set_response_timeout(200);" in main_cpp
    assert "-DCO_CONFIG_SDO_CLI=0x03" in CORE.build_flags
    assert "-DCO_CONFIG_FIFO=0x01" in CORE.build_flags
    # canopen.sdo_read
    assert "canopen::SDOReadAction<>" in main_cpp
    assert "set_size(4);" in main_cpp
    assert "get_value_trigger()" in main_cpp
    assert "get_error_trigger()" in main_cpp
    # canopen.sdo_write
    assert "canopen::SDOWriteAction<>" in main_cpp
    assert "set_size(1);" in main_cpp


def test_defaults() -> None:
    config = CONFIG_SCHEMA({"canbus_id": "can_bus"})
    assert config["response_timeout"].total_milliseconds == 500


@pytest.mark.parametrize(("timeout", "valid"), [("65535ms", True), ("65536ms", False)])
def test_response_timeout_fits_uint16(timeout: str, valid: bool) -> None:
    config = {"canbus_id": "can_bus", "response_timeout": timeout}
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)


def test_no_node_options() -> None:
    """The hub isn't a CANopen node."""
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"canbus_id": "can_bus", "node_id": 0x10})


SDO_WRITE = {"id": "canopen_hub", "node_id": 1, "index": 0x1017}


@pytest.mark.parametrize(
    ("value", "size", "valid"),
    [
        (255, 1, True),
        (256, 1, False),
        (-128, 1, True),
        (-129, 1, False),
        (65535, 2, True),
        (0xFFFFFFFF, 4, True),
    ],
)
def test_sdo_write_value_fits_size(value: int, size: int, valid: bool) -> None:
    config = {**SDO_WRITE, "value": value, "size": size}
    if valid:
        SDO_WRITE_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="doesn't fit"):
            SDO_WRITE_SCHEMA(config)


def test_sdo_write_size() -> None:
    with pytest.raises(cv.Invalid):
        SDO_WRITE_SCHEMA({**SDO_WRITE, "value": 1, "size": 3})


@pytest.mark.parametrize("node_id", [0, 128])
def test_sdo_node_id_range(node_id: int) -> None:
    with pytest.raises(cv.Invalid):
        SDO_WRITE_SCHEMA({**SDO_WRITE, "node_id": node_id, "value": 1})
