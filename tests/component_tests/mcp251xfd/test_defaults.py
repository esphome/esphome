"""MCP251XFD defaults for classic and FD operating modes."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
import yaml

from esphome.components.mcp251xfd import canbus as mcp251xfd
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE

from ..types import SetCoreConfigCallable

FD_DEFAULT_KEYS = ("default_fd", "default_bit_rate_switch", "default_pad_data")


@pytest.fixture(autouse=True)
def _set_esp32_core_config(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )


@pytest.mark.parametrize("key", FD_DEFAULT_KEYS)
@pytest.mark.parametrize("mode", [None, "NORMAL", "normal", "LOOPBACK", "LISTENONLY"])
def test_fd_defaults(key: str, mode: str | None) -> None:
    """NORMAL defaults to FD without BRS or padding; the other FD modes default to classic sends."""
    options = {"can_id": 1, "cs_pin": 21}
    if mode is not None:
        options["mode"] = mode
    config = mcp251xfd.CONFIG_SCHEMA(options)
    assert config["default_fd"] is (mode is None or mode.upper() == "NORMAL")
    assert config["default_bit_rate_switch"] is False
    assert config["default_pad_data"] is False
    assert config["data_bit_rate"] == "2000KBPS"
    assert mcp251xfd.CONFIG_SCHEMA({**options, key: False})[key] is False
    assert mcp251xfd.CONFIG_SCHEMA({**options, key: True})[key] is True


@pytest.mark.parametrize("key", FD_DEFAULT_KEYS)
@pytest.mark.parametrize("mode", ["CLASSIC", "classic"])
def test_classic_mode_is_a_classic_bus(key: str, mode: str) -> None:
    """CLASSIC drops the FD defaults, so canbus treats it as a classic bus, and rejects enabling one."""
    options = {"can_id": 1, "cs_pin": 21, "mode": mode}
    for config in (
        mcp251xfd.CONFIG_SCHEMA(options),
        mcp251xfd.CONFIG_SCHEMA({**options, key: False}),
    ):
        assert not set(FD_DEFAULT_KEYS) & set(config)
    with pytest.raises(cv.Invalid, match="CLASSIC"):
        mcp251xfd.CONFIG_SCHEMA({**options, key: True})


@pytest.mark.parametrize("rate", range(2000, 8001, 1000))
def test_nominal_rate_rejects_data_rates(rate: int) -> None:
    """The CAN FD data phase rates are only accepted as data_bit_rate."""
    with pytest.raises(cv.Invalid, match="bit_rate"):
        mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21, "bit_rate": f"{rate}KBPS"})


def test_fd_capability_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only the FD bus claims CAN FD support; the CLASSIC bus keeps the classic default."""
    main = generate_main(component_config_path("mcp251xfd_timing.yaml"))
    assert "fd_bus->set_supports_fd(true);" in main
    assert "classic_bus->set_supports_fd(" not in main
    assert "classic_bus->set_default_" not in main
    assert "USE_CANBUS_FD" in {define.name for define in CORE.defines}


@pytest.mark.parametrize(
    ("mode", "send", "error"),
    [
        ("CLASSIC", {"fd": True}, "fd needs a canbus platform that supports CAN FD"),
        ("CLASSIC", {"data": [0] * 9}, "at most 8"),
        ("CLASSIC", {}, None),
        ("NORMAL", {"data": [0] * 12}, None),
        ("NORMAL", {"data": [0] * 9}, "pad_data"),
        ("LOOPBACK", {"data": [0] * 9}, "at most 8"),
        ("LOOPBACK", {"data": [0] * 12, "fd": True}, None),
    ],
)
def test_send_validation(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    mode: str,
    send: dict,
    error: str | None,
) -> None:
    """canbus.send is held to what the bus mode can send."""
    spi_package = Path(
        "tests/test_build_components/common/spi/esp32-idf.yaml"
    ).resolve()
    config = {
        "esphome": {
            "name": "test",
            "on_boot": [
                {"then": [{"canbus.send": {"canbus_id": "bus", "data": [1], **send}}]}
            ],
        },
        "esp32": {"board": "esp32dev"},
        "canbus": [
            {
                "platform": "mcp251xfd",
                "id": "bus",
                "can_id": 2,
                "cs_pin": 21,
                "mode": mode,
            }
        ],
    }
    path = tmp_path / "test.yaml"
    path.write_text(
        yaml.safe_dump(config) + f"\npackages:\n  spi: !include {spi_package}\n",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = read_config({})
    output = capsys.readouterr().out
    assert (result is None) is (error is not None), output
    if error is not None:
        assert error in output


def test_send_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Static flags, lambdas and the bus defaults reach C++."""
    main = generate_main(component_config_path("send_fd.yaml"))
    assert "fd_bus->set_default_fd(true);" in main
    assert "fd_bus->set_default_bit_rate_switch(true);" in main
    assert "fd_bus->set_default_pad_data(true);" in main
    assert main.count("->set_fd(") == 4
    assert main.count("->set_bit_rate_switch(") == 3
    assert main.count("->set_pad_data(") == 3
    for setter in ("set_fd", "set_bit_rate_switch", "set_pad_data"):
        callbacks = re.findall(rf"->{setter}\((.*?)\);", main, re.DOTALL)
        assert sum("return true;" in callback for callback in callbacks) == (
            3 if setter == "set_fd" else 2
        )
        assert sum("return false;" in callback for callback in callbacks) == 1
