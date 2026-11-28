"""Validate and derive the microphone HTTP stream_id."""

import pytest

from esphome import config_validation as cv
from esphome.components.microphone_http_stream import (
    stream_id_from_microphone_id,
    validate_stream_id,
)


@pytest.mark.parametrize(
    ("microphone_id", "expected"),
    [
        ("kitchen_mic", "kitchen_mic"),
        ("Kitchen_Mic", "kitchen_mic"),
        ("_mic", "_mic"),
        ("a" * 40, "a" * 32),
    ],
)
def test_stream_id_from_microphone_id(microphone_id: str, expected: str) -> None:
    stream_id = stream_id_from_microphone_id(microphone_id)
    assert stream_id == expected
    assert validate_stream_id(stream_id) == stream_id


@pytest.mark.parametrize("stream_id", ["Mic", "2nd_mic", "-mic", "mic.1", "", "a" * 33])
def test_invalid_stream_id(stream_id: str) -> None:
    with pytest.raises(cv.Invalid, match="stream_id must be"):
        validate_stream_id(stream_id)
