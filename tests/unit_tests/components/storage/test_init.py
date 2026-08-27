"""Unit tests for the storage component's config validators."""

from __future__ import annotations

from unittest.mock import MagicMock, patch

import pytest

import esphome.codegen as cg
from esphome.components import storage
from esphome.components.storage import _validate_regex
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID

# _validate_regex guards the extract: `regex:` step. The runtime compiles the pattern with
# std::regex in the default ECMAScript grammar, and ESPHome builds with -fno-exceptions, so a
# pattern Python's re accepts but std::regex rejects would abort the node at runtime rather than
# raise. The validator hand-rolls a scanner to catch those at config time; these tests pin the
# behaviour it deliberately encodes so a refactor of the scanner has a regression net.


# Valid patterns whose constructs std::regex ECMAScript supports: they must pass through unchanged.
@pytest.mark.parametrize(
    "pattern",
    [
        "abc",
        "^a.*b$",
        "[a-z]+",
        r"\d{3}",
        "(?:abc)",  # non-capturing group
        "(?=abc)",  # lookahead
        "(?!abc)",  # negative lookahead
        "[(?]",  # '(?' inside a character class is a literal, not a group opener
        r"a\(?b",  # the '(' is escaped, so the following '?' is an ordinary quantifier
    ],
)
def test_validate_regex_accepts_supported_patterns(pattern: str) -> None:
    assert _validate_regex(pattern) == pattern


