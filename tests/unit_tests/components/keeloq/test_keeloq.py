"""The keeloq remote schema and the device key derived at build time."""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.keeloq import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    SEND_SCHEMA,
    _actions_have_channels,
    _channels,
    _command,
    _final_validate,
    _take_over_has_a_receiver,
    keeloq_decrypt,
    normal_learning_key,
)
import esphome.config_validation as cv
import esphome.final_validate as fv

# Made-up key, not a real manufacturer key
BASE = {
    "type": "jarolift",
    "manufacturer_key": "0x1A2B3C4D5E6F7081",
    "serial": 0x1234500,
}


@pytest.mark.parametrize(
    ("cipher", "key", "plain"),
    [
        (0xE44F4CDF, 0x5CEC6701B79FD949, 0xF741E2DB),
        (0x32CCD896, 0xEA125C50361424B1, 0xBB4A4F8E),
    ],
)
def test_decrypt_matches_published_vectors(cipher: int, key: int, plain: int) -> None:
    assert keeloq_decrypt(cipher, key) == plain


@pytest.mark.parametrize(
    ("serial", "device_key"),
    [(0x1234500, 0x109E92289CAB2950), (0x7654300, 0xDDFB7151FFB17E07)],
    ids=["serial", "neighbour"],
)
def test_device_key_matches_the_firmware_tests(serial: int, device_key: int) -> None:
    # keeloq_test.cpp uses these device keys; the frames of the first match the reference firmware.
    assert normal_learning_key(serial, 0x1A2B3C4D5E6F7081) == device_key


def test_jarolift_remote_is_accepted() -> None:
    config = CONFIG_SCHEMA(BASE)
    assert config["manufacturer_key"] == 0x1A2B3C4D5E6F7081


@pytest.mark.parametrize(
    "override",
    [
        {"type": "normal"},
        {"manufacturer_key": "0x1" + "0" * 16},
        {"serial": 0x10000000},
    ],
)
def test_invalid_remote_is_rejected(override: dict[str, Any]) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**BASE, **override})


@pytest.mark.parametrize(
    ("value", "expected"), [("Up", "up"), ("shade", "shade"), (0x8, 0x8)]
)
def test_commands_by_name_or_number(value: object, expected: object) -> None:
    assert _command(value) == expected


@pytest.mark.parametrize("value", ["open", 0x10])
def test_unknown_command_is_rejected(value: object) -> None:
    with pytest.raises(cv.Invalid, match="one of up, down, stop, learn, shade"):
        _command(value)


def test_on_frame_needs_a_receiver() -> None:
    with pytest.raises(cv.Invalid, match="on_frame needs receiver_id"):
        CONFIG_SCHEMA({**BASE, "on_frame": []})


def test_a_remote_needs_serial_or_receiver() -> None:
    config = {k: v for k, v in BASE.items() if k != "serial"}
    with pytest.raises(cv.Invalid, match="Set serial, or receiver_id"):
        CONFIG_SCHEMA(config)


def test_a_remote_without_serial_needs_an_id() -> None:
    # What it takes over is stored under its id, so an id ESPHome generates is not enough.
    without_serial = {k: v for k, v in BASE.items() if k != "serial"}
    hand_remote = {**without_serial, "receiver_id": "rx"}
    with pytest.raises(cv.Invalid, match="Set id on a remote without serial"):
        CONFIG_SCHEMA(hand_remote)
    CONFIG_SCHEMA({**hand_remote, "id": "hand_remote"})


def test_on_command_needs_a_receiver_and_valid_filters() -> None:
    with pytest.raises(cv.Invalid, match="on_command needs receiver_id"):
        CONFIG_SCHEMA({**BASE, "on_command": []})
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {**BASE, "receiver_id": "rx", "on_command": [{"channel": 17, "then": []}]}
        )


def _check_groups(remote: dict, action: dict) -> None:
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


def _check_learn(remote: dict) -> None:
    full = {
        "keeloq": [remote],
        # With channel 1, so only the serial can be missing.
        "button": [{"on_press": [{"keeloq.learn": {"id": "own", "channels": 0x0001}}]}],
    }
    token = fv.full_config.set(full)
    try:
        FINAL_VALIDATE_SCHEMA(remote)
    finally:
        fv.full_config.reset(token)


def test_learn_needs_a_serial() -> None:
    """keeloq.learn pairs the remote's own serial, so its remote needs serial."""
    _check_learn({"id": "own", "type": "jarolift", "serial": 0x1234500})
    # Without serial the remote takes over a hand remote instead.
    with pytest.raises(cv.Invalid, match="keeloq.learn needs serial on own"):
        _check_learn({"id": "own", "type": "jarolift", "receiver_id": "rx"})


@pytest.mark.parametrize(
    ("value", "expected"),
    [([1, 3], 0x0005), (0x0005, 0x0005), ([16], 0x8000), ([1, 1], 0x0001)],
)
def test_channels_as_list_or_mask(value: object, expected: int) -> None:
    # Channels 1 and 3 are bits 0 and 2.
    assert _channels(value) == expected


@pytest.mark.parametrize(
    ("value", "match"),
    [([17], None), ([], "set at least one channel"), (0, "set at least one channel")],
)
def test_invalid_channels_are_rejected(value: object, match: str | None) -> None:
    with pytest.raises(cv.Invalid, match=match):
        _channels(value)


def test_send_action_rejects_an_empty_channel_list() -> None:
    action = {"id": "shutter_remote", "command": "up", "channels": []}
    with pytest.raises(cv.Invalid, match="set at least one channel"):
        SEND_SCHEMA(action)


@pytest.mark.parametrize(
    ("second_serial", "valid"), [(0x1234501, True), (0x1234500, False)]
)
def test_remotes_need_distinct_serials(second_serial: int, valid: bool) -> None:
    # They would share one stored counter and send the same codes.
    remotes = [CONFIG_SCHEMA(BASE), CONFIG_SCHEMA({**BASE, "serial": second_serial})]
    token = fv.full_config.set({"keeloq": remotes})
    try:
        if valid:
            assert _final_validate(remotes[0]) is remotes[0]
        else:
            with pytest.raises(cv.Invalid, match="0x1234500 is used by more than one"):
                _final_validate(remotes[0])
    finally:
        fv.full_config.reset(token)


def test_a_remote_without_serial_is_left_out_of_the_serial_check() -> None:
    # It stores what it takes over under its id, not under a serial.
    without_serial = {k: v for k, v in BASE.items() if k != "serial"}
    remotes = [
        CONFIG_SCHEMA(BASE),
        CONFIG_SCHEMA({**without_serial, "id": "hand_remote", "receiver_id": "rx"}),
    ]
    token = fv.full_config.set({"keeloq": remotes})
    try:
        for remote in remotes:
            assert _final_validate(remote) is remote
    finally:
        fv.full_config.reset(token)
