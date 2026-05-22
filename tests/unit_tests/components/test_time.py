"""Tests for time component cron expression parsing."""

import errno
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.time import _load_tzdata, _parse_cron_part, validate_tz


def test_star_slash_seconds() -> None:
    assert _parse_cron_part("*/10", 0, 60, {}) == {0, 10, 20, 30, 40, 50, 60}


def test_star_slash_minutes() -> None:
    assert _parse_cron_part("*/5", 0, 59, {}) == {
        0,
        5,
        10,
        15,
        20,
        25,
        30,
        35,
        40,
        45,
        50,
        55,
    }


def test_star_slash_hours() -> None:
    assert _parse_cron_part("*/2", 0, 23, {}) == {
        0,
        2,
        4,
        6,
        8,
        10,
        12,
        14,
        16,
        18,
        20,
        22,
    }


def test_star_slash_days_of_month() -> None:
    """days_of_month starts at 1, not 0."""
    assert _parse_cron_part("*/5", 1, 31, {}) == {1, 6, 11, 16, 21, 26, 31}


def test_question_slash() -> None:
    assert _parse_cron_part("?/10", 0, 60, {}) == {0, 10, 20, 30, 40, 50, 60}


def test_empty_offset_slash() -> None:
    """Empty offset defaults to min_value."""
    assert _parse_cron_part("/10", 0, 60, {}) == {0, 10, 20, 30, 40, 50, 60}


def test_empty_offset_slash_nonzero_min() -> None:
    """Empty offset defaults to min_value, not 0."""
    assert _parse_cron_part("/5", 1, 31, {}) == {1, 6, 11, 16, 21, 26, 31}


def test_numeric_offset_slash() -> None:
    assert _parse_cron_part("5/10", 0, 60, {}) == {5, 15, 25, 35, 45, 55}


def test_star() -> None:
    assert _parse_cron_part("*", 0, 59, {}) == set(range(0, 60))


def test_question() -> None:
    assert _parse_cron_part("?", 0, 59, {}) == set(range(0, 60))


def test_range() -> None:
    assert _parse_cron_part("1-5", 0, 59, {}) == {1, 2, 3, 4, 5}


def test_single_value() -> None:
    assert _parse_cron_part("30", 0, 59, {}) == {30}


def _mock_resources_with_error(error: Exception) -> MagicMock:
    """Return a mock of importlib.resources.files where read_bytes raises error."""
    leaf = MagicMock()
    leaf.read_bytes.side_effect = error
    package = MagicMock()
    package.__truediv__.return_value = leaf
    return MagicMock(return_value=package)


def test_load_tzdata_returns_none_on_windows_einval() -> None:
    """On Windows, opening a tzdata path with NTFS-illegal chars raises OSError(EINVAL).

    Regression test for crash when the system TZ resolves to a POSIX string like
    "<+08>-8" (Asia/Shanghai, IST, etc.) and is fed back into _load_tzdata by
    validate_tz to check whether it is also a valid IANA key.
    """
    err = OSError(errno.EINVAL, "Invalid argument")
    with patch(
        "esphome.components.time.resources.files",
        _mock_resources_with_error(err),
    ):
        assert _load_tzdata("<+08>-8") is None


def test_load_tzdata_propagates_unexpected_oserror() -> None:
    """Unrelated OSErrors (e.g. PermissionError) must not be swallowed."""
    with (
        patch(
            "esphome.components.time.resources.files",
            _mock_resources_with_error(
                PermissionError(errno.EACCES, "Permission denied")
            ),
        ),
        pytest.raises(PermissionError),
    ):
        _load_tzdata("Some/Zone")


def test_load_tzdata_returns_none_on_file_not_found() -> None:
    """Existing behavior: missing tz file returns None rather than raising."""
    with patch(
        "esphome.components.time.resources.files",
        _mock_resources_with_error(FileNotFoundError()),
    ):
        assert _load_tzdata("Not/A/Zone") is None


def test_validate_tz_accepts_posix_string_when_read_bytes_raises_einval() -> None:
    """validate_tz must not crash when _load_tzdata hits the Windows EINVAL path.

    Simulates the Windows case where the auto-detected POSIX TZ string is fed
    back through _load_tzdata and the underlying read_bytes raises errno 22.
    """
    with patch(
        "esphome.components.time.resources.files",
        _mock_resources_with_error(OSError(errno.EINVAL, "Invalid argument")),
    ):
        assert validate_tz("<+08>-8") == "<+08>-8"
