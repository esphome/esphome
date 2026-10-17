"""Tests for the tsdb configuration validators."""

from __future__ import annotations

import pytest

from esphome.components.tsdb import (
    _validate_file_name,
    _validate_min_free_bytes,
    _validate_mount_point,
    _validate_partition_size,
    _validate_size,
)
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (1024, 1024),
        ("1024", 1024),
        ("1024B", 1024),
        ("1KB", 1024),
        ("1kb", 1024),
        ("1KiB", 1024),
        ("2MB", 2 * 1024 * 1024),
        ("1.5KB", 1536),
        ("0x40000", 0x40000),
        (" 3KB ", 3072),
    ],
)
def test_validate_size_accepts_units(value: object, expected: int) -> None:
    assert _validate_size(value) == expected


@pytest.mark.parametrize("value", ["", "abc", "KB", "5GB", "10PB", "-1", "1.2.3"])
def test_validate_size_rejects_bad_text(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_size(value)


def test_validate_size_rejects_bool_and_nonpositive() -> None:
    with pytest.raises(cv.Invalid):
        _validate_size(True)
    with pytest.raises(cv.Invalid):
        _validate_size(0)
    with pytest.raises(cv.Invalid):
        _validate_size(-5)
    assert _validate_size(0, allow_zero=True) == 0


def test_validate_partition_size_requires_4k_alignment() -> None:
    assert _validate_partition_size("64KB") == 64 * 1024
    with pytest.raises(cv.Invalid):
        _validate_partition_size("65KB")


def test_validate_partition_size_has_64k_floor() -> None:
    with pytest.raises(cv.Invalid):
        _validate_partition_size("8KB")


@pytest.mark.parametrize("value", ["spiffs", "littlefs", "/littlefs/"])
def test_validate_mount_point_rejects_relative_or_trailing(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_mount_point(value)


def test_validate_mount_point_accepts_absolute() -> None:
    assert _validate_mount_point("/littlefs") == "/littlefs"


@pytest.mark.parametrize("value", ["a/b.tsdb", "a\\b.tsdb"])
def test_validate_file_name_rejects_paths(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_file_name(value)


def test_validate_file_name_accepts_plain() -> None:
    assert _validate_file_name("history.tsdb") == "history.tsdb"


def test_validate_min_free_bytes_allows_zero() -> None:
    assert _validate_min_free_bytes(0) == 0
    assert _validate_min_free_bytes("32KB") == 32 * 1024
