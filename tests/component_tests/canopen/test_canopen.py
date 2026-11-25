"""Tests for the canopen component."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA, SDO_WRITE_SCHEMA
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("canopen.yaml"))
    assert "canopen::CANopenComponent(can_bus)" in main_cpp
    assert "canopen_hub->set_response_timeout(200);" in main_cpp
    assert "canopen_hub->add_on_heartbeat_callback(" in main_cpp
    assert "canopen_hub->add_on_emcy_callback(" in main_cpp
    defines = {define.name for define in CORE.defines}
    assert {"USE_CANOPEN_HEARTBEAT", "USE_CANOPEN_EMCY"} <= defines
    assert "-DCO_CONFIG_SDO_CLI=0x03" in CORE.build_flags
    assert "-DCO_CONFIG_SDO_CLI_BUFFER_SIZE=7" in CORE.build_flags
    assert "-DCO_CONFIG_FIFO=0x01" in CORE.build_flags
    assert (
        "::canopen_hub->read_sdo(32, 0x1018, 0x01, 4, canopen::sdo_read_reply"
        "<esphome__lambdaaction_id__fn, esphome__lambdaaction_id_2__fn>, nullptr);"
    ) in main_cpp
    assert (
        "::canopen_hub->write_sdo(32, 0x2000, 0x00, 1, 1, nullptr, nullptr);"
        in main_cpp
    )


@pytest.mark.parametrize(("timeout", "valid"), [("65535ms", True), ("65536ms", False)])
def test_response_timeout_fits_uint16(timeout: str, valid: bool) -> None:
    config = {"canbus_id": "can_bus", "response_timeout": timeout}
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)


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


def test_one_hub_per_canbus(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("same_bus.yaml")
    assert read_config({}) is None
    assert "one canopen hub per canbus" in capsys.readouterr().out


def test_hubs_on_two_buses(component_config_path: Callable[[str], Path]) -> None:
    CORE.config_path = component_config_path("two_buses.yaml")
    assert read_config({}) is not None
