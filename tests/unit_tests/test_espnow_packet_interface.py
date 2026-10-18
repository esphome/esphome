"""Unit tests for the espnow packet interface platform."""

import pytest

from esphome import config_validation as cv
from esphome.components.espnow import packet_interface as espnow_packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
import esphome.final_validate as fv

ADDRESSING = {"address": 1, "destination": 2}


def test_broadcast_takes_addressing(setup_core) -> None:
    """Broadcast is the default, and an addressing layer tells the devices apart."""
    config = espnow_packet_interface.CONFIG_SCHEMA({"addressing": ADDRESSING})
    assert str(config["peer_address"]) == "FF:FF:FF:FF:FF:FF"
    assert config[CONF_MAX_PACKET_SIZE] == 250


def test_peer_rejects_addressing(setup_core) -> None:
    """Packets to one peer are already addressed."""
    assert espnow_packet_interface.CONFIG_SCHEMA({"peer_address": "11:22:33:44:55:66"})
    with pytest.raises(cv.Invalid, match="addressing"):
        espnow_packet_interface.CONFIG_SCHEMA(
            {"peer_address": "11:22:33:44:55:66", "addressing": ADDRESSING}
        )


def test_broadcast_needs_addressing_for_two_device_layers(setup_core) -> None:
    """Every device hears a broadcast link, so reliability needs an addressing layer there."""
    with pytest.raises(cv.Invalid, match="every device hears this link"):
        espnow_packet_interface.CONFIG_SCHEMA({"reliability": None})
    assert espnow_packet_interface.CONFIG_SCHEMA(
        {"reliability": None, "addressing": ADDRESSING}
    )
    assert espnow_packet_interface.CONFIG_SCHEMA(
        {"peer_address": "11:22:33:44:55:66", "reliability": None}
    )


def test_packet_size_within_payload_size(setup_core) -> None:
    """A packet cannot be larger than the espnow component's max_payload_size."""
    token = fv.full_config.set({"espnow": {"max_payload_size": 250}})
    try:
        assert espnow_packet_interface._validate_packet_size(
            {CONF_MAX_PACKET_SIZE: 250}
        )
        with pytest.raises(cv.Invalid, match="max_payload_size"):
            espnow_packet_interface._validate_packet_size({CONF_MAX_PACKET_SIZE: 251})
    finally:
        fv.full_config.reset(token)
