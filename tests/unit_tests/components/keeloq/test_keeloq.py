"""The keeloq remote schema and the device key derived at build time."""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.keeloq import (
    CONFIG_SCHEMA,
    SEND_SCHEMA,
    _channels,
    _command,
    _final_validate,
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


def test_device_key_matches_the_firmware_tests() -> None:
    # keeloq_test.cpp uses this device key; its frames match the reference firmware.
    assert normal_learning_key(0x1234500, 0x1A2B3C4D5E6F7081) == 0x109E92289CAB2950


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
