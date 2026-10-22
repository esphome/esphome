"""The keeloq remote schema: only the Jarolift type for now, a 64-bit key, a 28-bit serial."""

from __future__ import annotations

import pytest

from esphome.components.keeloq import (
    CONFIG_SCHEMA,
    JAROLIFT_COMMANDS,
    SEND_SCHEMA,
    _actions_have_channels,
    _channels,
    _command,
    _take_over_has_a_receiver,
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


def test_on_frame_needs_a_receiver() -> None:
    with pytest.raises(cv.Invalid, match="on_frame needs receiver_id"):
        CONFIG_SCHEMA({**BASE, "on_frame": []})


def test_a_remote_needs_serial_or_receiver() -> None:
    config = {k: v for k, v in BASE.items() if k != "serial"}
    with pytest.raises(cv.Invalid, match="Set serial, or receiver_id"):
        CONFIG_SCHEMA(config)


def test_on_command_needs_a_receiver_and_valid_filters() -> None:
    with pytest.raises(cv.Invalid, match="on_command needs receiver_id"):
        CONFIG_SCHEMA({**BASE, "on_command": []})
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {**BASE, "receiver_id": "rx", "on_command": [{"channel": 17, "then": []}]}
        )


def test_shade_is_stop_sent_20_times() -> None:
    assert JAROLIFT_COMMANDS["shade"] == (0x4, 20)
    assert JAROLIFT_COMMANDS["up"] == (0x8, None)


def _check_groups(remote: dict, action: dict) -> None:
    import esphome.final_validate as fv

    full = {"keeloq": [remote], "button": [{"on_press": [{"keeloq.send": action}]}]}
    token = fv.full_config.set(full)
    try:
        _actions_have_channels(remote)
    finally:
        fv.full_config.reset(token)


def test_a_frame_needs_channels_from_the_action_or_the_remote() -> None:
    with_serial = {"id": "own", "serial": 0x1234500}
    _check_groups(with_serial, {"id": "own", "channels": 0x0001})
    _check_groups({**with_serial, "channels": 0x0001}, {"id": "own"})
    # Without serial the channels may come from the hand remote.
    _check_groups({"id": "clone"}, {"id": "clone"})
    with pytest.raises(cv.Invalid, match="own has no channels"):
        _check_groups(with_serial, {"id": "own"})


def _check_take_over(remote: dict) -> None:
    import esphome.final_validate as fv

    full = {
        "keeloq": [remote],
        "button": [{"on_press": [{"keeloq.take_over": {"id": "own"}}]}],
    }
    token = fv.full_config.set(full)
    try:
        _take_over_has_a_receiver(remote)
    finally:
        fv.full_config.reset(token)


def test_take_over_needs_a_receiver() -> None:
    """keeloq.take_over listens for the hand remote, so its remote needs receiver_id."""
    _check_take_over({"id": "own", "serial": 0x1234500, "receiver_id": "rx"})
    # Also with channels on the remote, which skips the channels check.
    with pytest.raises(cv.Invalid, match="keeloq.take_over needs receiver_id on own"):
        _check_take_over({"id": "own", "serial": 0x1234500, "channels": 0x0001})


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
