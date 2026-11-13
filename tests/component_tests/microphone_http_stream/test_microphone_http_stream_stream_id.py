"""Derive the default microphone HTTP stream_id."""

import pytest

from esphome.components.microphone_http_stream import (
    stream_id_from_microphone_id,
    validate_stream_id,
)


@pytest.mark.parametrize(
    ("microphone_id", "expected"),
    [
        ("kitchen_mic", "kitchen_mic"),
        ("Kitchen.Mic", "kitchen_mic"),
        ("2nd_mic", "m2nd_mic"),
        ("_mic", "m_mic"),
        ("a" * 40, "a" * 32),
        ("1" + "b" * 40, "m1" + "b" * 30),
    ],
)
def test_stream_id_from_microphone_id(microphone_id: str, expected: str) -> None:
    stream_id = stream_id_from_microphone_id(microphone_id)
    assert stream_id == expected
    assert validate_stream_id(stream_id) == stream_id
