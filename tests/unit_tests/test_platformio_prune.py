"""Tests for esphome.platformio.prune."""

from __future__ import annotations

from collections.abc import Generator
import json
import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.platformio.prune import DEPRECATED_PLATFORMS, prune_platformio_installs


class _FakeConfig:
    def __init__(self, core_dir: Path) -> None:
        self._dirs = {
            "core_dir": core_dir,
            "platforms_dir": core_dir / "platforms",
            "packages_dir": core_dir / "packages",
        }

    def get(self, section: str, option: str) -> str:
        assert section == "platformio"
        return str(self._dirs[option])


@pytest.fixture
def core_dir(tmp_path: Path) -> Generator[Path]:
    core_dir = tmp_path / "platformio"
    with patch(
        "esphome.platformio.prune.get_platformio_config",
        return_value=_FakeConfig(core_dir),
    ):
        yield core_dir


def _platform(core_dir: Path, name: str, packages: list[str]) -> Path:
    platform = core_dir / "platforms" / name
    platform.mkdir(parents=True)
    (platform / "platform.json").write_text(
        json.dumps({"packages": {package: {} for package in packages}}),
        encoding="utf-8",
    )
    return platform


def _package(core_dir: Path, name: str) -> Path:
    package = core_dir / "packages" / name
    (package / "bin").mkdir(parents=True)
    return package


def test_prune_removes_the_platforms_and_their_packages(core_dir: Path) -> None:
    esp32 = _platform(
        core_dir, "espressif32", ["framework-espidf", "tool-cmake", "tool-scons"]
    )
    # pioarduino installs from a URL, the official platform ESPHome used
    # before it came from the registry; both are named espressif32
    pioarduino = _platform(core_dir, "espressif32@src-0123", ["framework-espidf"])
    official = _platform(core_dir, "espressif32@3.5.0", ["toolchain-xtensa-esp32"])
    esp8266 = _platform(core_dir, "espressif8266", ["toolchain-xtensa"])
    libretiny = _platform(core_dir, "libretiny", ["framework-beken-bdk", "tool-cmake"])
    gone = [
        _package(core_dir, "framework-espidf"),
        _package(core_dir, "framework-espidf@3.50506.0"),
        _package(core_dir, "toolchain-xtensa-esp32"),
        _package(core_dir, "toolchain-xtensa"),
    ]
    stay = [
        # PlatformIO itself needs it
        _package(core_dir, "tool-scons"),
        # A remaining platform needs it
        _package(core_dir, "tool-cmake"),
        _package(core_dir, "framework-beken-bdk"),
        # No platform lists it
        _package(core_dir, "tool-picotool@1.2.3"),
    ]
    tools = core_dir / "tools"
    dist = core_dir / "dist"
    penv = core_dir / "penv"
    for path in (tools, dist, penv):
        path.mkdir()

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    platforms = {esp32, pioarduino, official, esp8266}
    assert set(removed) == {*platforms, *gone, tools, dist}
    # The platforms are last, they say which packages are theirs
    assert set(removed[-4:]) == platforms
    for path in removed:
        assert not path.exists()
    for path in (libretiny, *stay, penv):
        assert path.is_dir()
    assert not list(core_dir.rglob("*.pruning"))


def test_prune_removes_copies_a_newer_install_replaced(core_dir: Path) -> None:
    rp2 = _platform(core_dir, "raspberrypi", ["framework-arduinopico", "rp2-gcc"])
    old_rp2 = _platform(
        core_dir, "raspberrypi@src-0123", ["framework-arduinopico", "old-rp2-gcc"]
    )
    libretiny = _platform(core_dir, "libretiny", ["framework-beken-bdk"])
    old_libretiny = _platform(core_dir, "libretiny@src-4567", ["framework-beken-bdk"])
    framework = _package(core_dir, "framework-arduinopico")
    old_framework = _package(core_dir, "framework-arduinopico@src-89ab")
    old_toolchain = _package(core_dir, "toolchain-rp2040-earlephilhower@src-89ab")
    # Only the replaced platform copy lists it
    old_gcc = _package(core_dir, "old-rp2-gcc")
    stay = [
        framework,
        _package(core_dir, "rp2-gcc"),
        _package(core_dir, "toolchain-rp2040-earlephilhower"),
        # LibreTiny families build with different versions at once
        _package(core_dir, "framework-beken-bdk"),
        _package(core_dir, "framework-beken-bdk@src-cdef"),
    ]

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert set(removed) == {
        old_rp2,
        old_libretiny,
        old_framework,
        old_toolchain,
        old_gcc,
    }
    for path in (rp2, libretiny, *stay):
        assert path.is_dir()


def test_prune_removes_the_nrf52_platform(core_dir: Path) -> None:
    """Its toolchain shares a name with the one LibreTiny uses, which stays."""
    nrf52 = _platform(
        core_dir, "nordicnrf52", ["framework-zephyr", "toolchain-gccarmnoneeabi"]
    )
    _platform(core_dir, "libretiny", ["toolchain-gccarmnoneeabi"])
    zephyr = _package(core_dir, "framework-zephyr")
    toolchain = _package(core_dir, "toolchain-gccarmnoneeabi")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [zephyr, nrf52]
    assert toolchain.is_dir()


_LIBRETINY = "https://github.com/libretiny-eu"


def _libretiny_package(
    core_dir: Path, dirname: str, record: str | None, *, cloned: bool = True
) -> Path:
    """A package with PlatformIO's install record; ``cloned`` keeps it in .git."""
    package = _package(core_dir, dirname)
    if record is not None:
        location = package / ".git" if cloned else package
        location.mkdir(exist_ok=True)
        (location / ".piopm").write_text(record, encoding="utf-8")
    return package


