"""Tests for the sx1231 config validation and its platforms' final validation."""

from typing import Any

import pytest

from esphome.components import sx1231
from esphome.components.sx1231 import packet_interface, packet_transport
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_SPI_ID,
    PlatformFramework,
)
from esphome.core import ID
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

RADIO_ID = "radio"
BUS_ID = "spi_bus"


def _radio(**options: Any) -> ConfigType:
    """A radio config as the schema leaves it: FSK packet mode with a two byte sync word."""
    return {
        CONF_ID: ID(RADIO_ID, is_declaration=True, type="sx1231"),
        CONF_SPI_ID: ID(BUS_ID, is_declaration=False, type="spi"),
        "bandwidth": "125_0kHz",
        "bitrate": 4800,
        "bitsync": True,
        "dio0_pin": {"number": 10},
        "modulation": "FSK",
        "pa_pin": "BOOST",
        "pa_power": 13,
        "packet_mode": True,
        "payload_length": 0,
        "preamble_size": 4,
        "rx_start": True,
        "shaping": "NONE",
        "sync_value": [0x2D, 0xD4],
        **options,
    }


def _without(key: str, **options: Any) -> ConfigType:
    config = _radio(**options)
    del config[key]
    return config


def _full_config(radio: ConfigType, **bus_pins: int) -> Config:
    """A full config carrying one SPI bus and one radio, as the ID pass leaves it."""
    bus: ConfigType = {CONF_ID: ID(BUS_ID, is_declaration=True, type="spi"), **bus_pins}
    full = Config()
    full["spi"] = [bus]
    full["sx1231"] = [radio]
    full.declare_ids.append((bus[CONF_ID], ["spi", 0, CONF_ID]))
    full.declare_ids.append((radio[CONF_ID], ["sx1231", 0, CONF_ID]))
    return full


@pytest.fixture(autouse=True)
def esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)


@pytest.mark.parametrize(
    "config",
    [
        _radio(),
        _radio(modulation="OOK", shaping="CUTOFF_BR_X_2"),
        _radio(shaping="GAUSSIAN_BT_0_5", bandwidth="500_0kHz"),
        _radio(pa_pin="RFO", pa_power=13),
        _radio(pa_pin="BOOST", pa_power=-2),
        # GPIO16 can be DIO0 on chips other than the ESP8266
        _radio(dio0_pin={"number": 16}),
        # Continuous mode needs no DIO0 or bit synchronizer and sends no packets to time
        _without("dio0_pin", packet_mode=False, bitsync=False, preamble_size=65535),
        # The largest packet takes exactly the 3200 ms limit: (1851 + 2 + 1 + 64 + 2) bytes at 4800 bps
        _radio(preamble_size=1851),
        # A fixed length is timed instead of the largest packet
        _radio(preamble_size=1905, payload_length=10),
    ],
)
def test_config_valid(config: ConfigType) -> None:
    assert sx1231.validate_config(config) is config


