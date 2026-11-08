"""MCP251XFD defaults for classic and FD operating modes."""

import pytest

from esphome.components.mcp251xfd import canbus as mcp251xfd
import esphome.config_validation as cv

FD_DEFAULT_KEYS = ("default_fd", "default_bit_rate_switch", "default_pad_data")


@pytest.mark.parametrize("mode", [None, "NORMAL", "normal", "LOOPBACK", "LISTENONLY"])
def test_fd_defaults(mode: str | None) -> None:
    """NORMAL defaults to FD without BRS or padding; the other FD modes default to classic sends."""
    options = {"can_id": 1, "cs_pin": 21}
    if mode is not None:
        options["mode"] = mode
    config = mcp251xfd.CONFIG_SCHEMA(options)
    assert config["default_fd"] is (mode is None or mode.upper() == "NORMAL")
    assert config["default_bit_rate_switch"] is False
    assert config["default_pad_data"] is False
    assert config["data_bit_rate"] == "2000KBPS"


@pytest.mark.parametrize("key", (*FD_DEFAULT_KEYS, "data_bit_rate"))
def test_classic_mode_is_a_classic_bus(key: str) -> None:
    """CLASSIC has no FD options, so canbus treats it as a classic bus."""
    options = {"can_id": 1, "cs_pin": 21, "mode": "CLASSIC"}
    config = mcp251xfd.CONFIG_SCHEMA(options)
    assert not {*FD_DEFAULT_KEYS, "data_bit_rate"} & set(config)
    with pytest.raises(cv.Invalid, match=key):
        mcp251xfd.CONFIG_SCHEMA({**options, key: False})


def test_nominal_rate_rejects_data_rates() -> None:
    """The CAN FD data phase rates are only accepted as data_bit_rate."""
    with pytest.raises(cv.Invalid, match="bit_rate"):
        mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21, "bit_rate": "2000KBPS"})
