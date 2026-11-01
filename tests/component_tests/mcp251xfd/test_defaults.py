"""MCP251XFD defaults for classic and FD operating modes."""

import pytest

from esphome.components.mcp251xfd import canbus as mcp251xfd
import esphome.config_validation as cv
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("key", ["default_fd", "default_bit_rate_switch"])
@pytest.mark.parametrize(
    "mode", [None, "NORMAL", "normal", "LOOPBACK", "LISTENONLY", "CLASSIC", "classic"]
)
def test_fd_defaults(
    key: str, mode: str | None, set_core_config: SetCoreConfigCallable
) -> None:
    """NORMAL defaults to FD without BRS, while CLASSIC rejects either enabled flag."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    options = {"can_id": 1, "cs_pin": 21}
    if mode is not None:
        options["mode"] = mode
    config = mcp251xfd.CONFIG_SCHEMA(options)
    assert config["default_fd"] is (mode is None or mode.upper() == "NORMAL")
    assert config["default_bit_rate_switch"] is False
    assert config["data_bit_rate"] == "2000KBPS"
    assert mcp251xfd.CONFIG_SCHEMA({**options, key: False})[key] is False
    if mode is not None and mode.upper() == "CLASSIC":
        with pytest.raises(cv.Invalid, match="CLASSIC"):
            mcp251xfd.CONFIG_SCHEMA({**options, key: True})
    else:
        assert mcp251xfd.CONFIG_SCHEMA({**options, key: True})[key] is True
