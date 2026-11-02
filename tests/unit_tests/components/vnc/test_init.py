"""Tests for the vnc password validator, auth key and source filter."""

import logging

import pytest

from esphome.components import vnc
import esphome.config_validation as cv
from esphome.core import CORE, Define


def _reverse_bits(byte: int) -> int:
    return sum(((byte >> bit) & 1) << (7 - bit) for bit in range(8))


def test_auth_key_known_password() -> None:
    assert vnc.vnc_auth_key("secret12") == [
        0xCE,
        0xA6,
        0xC6,
        0x4E,
        0xA6,
        0x2E,
        0x8C,
        0x4C,
    ]


@pytest.mark.parametrize(
    "password",
    ["a", "ab", "1234567", "\x7f\x80", "é", "pässwö"],
)
def test_auth_key_short_password_is_zero_padded(password: str) -> None:
    raw = password.encode("utf-8")
    key = vnc.vnc_auth_key(password)

    assert len(key) == 8
    assert key[: len(raw)] == [_reverse_bits(byte) for byte in raw]
    assert key[len(raw) :] == [0] * (8 - len(raw))


def test_auth_key_bit_reverses_every_byte() -> None:
    assert vnc.vnc_auth_key("\x01\x0f\x55\x7f\x7e\x2a\x40\x10") == [
        0x80,
        0xF0,
        0xAA,
        0xFE,
        0x7E,
        0x54,
        0x02,
        0x08,
    ]


@pytest.mark.parametrize(
    ("password", "used"),
    [
        ("secret1234", "secret12"),
        ("abcdefghijklmnop", "abcdefgh"),
    ],
)
def test_auth_key_long_password_is_truncated(password: str, used: str) -> None:
    assert vnc.vnc_auth_key(password) == vnc.vnc_auth_key(used)


def test_auth_key_truncates_utf8_bytes_not_characters() -> None:
    """The password is cut at 8 bytes like TightVNC does, even inside a character."""
    password = "aéééé"
    assert vnc.vnc_auth_key(password) == [
        _reverse_bits(byte) for byte in password.encode("utf-8")[:8]
    ]


@pytest.mark.parametrize("password", ["a", "secret12", "éééé"])
def test_validate_password_up_to_8_bytes_no_warning(
    password: str, caplog: pytest.LogCaptureFixture
) -> None:
    with caplog.at_level(logging.WARNING):
        assert vnc.validate_password(password) == password
    assert caplog.records == []


@pytest.mark.parametrize("password", ["secret123", "ééééé", "a" * 64])
def test_validate_password_over_8_bytes_warns(
    password: str, caplog: pytest.LogCaptureFixture
) -> None:
    """The value is kept as written; only the key uses the first 8 bytes."""
    with caplog.at_level(logging.WARNING):
        assert vnc.validate_password(password) == password
    assert len(caplog.records) == 1
    assert caplog.records[0].levelno == logging.WARNING
    assert "only the first 8 are used" in caplog.records[0].getMessage()


def test_validate_password_rejects_empty() -> None:
    with pytest.raises(cv.Invalid, match="must not be empty"):
        vnc.validate_password("")


@pytest.mark.parametrize("value", [None, 12345678, 1.5, True, ["secret"], {}])
def test_validate_password_rejects_non_strings(value: object) -> None:
    with pytest.raises(cv.Invalid):
        vnc.validate_password(value)


def test_password_schema_is_sensitive() -> None:
    schema = vnc.CONFIG_SCHEMA.validators[0].schema
    validator = next(
        value for key, value in schema.items() if str(key) == vnc.CONF_PASSWORD
    )
    assert isinstance(validator, cv.SensitiveValidator)


def test_des_source_compiled_only_with_auth() -> None:
    CORE.defines = set()
    assert vnc.FILTER_SOURCE_FILES() == ["vnc_des.cpp"]

    CORE.defines = {Define("USE_VNC_AUTH")}
    assert vnc.FILTER_SOURCE_FILES() == []