# Patterns Python's re accepts but std::regex ECMAScript rejects -- must be refused at config time.
@pytest.mark.parametrize(
    "pattern",
    [
        "(?P<name>x)",  # named group
        "(?<=x)",  # lookbehind
        "(?i)x",  # inline flag
        r"\Ax",  # \A anchor (std::regex has only ^)
        r"x\Z",  # \Z anchor (std::regex has only $)
        "a*+",  # possessive quantifier
        "a{1,2}+",  # possessive quantifier on a bounded repeat
    ],
)
def test_validate_regex_rejects_unsupported_constructs(pattern: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_regex(pattern)


# A pattern that is not valid regex at all is caught by the re.compile() guard rather than the
# std::regex scanner; both paths raise cv.Invalid.
def test_validate_regex_rejects_syntactically_invalid_pattern() -> None:
    with pytest.raises(cv.Invalid):
        _validate_regex("a(b")


# validate_mount_path enforces the invariant resolve_path()/build_path() rely on but never re-check
# (storage.cpp): a mount point is exactly one root-level segment. A regression that let a nested or
# trailing-slash path through would not fail validation -- it produces runtime path shadowing -- so
# pin the shape here.
@pytest.mark.parametrize("path", ["/sd", "/sdcard"])
def test_validate_mount_path_accepts_root_segments(path: str) -> None:
    assert storage.validate_mount_path(path) == path


@pytest.mark.parametrize(
    "path",
    [
        "sd",  # no leading slash
        "/",  # root is not a mount point
        "/sd/",  # trailing slash
        "/sd/nested",  # nested segment
    ],
)
def test_validate_mount_path_rejects_bad_shapes(path: str) -> None:
    with pytest.raises(cv.Invalid):
        storage.validate_mount_path(path)


def _final_validate_over(full_config: dict) -> object:
    """Run _final_validate with a stubbed full config (it reads fv.full_config, writes nothing)."""
    marker = {"_": object()}
    with patch.object(storage.fv, "full_config") as fc:
        fc.get.return_value = full_config
        result = storage._final_validate(marker)
    return result is marker


def _device(mount_path: str) -> dict:
    """A validated storage-device fragment: an id whose type inherits from Storage, plus a mount."""
    return {
        CONF_ID: ID("dev", type=storage.PathStorage),
        storage.CONF_MOUNT_PATH: mount_path,
    }


# _final_validate is the only place that can catch two storage devices sharing a mount point,
# because each driver is a separate component and cannot see the others (see its docstring).
def test_final_validate_rejects_duplicate_within_one_domain() -> None:
    with pytest.raises(cv.Invalid, match="claimed twice"):
        _final_validate_over({"sd_storage": [_device("/sd"), _device("/sd")]})


def test_final_validate_rejects_duplicate_across_domains() -> None:
    with pytest.raises(cv.Invalid, match="claimed by both"):
        _final_validate_over(
            {"sd_storage": [_device("/sd")], "nfs_client": [_device("/sd")]}
        )


def test_final_validate_accepts_distinct_mount_points() -> None:
    assert _final_validate_over(
        {"sd_storage": [_device("/sd")], "nfs_client": [_device("/nfs")]}
    )


# A storage device is identified by its id type, not by a key named mount_path. A foreign component
# using that key -- even with a string value matching a real device -- must be ignored, or it would
# raise a false collision and reserve the name 'mount_path' across every component's schema.
def test_final_validate_ignores_foreign_mount_path() -> None:
    foreign = {CONF_ID: ID("x", type=cg.Component), storage.CONF_MOUNT_PATH: "/sd"}
    assert _final_validate_over({"other": [foreign], "sd_storage": [_device("/sd")]})


def test_collect_mount_paths_collects_only_storage_devices() -> None:
    out: list[tuple[str, str]] = []
    storage._collect_mount_paths(
        {
            "devices": [
                _device("/sd"),
                {CONF_ID: ID("x", type=cg.Component), storage.CONF_MOUNT_PATH: "/foo"},
            ]
        },
        "sd_storage",
        out,
    )
    assert out == [("/sd", "sd_storage")]


def _data(path_max: int = 0, fatfs: bool = False) -> MagicMock:
    data = MagicMock()
    data.path_max = path_max
    data.fatfs_path_bound = fatfs
    return data


# _resolve_path_max sizes USE_STORAGE_PATH_MAX / USE_STORAGE_VFS_PATH_MAX, i.e. every path buffer on
# the device; a wrong branch fails paths with INVALID_ARGS only once a card is mounted. Pin all four.
def test_resolve_path_max_explicit_wins() -> None:
    assert storage._resolve_path_max({storage.CONF_PATH_MAX: 300}) == 300


def test_resolve_path_max_uses_driver_bound() -> None:
    with patch.object(storage, "_get_data", return_value=_data(path_max=100)):
        assert storage._resolve_path_max({}) == 100


def test_resolve_path_max_defaults_without_driver() -> None:
    with patch.object(storage, "_get_data", return_value=_data()):
        assert storage._resolve_path_max({}) == storage._DEFAULT_PATH_MAX


def test_resolve_path_max_fatfs_short_names() -> None:
    # CONFIG_FATFS_LFN_NONE on: FatFs hands back 8.3 names, so the bound is the short-name walk
    # formula rather than a long-filename length.
    from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS

    sdkconfig = {KEY_ESP32: {KEY_SDKCONFIG_OPTIONS: {"CONFIG_FATFS_LFN_NONE": "y"}}}
    expected = storage._FATFS_SHORT_NAME_MAX * (storage._MAX_RECURSION_DEPTH + 1) + 1
    with (
        patch.object(storage, "_get_data", return_value=_data(fatfs=True)),
        patch.object(storage, "CORE", MagicMock(is_esp32=True, data=sdkconfig)),
    ):
        assert storage._resolve_path_max({}) == expected


def test_resolve_path_max_fatfs_long_filename() -> None:
    from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS

    sdkconfig = {KEY_ESP32: {KEY_SDKCONFIG_OPTIONS: {"CONFIG_FATFS_MAX_LFN": 255}}}
    with (
        patch.object(storage, "_get_data", return_value=_data(fatfs=True)),
        patch.object(storage, "CORE", MagicMock(is_esp32=True, data=sdkconfig)),
    ):
        assert storage._resolve_path_max({}) == 256


def test_resolve_path_max_fatfs_non_numeric_lfn_raises() -> None:
    from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
    from esphome.core import EsphomeError

    sdkconfig = {KEY_ESP32: {KEY_SDKCONFIG_OPTIONS: {"CONFIG_FATFS_MAX_LFN": "abc"}}}
    with (
        patch.object(storage, "_get_data", return_value=_data(fatfs=True)),
        patch.object(storage, "CORE", MagicMock(is_esp32=True, data=sdkconfig)),
        pytest.raises(EsphomeError),
    ):
        storage._resolve_path_max({})
