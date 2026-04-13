"""Tests for time component cron expression parsing."""

from esphome.components.time import _parse_cron_part


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
