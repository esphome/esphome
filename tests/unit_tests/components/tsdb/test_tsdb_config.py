"""Tests for the tsdb configuration validators."""

from __future__ import annotations

import pytest

from esphome.components.tsdb import (
    CONF_COLUMNS,
    CONF_MAX_FILE_SIZE,
    CONF_MIN_FREE_BYTES,
    CONF_NAME,
    CONF_OFFSET,
    CONF_PARTITION_SIZE,
    CONF_SCALE_FACTOR,
    CONF_SENSOR,
    _validate_config,
    _validate_file_name,
    _validate_memory,
    _validate_min_free_bytes,
    _validate_mount_point,
    _validate_partition_size,
    _validate_size,
)
from esphome.components.tsdb.sensor import (
    CONF_AVERAGE,
    CONF_COLUMN,
    CONF_MIN,
    _final_validate,
)
import esphome.config_validation as cv
from esphome.const import CONF_TIME_ID
import esphome.final_validate as fv


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
        ("0.5KB", 512),
        ("0x40000", 0x40000),
        (" 3KB ", 3072),
    ],
)
def test_validate_size_accepts_units(value: object, expected: int) -> None:
    assert _validate_size(value) == expected


@pytest.mark.parametrize(
    "value",
    ["", "abc", "KB", "5GB", "10PB", "-1", "1.2.3", "1.1KB", "1.0001KB", "0.1MB"],
)
def test_validate_size_rejects_bad_text(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_size(value)


def test_validate_size_rejects_fractional_bytes() -> None:
    # 1.0001KB is 1024.1024 bytes; truncating it would silently store 1024.
    with pytest.raises(cv.Invalid, match="whole number of bytes"):
        _validate_size("1.0001KB")


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


def _column(name: str = "value", **kwargs: object) -> dict:
    column: dict = {
        CONF_NAME: name,
        CONF_SCALE_FACTOR: 1.0,
        CONF_OFFSET: 0.0,
        CONF_SENSOR: "source",
    }
    column.update(kwargs)
    return column


def _config(**kwargs: object) -> dict:
    config: dict = {
        CONF_COLUMNS: [_column()],
        CONF_PARTITION_SIZE: 512 * 1024,
        CONF_MIN_FREE_BYTES: 32 * 1024,
        CONF_MAX_FILE_SIZE: 384 * 1024,
    }
    config.update(kwargs)
    return config


def test_validate_config_accepts_a_fitting_budget() -> None:
    config = _config()
    assert _validate_config(config) is config


def test_validate_config_rejects_duplicate_column_names() -> None:
    config = _config(columns=[_column("value"), _column("value")])
    with pytest.raises(cv.Invalid, match="duplicate column name"):
        _validate_config(config)


def test_validate_config_rejects_zero_scale_factor() -> None:
    config = _config(columns=[_column(scale_factor=0.0)])
    with pytest.raises(cv.Invalid, match="scale_factor must not be 0"):
        _validate_config(config)


def _final_validate_with_hub(hub: dict, platform: dict) -> None:
    token = fv.full_config.set({"tsdb": hub})
    try:
        _final_validate(platform)
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("key", [CONF_AVERAGE, CONF_MIN])
def test_sensor_platform_requires_a_clock_for_aggregates(key: str) -> None:
    platform = {CONF_COLUMNS: [{CONF_COLUMN: "value", key: {}}]}
    with pytest.raises(cv.Invalid, match="need a time source"):
        _final_validate_with_hub(_config(), platform)
    # With a clock the same columns are accepted.
    _final_validate_with_hub(_config(**{CONF_TIME_ID: "clock"}), platform)


def test_sensor_platform_rejects_an_unknown_column() -> None:
    platform = {CONF_COLUMNS: [{CONF_COLUMN: "missing", CONF_AVERAGE: {}}]}
    with pytest.raises(cv.Invalid, match="'missing' is not a tsdb column"):
        _final_validate_with_hub(_config(**{CONF_TIME_ID: "clock"}), platform)


def test_validate_config_rejects_a_budget_that_does_not_fit() -> None:
    with pytest.raises(cv.Invalid, match="min_free_bytes must stay below"):
        _validate_config(_config(min_free_bytes=512 * 1024))


@pytest.mark.parametrize("bad", [float("nan"), float("inf"), float("-inf")])
def test_validate_config_rejects_non_finite_scale_factor(bad: float) -> None:
    # A NaN scale reaches the int16 cast (undefined behaviour) and infinity is
    # emitted as the invalid C++ literal `inff`.
    with pytest.raises(cv.Invalid, match="scale_factor must be a finite number"):
        _validate_config(_config(columns=[_column(scale_factor=bad)]))


@pytest.mark.parametrize("bad", [float("nan"), float("inf"), float("-inf")])
def test_validate_config_rejects_non_finite_offset(bad: float) -> None:
    with pytest.raises(cv.Invalid, match="offset must be a finite number"):
        _validate_config(_config(columns=[_column(offset=bad)]))


def test_validate_config_accepts_finite_boundaries() -> None:
    config = _config(columns=[_column(scale_factor=1e300, offset=-1e300)])
    assert _validate_config(config) is config


def test_validate_config_rejects_budget_over_partition() -> None:
    with pytest.raises(cv.Invalid, match="do not fit into partition_size"):
        _validate_config(_config(max_file_size=500 * 1024))


@pytest.mark.parametrize("value", ["internal", "auto", "AUTO"])
def test_validate_memory_accepts_modes(value: str) -> None:
    assert _validate_memory(value) == value.lower()


def test_validate_memory_rejects_unknown_mode() -> None:
    with pytest.raises(cv.Invalid):
        _validate_memory("sdram")


def test_validate_memory_requires_the_psram_component() -> None:
    with pytest.raises(cv.Invalid, match="requires component psram"):
        _validate_memory("psram")


def test_validate_config_reserves_room_for_filesystem_metadata() -> None:
    # 480KB + 32KB exactly fills a 512KB partition, which LittleFS cannot write
    # to because it needs blocks for its own metadata.
    with pytest.raises(cv.Invalid, match="filesystem metadata"):
        _validate_config(_config(max_file_size=480 * 1024))
    assert _validate_config(_config())[CONF_MAX_FILE_SIZE] == 384 * 1024
