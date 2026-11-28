"""Tests for esphome.components.libretiny.prune."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from esphome.components.libretiny.const import CUSTOM_VERSIONS
from esphome.components.libretiny.prune import package_uris

_ORG = "https://github.com/libretiny-eu"
_PACKAGES: dict[str, Any] = {
    "framework-beken-bdk": {
        "version": f"{_ORG}/framework-beken-bdk#3.0.33",
        "libraries": {
            "freertos": "9.0.0",
            "lwip": {"2.1.3": "2.1.3-bdk", "default": "2.2.0-bdk"},
        },
    },
    "framework-realtek-amb1": {
        "version": f"{_ORG}/framework-realtek-amb1#v2022.06.21",
        "version_prefix": True,
        "libraries": {"freertos": "8.1.2"},
    },
    "library-lwip": {"base_url": f"{_ORG}/lwip"},
    "library-freertos": {"base_url": f"{_ORG}/library-freertos"},
    "library-printf": {"version": f"{_ORG}/library-printf#6.1.0"},
    "toolchain-gccarmnoneeabi": {"optionalVersions": ["~1.100301.0"]},
}


def _platform(tmp_path: Path, manifest: str) -> Path:
    (tmp_path / "platform.json").write_text(manifest, encoding="utf-8")
    return tmp_path


def test_package_uris_lists_the_versions_a_build_installs(tmp_path: Path) -> None:
    platform = _platform(tmp_path, json.dumps({"packages": _PACKAGES}))

    assert package_uris(platform) == {
        f"git+{_ORG}/{package}#{version}"
        for package, version in (
            # What the frameworks install by default
            ("framework-beken-bdk", "3.0.33"),
            ("framework-realtek-amb1", "v2022.06.21"),
            ("library-printf", "6.1.0"),
            # Their libraries: each family's version, and the default where
            # the manifest offers a choice (2.1.3-bdk needs custom_versions)
            ("library-freertos", "9.0.0"),
            ("library-freertos", "8.1.2"),
            ("lwip", "2.2.0-bdk"),
            # The versions ESPHome selects itself
            ("framework-beken-bdk", CUSTOM_VERSIONS["beken-bdk"]),
            ("library-freertos", CUSTOM_VERSIONS["freertos"]),
        )
    }


def test_package_uris_skips_a_choice_without_a_default(tmp_path: Path) -> None:
    packages = {
        "framework-x": {
            "version": f"{_ORG}/framework-x#1.0",
            "libraries": {"lwip": {"2.1.3": "2.1.3-x"}},
        },
        "library-lwip": _PACKAGES["library-lwip"],
    }
    platform = _platform(tmp_path, json.dumps({"packages": packages}))

    assert package_uris(platform) == {f"git+{_ORG}/framework-x#1.0"}


def test_package_uris_skips_a_custom_version_of_a_package_that_is_gone(
    tmp_path: Path,
) -> None:
    packages = {"library-printf": _PACKAGES["library-printf"]}
    platform = _platform(tmp_path, json.dumps({"packages": packages}))

    assert package_uris(platform) == {f"git+{_ORG}/library-printf#6.1.0"}


@pytest.mark.parametrize(
    "manifest",
    [
        None,
        "{",
        "{}",
        '{"packages": []}',
        # A library the manifest does not define
        json.dumps({"packages": {"framework-x": {"libraries": {"gone": "1.0"}}}}),
    ],
)
def test_package_uris_unreadable_manifest(manifest: str | None, tmp_path: Path) -> None:
    if manifest is not None:
        _platform(tmp_path, manifest)

    assert package_uris(tmp_path) is None