def _record(uri: str | None) -> str:
    return json.dumps({"spec": {"uri": uri}})


def test_prune_removes_libretiny_packages_in_a_version_no_longer_used(
    core_dir: Path,
) -> None:
    platform = core_dir / "platforms" / "libretiny"
    platform.mkdir(parents=True)
    packages = {
        "framework-beken-bdk": {"version": f"{_LIBRETINY}/framework-beken-bdk#3.0.33"},
        "library-printf": {"version": f"{_LIBRETINY}/library-printf#6.1.0"},
        "toolchain-gccarmnoneeabi": {"optionalVersions": ["~1.100301.0"]},
    }
    (platform / "platform.json").write_text(
        json.dumps({"packages": packages}), encoding="utf-8"
    )
    bdk = f"git+{_LIBRETINY}/framework-beken-bdk"
    old = [
        _libretiny_package(
            core_dir, "framework-beken-bdk@src-0001", _record(f"{bdk}#3.0.7")
        ),
        _libretiny_package(
            core_dir,
            "library-printf@src-0002",
            _record(f"git+{_LIBRETINY}/library-printf#5.2.0"),
            cloned=False,
        ),
    ]
    stay = [
        # The latest install, whatever its version
        _libretiny_package(core_dir, "framework-beken-bdk", _record(f"{bdk}#3.0.7")),
        # Both versions the platform builds with: its own and the one ESPHome selects
        _libretiny_package(
            core_dir, "framework-beken-bdk@src-0003", _record(f"{bdk}#3.0.33")
        ),
        _libretiny_package(
            core_dir, "framework-beken-bdk@src-0004", _record(f"{bdk}#3.0.78")
        ),
        # Nothing says where these came from
        _libretiny_package(core_dir, "framework-beken-bdk@src-0005", None),
        _libretiny_package(core_dir, "framework-beken-bdk@src-0006", "{"),
        _libretiny_package(core_dir, "framework-beken-bdk@src-0007", _record(None)),
        # Not a LibreTiny package
        _libretiny_package(
            core_dir, "framework-other@src-0008", _record(f"{bdk}#3.0.7")
        ),
    ]

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == old
    for path in stay:
        assert path.is_dir()


@pytest.mark.parametrize("manifest", ["{}", json.dumps({"packages": {"a": 1}})])
def test_prune_keeps_libretiny_packages_when_its_versions_are_unreadable(
    manifest: str, core_dir: Path
) -> None:
    platform = core_dir / "platforms" / "libretiny"
    platform.mkdir(parents=True)
    (platform / "platform.json").write_text(manifest, encoding="utf-8")
    copy = _libretiny_package(core_dir, "a@src-0001", _record("git+https://x/a#1"))

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert copy.is_dir()


def test_prune_dry_run_changes_nothing(
    core_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    esp32 = _platform(core_dir, "espressif32", ["framework-espidf"])
    package = _package(core_dir, "framework-espidf")

    with caplog.at_level(logging.INFO):
        removed = prune_platformio_installs(DEPRECATED_PLATFORMS, dry_run=True)

    assert removed == [package, esp32]
    assert esp32.is_dir()
    assert package.is_dir()
    assert f"Would delete {package}" in caplog.text


@pytest.mark.parametrize("manifest", [None, "{", "{}", '{"packages": 1}'])
def test_prune_keeps_packages_when_a_remaining_platform_is_unreadable(
    manifest: str | None, core_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    esp32 = _platform(core_dir, "espressif32", ["framework-espidf"])
    package = _package(core_dir, "framework-espidf")
    other = core_dir / "platforms" / "libretiny"
    other.mkdir()
    if manifest is not None:
        (other / "platform.json").write_text(manifest, encoding="utf-8")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [esp32]
    assert package.is_dir()
    assert "Keeping all PlatformIO packages" in caplog.text


def test_prune_removes_an_unreadable_platform_without_its_packages(
    core_dir: Path,
) -> None:
    esp32 = core_dir / "platforms" / "espressif32"
    esp32.mkdir(parents=True)
    package = _package(core_dir, "framework-espidf")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [esp32]
    assert package.is_dir()


def test_prune_finishes_an_interrupted_prune(core_dir: Path) -> None:
    """Leftovers go even when the platform that listed them is already gone."""
    leftover_platform = core_dir / "platforms" / "espressif32.pruning"
    leftover_platform.mkdir(parents=True)
    leftover_package = _package(core_dir, "framework-espidf.pruning")
    tools = core_dir / "tools"
    tools.mkdir()

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert set(removed) == {leftover_platform, leftover_package, tools}
    assert not list(core_dir.rglob("*.pruning"))
    assert not tools.exists()


def test_prune_leaves_a_symlinked_platform_and_what_it_uses(core_dir: Path) -> None:
    checkout = core_dir / "my-platform-espressif32"
    checkout.mkdir(parents=True)
    (checkout / "platform.json").write_text(
        json.dumps({"packages": {"framework-espidf": {}}}), encoding="utf-8"
    )
    old = _platform(core_dir, "espressif32@src-0123", ["framework-espidf", "old-gcc"])
    link = core_dir / "platforms" / "espressif32"
    try:
        link.symlink_to(checkout, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")
    package = _package(core_dir, "framework-espidf")
    old_gcc = _package(core_dir, "old-gcc")
    tools = core_dir / "tools"
    tools.mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [old_gcc, old]
    assert link.is_symlink()
    assert package.is_dir()
    assert tools.is_dir()


def test_prune_without_platformio_does_nothing(tmp_path: Path) -> None:
    with patch("esphome.platformio.prune.get_platformio_config", return_value=None):
        assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []


def test_prune_without_an_install_does_nothing(core_dir: Path) -> None:
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert not core_dir.exists()
