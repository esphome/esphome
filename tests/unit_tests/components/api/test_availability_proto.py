"""Tests for the native API availability wire contract."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[4]
PROTO_TEXT = (ROOT / "esphome/components/api/api.proto").read_text()


def _extract_message_body(message_name: str) -> str:
    """Return a top-level protobuf message body."""
    match = re.search(
        rf"message {message_name} \{{(?P<body>.*?)\n\}}", PROTO_TEXT, re.DOTALL
    )
    assert match is not None, f"message {message_name} not found"
    return match.group("body")


def test_device_state_response_wire_contract() -> None:
    """Device availability uses the next free server message ID."""
    body = _extract_message_body("DeviceStateResponse")
    assert "option (id) = 149;" in body
    assert "option (source) = SOURCE_SERVER;" in body
    assert 'option (ifdef) = "USE_DEVICES";' in body
    assert "option (no_delay) = true;" in body
    assert "uint32 device_id = 1;" in body
    assert "bool available = 2;" in body
