"""Tests for separate nominal and CAN FD data bitrate tables."""

from types import ModuleType

import pytest

from esphome.components import canbus
from esphome.components.mcp251xfd import canbus as mcp251xfd
from esphome.components.mcp2515 import canbus as mcp2515
import esphome.config_validation as cv
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("rate", range(2000, 8001, 1000))
def test_high_bitrates(rate: int) -> None:
    """High bitrates are available only for the CAN FD data phase."""
    data_rate = cv.enum(canbus.CAN_DATA_SPEEDS, upper=True)(f"{rate}kbps")
    assert data_rate == f"{rate}KBPS"
    assert canbus.get_rate(data_rate) == rate * 1000
    assert str(canbus.CAN_DATA_SPEEDS[data_rate]) == f"canbus::CAN_{rate}KBPS"
    with pytest.raises(cv.Invalid):
        canbus.CANBUS_SCHEMA({"can_id": 1, "bit_rate": f"{rate}kbps"})


@pytest.mark.parametrize("rate", ["9000KBPS", "1500KBPS"])
def test_unsupported_bitrates(rate: str) -> None:
    """Only explicitly defined bitrates are accepted."""
    with pytest.raises(cv.Invalid):
        canbus.CANBUS_SCHEMA({"can_id": 1, "bit_rate": rate})
    with pytest.raises(cv.Invalid):
        cv.enum(canbus.CAN_DATA_SPEEDS, upper=True)(rate)


@pytest.mark.parametrize("platform", [mcp2515, mcp251xfd])
@pytest.mark.parametrize("rate", range(2000, 8001, 1000))
def test_classic_controllers_reject_high_bitrates(
    platform: ModuleType, rate: int, set_core_config: SetCoreConfigCallable
) -> None:
    """Drivers must not configure high rates as nominal timing."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    with pytest.raises(cv.Invalid, match="bit_rate"):
        platform.CONFIG_SCHEMA({"can_id": 1, "bit_rate": f"{rate}KBPS", "cs_pin": 5})


def test_classic_bitrates_preserved() -> None:
    """Classic rates and the default remain available unchanged."""
    assert len(canbus.CAN_SPEEDS) == 21
    assert len(canbus.CAN_DATA_SPEEDS) == 28
    for rate, value in canbus.CAN_SPEEDS.items():
        assert canbus.CAN_DATA_SPEEDS[rate] is value
        assert canbus.CANBUS_SCHEMA({"can_id": 1, "bit_rate": rate})["bit_rate"] == rate
    assert canbus.CANBUS_SCHEMA({"can_id": 1})["bit_rate"] == "125KBPS"