@pytest.mark.parametrize(
    ("config", "message"),
    [
        (
            _radio(modulation="OOK", bandwidth="500_0kHz"),
            "500_0kHz is not available with OOK",
        ),
        (
            _radio(modulation="OOK", shaping="GAUSSIAN_BT_0_5"),
            "GAUSSIAN_BT_0_5 is only available with FSK",
        ),
        (_radio(shaping="CUTOFF_BR_X_1"), "CUTOFF_BR_X_1 is only available with OOK"),
        (_without("dio0_pin"), "'dio0_pin' required in packet mode"),
        (_radio(bitsync=False), "'bitsync' cannot be turned off in packet mode"),
        (_radio(sync_value=[0x2D, 0x00]), "sync value byte of 0x00"),
        (_radio(pa_pin="RFO", pa_power=14), "PA power must be <= 13 dbm"),
        (_radio(pa_pin="BOOST", pa_power=-3), "PA power must be >= -2 dbm"),
        (_radio(preamble_size=1852), "takes 3202 ms to send, and the limit is 3200 ms"),
        (
            _radio(preamble_size=1906, payload_length=10),
            "a packet of 10 bytes takes 3202 ms",
        ),
        # 4000 ms at the requested bitrate, more at the one the radio's divider gives
        (_radio(bitrate=68000, preamble_size=33931), "takes 4000 ms to send"),
    ],
)
def test_config_invalid(config: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        sx1231.validate_config(config)


@pytest.mark.parametrize(
    ("data", "expected"),
    [
        ("hi", b"hi"),
        ([0x01, 0xFF], [0x01, 0xFF]),
        ([0] * 64, [0] * 64),
        ("", None),
        ([], None),
        ([0] * 65, None),
        (5, None),
    ],
)
def test_packet_data(data: Any, expected: bytes | list[int] | None) -> None:
    if expected is None:
        with pytest.raises(cv.Invalid):
            sx1231.validate_packet_data(data)
    else:
        assert sx1231.validate_packet_data(data) == expected


def test_packet_data_schema_is_extractable() -> None:
    assert sx1231.validate_raw_data(SCHEMA_EXTRACT)([0x01]) == [0x01]


def test_esp8266_gpio16_rejected_as_dio0(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    sx1231.validate_config(_radio(dio0_pin={"number": 15}))
    sx1231.validate_config(_radio(dio0_pin={"number": 16}, packet_mode=False))
    with pytest.raises(cv.Invalid, match="GPIO16 has no interrupt"):
        sx1231.validate_config(_radio(dio0_pin={"number": 16}))


@pytest.mark.parametrize(
    ("bus_pins", "message"),
    [
        ({CONF_MISO_PIN: 12, CONF_MOSI_PIN: 13}, None),
        ({CONF_MOSI_PIN: 13}, "requires this spi bus to declare a miso_pin"),
        ({CONF_MISO_PIN: 12}, "requires this spi bus to declare a mosi_pin"),
    ],
)
def test_spi_bus_pins(
    set_core_config: SetCoreConfigCallable,
    bus_pins: dict[str, int],
    message: str | None,
) -> None:
    radio = _radio()
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(radio, **bus_pins)
    )
    if message is None:
        sx1231.FINAL_VALIDATE_SCHEMA(radio)
        return
    with pytest.raises(cv.Invalid, match=message):
        sx1231.FINAL_VALIDATE_SCHEMA(radio)


@pytest.mark.parametrize(
    ("radio", "ping_pong", "message"),
    [
        (_radio(), False, None),
        (_radio(), True, None),
        # Without ping-pong the transport only sends, so the radio may sleep between packets
        (_radio(rx_start=False), False, None),
        (_radio(packet_mode=False), False, "'packet_mode' must be true"),
        (_radio(payload_length=16), False, "'payload_length' must be 0"),
        (_radio(rx_start=False), True, "unless its 'rx_start' is true"),
    ],
)
def test_packet_transport_final_validate(
    set_core_config: SetCoreConfigCallable,
    radio: ConfigType,
    ping_pong: bool,
    message: str | None,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(radio))
    config: ConfigType = {
        "sx1231_id": ID(RADIO_ID, is_declaration=False, type="sx1231"),
        "ping_pong_enable": ping_pong,
    }
    if message is None:
        assert packet_transport.FINAL_VALIDATE_SCHEMA(config) is config
        return
    with pytest.raises(cv.Invalid, match=message):
        packet_transport.FINAL_VALIDATE_SCHEMA(config)


@pytest.mark.parametrize(
    ("radio", "message"),
    [
        (_radio(), None),
        # No layer on the link needs packets back, so the radio may sleep between packets
        (_radio(rx_start=False), None),
        (_radio(packet_mode=False), "'packet_mode' must be true"),
        (_radio(sync_value=[]), "The radio needs a 'sync_value'"),
        (_radio(payload_length=16), "'payload_length' must be 0"),
    ],
)
def test_packet_interface_final_validate(
    set_core_config: SetCoreConfigCallable,
    radio: ConfigType,
    message: str | None,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(radio))
    config: ConfigType = {
        "sx1231_id": ID(RADIO_ID, is_declaration=False, type="sx1231"),
        "max_packet_size": 64,
    }
    if message is None:
        assert packet_interface.FINAL_VALIDATE_SCHEMA(config) is config
        return
    with pytest.raises(cv.Invalid, match=message):
        packet_interface.FINAL_VALIDATE_SCHEMA(config)
