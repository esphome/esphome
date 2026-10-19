"""Unit tests for the sx126x and sx127x packet interface platforms."""

import pytest

from esphome import config_validation as cv
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE, radio_link
from esphome.components.sx126x import packet_interface as sx126x_packet_interface
from esphome.components.sx127x import packet_interface as sx127x_packet_interface
import esphome.final_validate as fv


@pytest.mark.parametrize(
    ("schema", "radio_key"),
    [
        (sx126x_packet_interface.CONFIG_SCHEMA, "sx126x_id"),
        (sx127x_packet_interface.CONFIG_SCHEMA, "sx127x_id"),
    ],
)
def test_max_packet_size_is_radio_limit(
    setup_core, schema: cv.Schema, radio_key: str
) -> None:
    """A radio packet holds at most 255 bytes, which is also the default."""
    assert schema({radio_key: "radio"})[CONF_MAX_PACKET_SIZE] == 255
    with pytest.raises(cv.Invalid):
        schema({radio_key: "radio", CONF_MAX_PACKET_SIZE: 256})


def _lora(**options: object) -> dict:
    return {
        "modulation": "LORA",
        "spreading_factor": 7,
        "bandwidth": "125_0kHz",
        "coding_rate": "CR_4_5",
        "preamble_size": 8,
        "crc_enable": False,
        **options,
    }


@pytest.mark.parametrize(
    ("radio", "airtime_ms"),
    [
        # Worked by hand from the Semtech formula, for a 255 byte packet with an explicit header
        (_lora(spreading_factor=7), 394.5),
        (_lora(spreading_factor=7, crc_enable=True), 399.6),
        (_lora(spreading_factor=10), 2254.8),
        # A symbol takes over 16 ms, so the low data rate optimization lengthens the packet
        (_lora(spreading_factor=12), 9019.4),
    ],
)
def test_lora_airtime(radio: dict, airtime_ms: float) -> None:
    """The airtime of the largest LoRa packet follows the Semtech formula."""
    assert radio_link.packet_airtime_ms(radio, 255) == pytest.approx(
        airtime_ms, abs=0.5
    )


def test_max_busy_follows_airtime() -> None:
    """Listen before talk waits at least 2 s, and longer than the largest packet takes."""
    assert radio_link.max_busy_ms(_lora(), 255) == 2000
    assert radio_link.max_busy_ms(_lora(spreading_factor=12), 255) > 9019
    fsk = {
        "modulation": "FSK",
        "preamble_size": 4,
        "bitrate": 1200,
        "sync_value": [1, 2],
    }
    assert radio_link.packet_airtime_ms(fsk, 64) == pytest.approx(
        (4 + 2 + 1 + 64 + 2) * 8 / 1200 * 1000
    )


class _FullConfig(dict):
    """Enough of the full configuration for the radio links' final validation."""

    def __init__(self, domain: str, radio: dict) -> None:
        super().__init__({domain: [{"rx_start": True, **radio}]})
        self.domain = domain

    def get_path_for_id(self, id_: object) -> list:
        return [self.domain, 0, "id"]

    def get_config_for_path(self, path: list) -> dict:
        return self[path[0]][path[1]]


@pytest.mark.parametrize(
    ("module", "domain", "radio", "link", "match", "fix"),
    [
        (
            sx126x_packet_interface,
            "sx126x",
            {"payload_length": 10},
            {},
            "payload_length",
            {"payload_length": 0},
        ),
        (
            sx127x_packet_interface,
            "sx127x",
            _lora(payload_length=10),
            {},
            "payload_length",
            {"payload_length": 0},
        ),
        (
            sx127x_packet_interface,
            "sx127x",
            {"modulation": "FSK", "packet_mode": False, "payload_length": 0},
            {},
            "packet_mode",
            {"packet_mode": True},
        ),
        (
            sx127x_packet_interface,
            "sx127x",
            {"modulation": "OOK", "packet_mode": True, "payload_length": 0},
            {CONF_MAX_PACKET_SIZE: 65},
            "at most 64",
            None,
        ),
    ],
)
def test_radio_settings_checked(
    module: object,
    domain: str,
    radio: dict,
    link: dict,
    match: str,
    fix: dict | None,
) -> None:
    """Packets vary in length, and FSK or OOK on an SX127x needs packet mode and holds at most 64 bytes."""
    config = {f"{domain}_id": "radio", CONF_MAX_PACKET_SIZE: 64, **link}
    token = fv.full_config.set(_FullConfig(domain, radio))
    try:
        with pytest.raises(cv.Invalid, match=match):
            module.FINAL_VALIDATE_SCHEMA(config)
        if fix is not None:
            fv.full_config.set(_FullConfig(domain, {**radio, **fix}))
            assert module.FINAL_VALIDATE_SCHEMA(config)
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize(
    ("link", "match"),
    [
        ({}, None),
        ({"encryption": "secret"}, None),
        ({"reliability": {}}, "reliability"),
        (
            {
                "addressing": {
                    "address": 1,
                    "peers": [
                        {"id": "a", "destination": 2, "session": {}},
                        {"id": "b", "destination": 3},
                    ],
                }
            },
            "session",
        ),
    ],
)
def test_rx_start_needed_for_replies(setup_core, link: dict, match: str | None) -> None:
    """A radio that sleeps after sending suits only a link whose layers need no packets back."""
    sleeping = {"payload_length": 0, "rx_start": False}
    config = sx126x_packet_interface.CONFIG_SCHEMA({"sx126x_id": "radio", **link})
    token = fv.full_config.set(_FullConfig("sx126x", sleeping))
    try:
        if match is None:
            assert sx126x_packet_interface.FINAL_VALIDATE_SCHEMA(config)
            return
        with pytest.raises(cv.Invalid, match=f"'{match}'.*rx_start"):
            sx126x_packet_interface.FINAL_VALIDATE_SCHEMA(config)
    finally:
        fv.full_config.reset(token)
