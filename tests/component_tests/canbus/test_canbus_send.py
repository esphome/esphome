"""Schema and code generation tests for classic and FD send actions."""

from collections.abc import Callable
from pathlib import Path
import re
from typing import Any

import pytest

from esphome.components import canbus
from esphome.components.esp32_can import canbus as esp32_can
from esphome.components.mcp251xfd import canbus as mcp251xfd
from esphome.components.mcp2515 import canbus as mcp2515
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import Lambda

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("size", [0, 8, 9, 12, 16, 20, 24, 32, 48, 64])
@pytest.mark.parametrize("make", [lambda n: [1] * n, lambda n: "a" * n])
def test_unspecified_fd(size: int, make: Callable[[int], Any]) -> None:
    """An absent FD flag is inherited, not treated as classic."""
    config = canbus.CANBUS_SEND_ACTION_SCHEMA({"data": make(size)})
    assert len(config["data"]) == size
    for key in ("fd", "bit_rate_switch", "pad_data"):
        assert key not in config


@pytest.mark.parametrize(
    ("options", "message"),
    [
        ({"fd": False, "bit_rate_switch": True}, "bit_rate_switch"),
        (
            {"fd": True, "remote_transmission_request": True},
            "remote_transmission_request",
        ),
        ({"data": [0] * 65}, "at most 64"),
        ({"fd": False, "data": [0] * 9}, "at most 8"),
        ({"fd": False, "pad_data": True, "data": [0] * 9}, "at most 8"),
        ({"fd": True, "pad_data": False, "data": [0] * 9}, "pad_data"),
        ({"fd": True, "pad_data": False, "data": "a" * 63}, "pad_data"),
    ],
)
def test_invalid_static_send(options: dict[str, Any], message: str) -> None:
    """Reject combinations that cannot become valid at runtime."""
    with pytest.raises(cv.Invalid, match=message):
        canbus.CANBUS_SEND_ACTION_SCHEMA({"data": [1], **options})


@pytest.mark.parametrize("size", [*range(9), 12, 16, 20, 24, 32, 48, 64])
def test_fd_dlc_lengths(size: int) -> None:
    """Every directly representable FD payload length is accepted."""
    canbus.CANBUS_SEND_ACTION_SCHEMA({"fd": True, "data": [0] * size})


@pytest.mark.parametrize("size", [9, 10, 11, 13, 17, 21, 25, 33, 49, 63])
@pytest.mark.parametrize("pad", [True, Lambda("return true;")])
def test_fd_padding(size: int, pad: bool | Lambda) -> None:
    """Explicit or runtime padding can make non-DLC lengths valid."""
    canbus.CANBUS_SEND_ACTION_SCHEMA({"fd": True, "pad_data": pad, "data": [0] * size})


@pytest.mark.parametrize(
    "options",
    [
        {"fd": Lambda("return false;"), "bit_rate_switch": True},
        {"fd": False, "bit_rate_switch": Lambda("return false;")},
        {"fd": Lambda("return true;"), "remote_transmission_request": True},
        {"fd": True, "data": Lambda("return {1, 2, 3};")},
        {"fd": True, "data": [0] * 9},
        {"bit_rate_switch": True},
        {"fd": Lambda("return true;"), "data": [0] * 9},
    ],
)
def test_runtime_send_validation(options: dict[str, Any]) -> None:
    """Unresolved flags and payloads are checked by the C++ action."""
    canbus.CANBUS_SEND_ACTION_SCHEMA({"data": [1], **options})


@pytest.mark.parametrize("driver", [canbus, mcp2515, esp32_can, mcp251xfd])
@pytest.mark.parametrize("pad", [None, False, True])
def test_padding_default(
    driver: Any, pad: bool | None, set_core_config: SetCoreConfigCallable
) -> None:
    """Padding defaults are false and can be configured independently of FD support."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    schema = canbus.CANBUS_SCHEMA if driver is canbus else driver.CONFIG_SCHEMA
    options = {"can_id": 1}
    if driver in (mcp2515, mcp251xfd):
        options["cs_pin"] = 21
    elif driver is esp32_can:
        options.update(rx_pin=4, tx_pin=5)
    if pad is not None:
        options["default_pad_data"] = pad
    assert schema(options)["default_pad_data"] is (pad is True)


@pytest.mark.parametrize("key", ["default_fd", "default_bit_rate_switch"])
@pytest.mark.parametrize("driver", [canbus, mcp2515, esp32_can])
def test_classic_defaults(
    key: str, driver: Any, set_core_config: SetCoreConfigCallable
) -> None:
    """Classic-only drivers inherit false-only default validators."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    schema = canbus.CANBUS_SCHEMA if driver is canbus else driver.CONFIG_SCHEMA
    options = {"can_id": 1}
    if driver is mcp2515:
        options["cs_pin"] = 21
    elif driver is esp32_can:
        options.update(rx_pin=4, tx_pin=5)
    assert schema({**options, key: "false"})[key] is False
    with pytest.raises(cv.Invalid, match=key):
        schema({**options, key: True})


def test_send_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Static flags, lambdas, inherited defaults and identifiers reach C++ correctly."""
    main = generate_main(component_config_path("send_fd.yaml"))
    assert "fd_bus->set_default_fd(true);" in main
    assert "fd_bus->set_default_bit_rate_switch(true);" in main
    assert "fd_bus->set_default_pad_data(true);" in main
    assert "set_default_fd(false)" not in main
    assert "set_default_bit_rate_switch(false)" not in main
    assert "set_default_pad_data(false)" not in main
    assert main.count("->set_fd(") == 4
    assert main.count("->set_bit_rate_switch(") == 3
    assert main.count("->set_pad_data(") == 3
    for setter in ("set_fd", "set_bit_rate_switch", "set_pad_data"):
        callbacks = re.findall(rf"->{setter}\((.*?)\);", main, re.DOTALL)
        assert all("() -> bool" in callback for callback in callbacks)
        assert sum("return true;" in callback for callback in callbacks) == (
            3 if setter == "set_fd" else 2
        )
        assert sum("return false;" in callback for callback in callbacks) == 1
    assert main.count("->set_can_id(291);") == 1
    assert main.count("->set_use_extended_id(false);") == 1
    assert main.count("->set_use_extended_id(true);") == 1


@pytest.mark.parametrize(
    ("can_id", "extended", "valid"),
    [
        (0, False, True),
        (0x7FF, False, True),
        (0x800, False, False),
        (0x1FFFFFFF, True, True),
        (0x20000000, True, False),
        (-1, False, False),
    ],
)
def test_send_identifiers(can_id: int, extended: bool, valid: bool) -> None:
    """The existing standard and extended identifier bounds are preserved."""
    options = {"can_id": can_id, "use_extended_id": extended, "data": [1]}
    if valid:
        assert canbus.CANBUS_SEND_ACTION_SCHEMA(options)["can_id"] == can_id
    else:
        with pytest.raises(cv.Invalid):
            canbus.CANBUS_SEND_ACTION_SCHEMA(options)


def test_utf8_byte_length() -> None:
    """Payload limits apply to encoded bytes, not character counts."""
    with pytest.raises(cv.Invalid, match="at most 64"):
        canbus.CANBUS_SEND_ACTION_SCHEMA({"data": "é" * 33})
    with pytest.raises(cv.Invalid, match="at most 8"):
        canbus.CANBUS_SEND_ACTION_SCHEMA({"fd": False, "data": "é" * 5})
