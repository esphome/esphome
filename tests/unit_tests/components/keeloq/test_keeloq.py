"""The keeloq remote schema: only the Jarolift type for now, a 64-bit key, a 28-bit serial."""

from __future__ import annotations

import pytest

from esphome.components.keeloq import (
    CONFIG_SCHEMA,
    JAROLIFT_COMMANDS,
    SEND_SCHEMA,
    _channels,
    _command,
)
import esphome.config_validation as cv

# Made-up key, not a real manufacturer key
BASE = {
    "type": "jarolift",
    "manufacturer_key": "0x1A2B3C4D5E6F7081",
    "serial": 0x1234500,
}


def test_jarolift_remote_is_accepted() -> None:
    config = CONFIG_SCHEMA(BASE)
    assert config["manufacturer_key"] == 0x1A2B3C4D5E6F7081


def test_other_types_are_not_there_yet() -> None:
    with pytest.raises(cv.Invalid, match="jarolift"):
        CONFIG_SCHEMA({**BASE, "type": "normal"})


def test_key_has_64_bits() -> None:
    with pytest.raises(cv.Invalid, match="64-bit"):
        CONFIG_SCHEMA({**BASE, "manufacturer_key": "0x1" + "0" * 16})


def test_serial_has_28_bits() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**BASE, "serial": 0x10000000})


def test_commands_by_name_or_number() -> None:
    assert _command("Up") == "up"
    assert _command(0x8) == 0x8
    with pytest.raises(cv.Invalid, match="one of up, down, stop, learn, shade"):
        _command("open")


def test_shade_is_stop_sent_20_times() -> None:
    # The other names leave the count to the type: twice for Jarolift.
    assert JAROLIFT_COMMANDS["shade"] == (0x4, 20)
    assert JAROLIFT_COMMANDS["up"] == (0x8, None)


def test_channels_as_list_or_mask() -> None:
    # Channels 1 and 3 are bits 0 and 2: mask 0x0005.
    assert _channels([1, 3]) == 0x0005
    assert _channels(0x0005) == 0x0005
    assert _channels([16]) == 0x8000
    with pytest.raises(cv.Invalid, match="each channel only once"):
        _channels([1, 1])
    with pytest.raises(cv.Invalid):
        _channels([17])


def test_empty_channel_list_is_rejected() -> None:
    # No motor acts on a frame without a channel.
    with pytest.raises(cv.Invalid, match="set at least one channel"):
        _channels([])


def test_channel_mask_0_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="set at least one channel"):
        _channels(0)


def test_send_action_rejects_an_empty_channel_list() -> None:
    action = {"id": "shutter_remote", "command": "up", "channels": []}
    with pytest.raises(cv.Invalid, match="set at least one channel"):
        SEND_SCHEMA(action)
