"""Unit tests for the espnow packet interface platform."""

import pytest

from esphome import config_validation as cv
from esphome.components.espnow import packet_interface as espnow_packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE

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
