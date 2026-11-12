"""Unit tests for the sx126x and sx127x packet interface platforms."""

import math

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


def _fsk(**options: object) -> dict:
    return {
        "modulation": "FSK",
        "packet_mode": True,
        "payload_length": 0,
        "sync_value": [0x12],
        "preamble_size": 4,
        "bitrate": 4800,
        **options,
    }


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
            _lora(payload_length=10),
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
            {
                **_fsk(modulation="FSK", packet_mode=False),
            },
            {},
            "packet_mode",
            {"packet_mode": True},
        ),
        (
            sx127x_packet_interface,
            "sx127x",
            {
                **_fsk(modulation="OOK"),
            },
            {CONF_MAX_PACKET_SIZE: 64},
            "at most 63",
            None,
        ),
        (
            sx127x_packet_interface,
            "sx127x",
            {
                **_fsk(sync_value=[]),
            },
            {},
            "sync_value",
            {"sync_value": [0x12]},
        ),
        (
            sx126x_packet_interface,
            "sx126x",
            _fsk(sync_value=[]),
            {},
            "sync_value",
            {"sync_value": [0x12, 0x34]},
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
    """Packets vary in length; FSK or OOK on an SX127x needs packet mode and a sync word, and holds at most 63 bytes;
    FSK on an SX126x needs a sync word.
    """
    config = {f"{domain}_id": "radio", CONF_MAX_PACKET_SIZE: 63, **link}
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
    sleeping = _lora(payload_length=0, rx_start=False)
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


def _final_validate(radio: dict, link: dict) -> None:
    config = sx126x_packet_interface.CONFIG_SCHEMA({"sx126x_id": "radio", **link})
    token = fv.full_config.set(_FullConfig("sx126x", {"payload_length": 0, **radio}))
    try:
        sx126x_packet_interface.FINAL_VALIDATE_SCHEMA(config)
    finally:
        fv.full_config.reset(token)


def test_packet_must_fit_transmit_time(setup_core) -> None:
    """The radios give up on a packet that takes longer than 4 s to send, so the largest packet must take less."""
    slow = _lora(spreading_factor=12)
    with pytest.raises(cv.Invalid, match=r"255 bytes takes 9019 ms.*to 7\d or less"):
        _final_validate(slow, {})
    _final_validate(slow, {CONF_MAX_PACKET_SIZE: 64})
    _final_validate(_lora(), {})


def test_reliability_timeout_follows_airtime(setup_core) -> None:
    """A reliability timeout must allow for the peer's largest packet and the acknowledgement."""
    needed = radio_link.reliability_timeout_ms(_lora(), 255, 0)
    assert needed == math.ceil(
        (
            radio_link.packet_airtime_ms(_lora(), 255)
            + radio_link.packet_airtime_ms(_lora(), radio_link.ACK_PACKET_SIZE)
        )
        * radio_link.MARGIN
    )
    assert radio_link.reliability_timeout_ms(_lora(), 255, 100) == needed + 100
    with pytest.raises(cv.Invalid, match=f"at least {needed}ms"):
        _final_validate(_lora(), {"reliability": {"timeout": "200ms"}})
    # Left out, the link works it out when generating code
    _final_validate(_lora(), {"reliability": {}})
    _final_validate(_lora(), {"reliability": {"timeout": f"{needed}ms"}})
    # The worked out timeout allows for any ack_delay, so one longer than 200 ms needs no timeout set
    _final_validate(_lora(spreading_factor=9), {"reliability": {"ack_delay": "250ms"}})
