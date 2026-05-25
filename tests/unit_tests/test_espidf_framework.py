"""Tests for esphome.espidf.framework helpers."""

# pylint: disable=protected-access

import pytest

from esphome.espidf.framework import _parse_git_source


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        # github:// shorthand
        (
            "github://espressif/esp-idf",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "github://espressif/esp-idf@release/v6.0",
            ("https://github.com/espressif/esp-idf.git", "release/v6.0"),
        ),
        # explicit https://github.com/...git URL
        (
            "https://github.com/espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "https://github.com/espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "https://github.com/espressif/esp-idf.git@v6.0.1",
            ("https://github.com/espressif/esp-idf.git", "v6.0.1"),
        ),
    ],
)
def test_parse_git_source_recognized(
    source: str, expected: tuple[str, str | None]
) -> None:
    assert _parse_git_source(source) == expected


@pytest.mark.parametrize(
    "source",
    [
        # archive URLs fall through to the existing download path
        "https://github.com/espressif/esp-idf/archive/refs/heads/master.zip",
        "https://dl.espressif.com/dl/esp-idf/v6.0.1/esp-idf-v6.0.1.zip",
        "https://github.com/esphome-libs/esp-idf/releases/download/v5.5.4/esp-idf-v5.5.4.tar.xz",
        # SSH and other git protocols are intentionally rejected — match
        # external_components, which only recognizes github:// + structured
        # dicts for these.
        "git@github.com:espressif/esp-idf.git",
        "ssh://git@github.com/espressif/esp-idf.git",
        "git://github.com/espressif/esp-idf.git",
        # non-GitHub .git URLs are intentionally rejected for the same reason
        "https://gitlab.com/foo/bar.git",
        "https://github.example.com/foo/bar.git",
    ],
)
def test_parse_git_source_rejected(source: str) -> None:
    assert _parse_git_source(source) is None
