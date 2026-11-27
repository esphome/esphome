"""Tests for the storage size validators."""

import pytest

from esphome import config_validation as cv
from esphome.components.storage import block_size, storage_size


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (4096, 4096),
        ("4096", 4096),
        ("64KB", 64 * 1024),
        ("64kb", 64 * 1024),
        ("64KiB", 64 * 1024),
        ("64K", 64 * 1024),
        ("1MB", 1024 * 1024),
        ("2 MB", 2 * 1024 * 1024),
        ("1GB", 1024 * 1024 * 1024),
    ],
)
def test_storage_size_accepts(value: object, expected: int) -> None:
    assert storage_size(value) == expected


@pytest.mark.parametrize("value", [0, -1, "0KB", "1.5MB", "10TB", "big", ""])
def test_storage_size_rejects(value: object) -> None:
    with pytest.raises(cv.Invalid):
        storage_size(value)


@pytest.mark.parametrize("value", [128, 512, "4KB", "64KB"])
def test_block_size_accepts(value: object) -> None:
    assert block_size(value) == storage_size(value)


@pytest.mark.parametrize("value", [64, 500, 3072, "128KB"])
def test_block_size_rejects(value: object) -> None:
    with pytest.raises(cv.Invalid):
        block_size(value)
