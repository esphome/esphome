"""Tests for the shared noise encryption key helpers."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.noise import decode_encryption_key, validate_encryption_key

KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="


def test_validate_encryption_key_roundtrips() -> None:
    assert validate_encryption_key(KEY) == KEY


@pytest.mark.parametrize("value", ["not-base64!!!", "AAECAw=="])
def test_validate_encryption_key_rejects_bad_input(value: str) -> None:
    with pytest.raises(cv.Invalid):
        validate_encryption_key(value)


def test_decode_encryption_key_returns_32_bytes() -> None:
    assert decode_encryption_key(KEY) == bytes(range(32))


def test_decode_encryption_key_rejects_invalid_base64() -> None:
    """The shared helper raises cv.Invalid, not binascii.Error."""
    with pytest.raises(cv.Invalid, match="base64"):
        decode_encryption_key("A")


def test_decode_encryption_key_rejects_short_decode() -> None:
    """a2b_base64 stops at embedded padding; a short decode must not become
    a zero padded PSK on the device."""
    with pytest.raises(cv.Invalid, match="32 bytes"):
        decode_encryption_key("AAECAw==")
