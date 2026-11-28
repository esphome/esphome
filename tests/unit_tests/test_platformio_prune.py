"""Tests for esphome.platformio.prune."""

from __future__ import annotations

from collections.abc import Generator
import json
import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.framework_helpers import get_python_env_executable_path
from esphome.platformio.prune import DEPRECATED_PLATFORMS, prune_platformio_installs


class _FakeConfig:
    def __init__(self, core_dir: Path) -> None:
        self._dirs = {
            "core_dir": core_dir,
            "platforms_dir": core_dir / "platforms",
            "packages_dir": core_dir / "packages",
            "cache_dir": core_dir / ".cache",
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


_PRUNING = "_tmp_installing.pruning-"
_RP2_SOURCE = "https://github.com/maxgerhardt/platform-raspberrypi.git#0123abc"
_LIBRETINY_SOURCE = "https://github.com/libretiny-eu/libretiny.git#v1.14.0"
_RECOMMENDED = {"raspberrypi": _RP2_SOURCE, "libretiny": _LIBRETINY_SOURCE}


def _record(uri: object) -> str:
    """PlatformIO's install record; it stores a git URL with a ``git+`` prefix."""
    return json.dumps({"spec": {"uri": uri}})


def _platform(
    core_dir: Path,
    name: str,
    packages: list[str],
    optional: tuple[str, ...] = (),
    source: str | None = None,
) -> Path:
    """A platform; ``source`` is the URL its install record says it came from."""
    platform = core_dir / "platforms" / name
    platform.mkdir(parents=True)
    manifest = {package: {} for package in packages}
    manifest |= {package: {"optional": True} for package in optional}
    (platform / "platform.json").write_text(
        json.dumps({"packages": manifest}), encoding="utf-8"
    )
    if source is not None:
        (platform / ".git").mkdir()
        (platform / ".git" / ".piopm").write_text(
            _record(f"git+{source}"), encoding="utf-8"
        )
    return platform


def _leftovers(core_dir: Path) -> list[Path]:
    return [path for path in core_dir.rglob("*") if "pruning" in path.name]


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
    assert not _leftovers(core_dir)


def test_prune_keeps_only_the_recommended_rp2_and_libretiny_platforms(
    core_dir: Path,
) -> None:
    """Whichever folder it is in: a pinned version built last has the plain name."""
    pinned_rp2 = _platform(
        core_dir,
        "raspberrypi",
        ["pinned-rp2-gcc"],
        optional=("framework-arduinopico",),
        source="https://github.com/maxgerhardt/platform-raspberrypi.git#9999fff",
    )
    rp2 = _platform(
        core_dir,
        "raspberrypi@src-0123",
        ["rp2-gcc"],
        optional=("framework-arduinopico",),
        source=_RP2_SOURCE,
    )
    # Nothing says where this one came from
    unknown_rp2 = _platform(core_dir, "raspberrypi@src-4567", [])
    libretiny = _platform(
        core_dir, "libretiny", ["framework-beken-bdk"], source=_LIBRETINY_SOURCE
    )
    old_libretiny = _platform(
        core_dir,
        "libretiny@src-89ab",
        ["framework-beken-bdk"],
        source="https://github.com/libretiny-eu/libretiny.git#v1.12.1",
    )
    # Only the pinned platform lists it
    pinned_gcc = _package(core_dir, "pinned-rp2-gcc")
    stay = [
        _package(core_dir, "framework-arduinopico"),
        # ESPHome installs the framework from a URL of its own, not the
        # one the platform lists, so a replaced copy may be the default one
        _libretiny_package(
            core_dir,
            "framework-arduinopico@src-0001",
            _record("https://example.com/arduino-pico.zip"),
            cloned=False,
        ),
        _package(core_dir, "rp2-gcc"),
        _package(core_dir, "framework-beken-bdk"),
    ]

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS, _RECOMMENDED)

    assert set(removed) == {pinned_rp2, old_libretiny, pinned_gcc}
    for path in (rp2, unknown_rp2, libretiny, *stay):
        assert path.is_dir()


def test_prune_keeps_every_rp2_and_libretiny_platform_without_a_recommended_one(
    core_dir: Path,
) -> None:
    rp2 = _platform(core_dir, "raspberrypi", [], source=_RP2_SOURCE)
    old_rp2 = _platform(core_dir, "raspberrypi@src-0123", [], source=f"{_RP2_SOURCE}0")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert rp2.is_dir()
    assert old_rp2.is_dir()


def test_prune_is_not_held_back_by_an_optional_listing(core_dir: Path) -> None:
    """A removed platform's package goes even if a kept one lists it as optional."""
    nrf52 = _platform(
        core_dir, "nordicnrf52", ["tool-cmake", "toolchain-gccarmnoneeabi"]
    )
    old_rp2 = _platform(
        core_dir,
        "raspberrypi@src-0123",
        [],
        optional=("tool-cmake",),
        source=f"{_RP2_SOURCE}0",
    )
    _platform(
        core_dir,
        "raspberrypi",
        ["toolchain-gccarmnoneeabi"],
        optional=("tool-cmake", "tool-jlink"),
        source=_RP2_SOURCE,
    )
    cmake = _package(core_dir, "tool-cmake")
    cmake_copy = _package(core_dir, "tool-cmake@src-0123")
    toolchain = _package(core_dir, "toolchain-gccarmnoneeabi")

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS, _RECOMMENDED)

    assert removed == [cmake, cmake_copy, nrf52, old_rp2]
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


def test_prune_removes_the_nrf52_platform(core_dir: Path) -> None:
    """Its toolchain shares a name with the one LibreTiny and RP2 list, which stays."""
    nrf52 = _platform(
        core_dir, "nordicnrf52", ["framework-zephyr", "toolchain-gccarmnoneeabi"]
    )
    _platform(core_dir, "libretiny", ["toolchain-gccarmnoneeabi"])
    _platform(core_dir, "raspberrypi", ["toolchain-gccarmnoneeabi"])
    zephyr = _package(core_dir, "framework-zephyr")
    toolchain = _package(core_dir, "toolchain-gccarmnoneeabi")
    # The nRF52 one, moved aside when LibreTiny installed its own
    nrf52_toolchain = _libretiny_package(
        core_dir,
        "toolchain-gccarmnoneeabi@src-0001",
        _record("https://example.com/toolchain-sdk-ng/v0.17.4-0.zip"),
        cloned=False,
    )
    # Nothing says where this one came from
    unknown = _package(core_dir, "toolchain-gccarmnoneeabi@src-0002")

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert removed == [zephyr, nrf52_toolchain, nrf52]
    assert toolchain.is_dir()
    assert unknown.is_dir()


def test_prune_keeps_a_replaced_copy_a_remaining_platform_installs(
    core_dir: Path,
) -> None:
    url = "https://example.com/tool-cmake-4.zip"
    nrf52 = _platform(core_dir, "nordicnrf52", ["tool-cmake"])
    other = core_dir / "platforms" / "other"
    other.mkdir()
    (other / "platform.json").write_text(
        json.dumps({"packages": {"tool-cmake": {"version": url}}}), encoding="utf-8"
    )
    copy = _libretiny_package(
        core_dir, "tool-cmake@src-0001", _record(url), cloned=False
    )
    old = _libretiny_package(
        core_dir, "tool-cmake@src-0002", _record(f"{url}.old"), cloned=False
    )

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [old, nrf52]
    assert copy.is_dir()


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
        _libretiny_package(core_dir, "framework-beken-bdk@src-0009", _record([])),
        # Not a LibreTiny package
        _libretiny_package(
            core_dir, "framework-other@src-0008", _record(f"{bdk}#3.0.7")
        ),
    ]

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == old
    for path in stay:
        assert path.is_dir()


def test_prune_keeps_libretiny_packages_when_its_versions_are_unreadable(
    core_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The packages are listed, but a library they name is not."""
    platform = core_dir / "platforms" / "libretiny"
    platform.mkdir(parents=True)
    packages = {"framework-x": {"libraries": {"gone": "1.0"}}}
    (platform / "platform.json").write_text(
        json.dumps({"packages": packages}), encoding="utf-8"
    )
    copy = _libretiny_package(
        core_dir, "framework-x@src-0001", _record("git+https://x/framework-x#1")
    )

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert copy.is_dir()
    assert "Keeping all replaced packages" in caplog.text


def _libretiny_with_an_old_copy(core_dir: Path, package: str) -> Path:
    """A LibreTiny platform and a replaced copy of ``package`` in an old version."""
    platform = core_dir / "platforms" / "libretiny"
    platform.mkdir(parents=True)
    packages = {package: {"version": f"{_LIBRETINY}/{package}#2.0"}}
    (platform / "platform.json").write_text(
        json.dumps({"packages": packages}), encoding="utf-8"
    )
    return _libretiny_package(
        core_dir, f"{package}@src-0001", _record(f"git+{_LIBRETINY}/{package}#1.0")
    )


def test_prune_keeps_a_libretiny_copy_of_a_package_another_platform_lists(
    core_dir: Path,
) -> None:
    copy = _libretiny_with_an_old_copy(core_dir, "toolchain-gccarmnoneeabi")
    _platform(core_dir, "raspberrypi", [], optional=("toolchain-gccarmnoneeabi",))

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert copy.is_dir()


def test_prune_keeps_libretiny_copies_when_another_platform_is_unreadable(
    core_dir: Path,
) -> None:
    copy = _libretiny_with_an_old_copy(core_dir, "framework-beken-bdk")
    (core_dir / "platforms" / "raspberrypi").mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert copy.is_dir()

    # With every platform readable the old copy goes
    (core_dir / "platforms" / "raspberrypi").rmdir()
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [copy]


def _pioarduino_penv(core_dir: Path, name: str = "penv") -> Path:
    """The Python environment pioarduino makes: the one with esptool in it."""
    penv = core_dir / name
    esptool = get_python_env_executable_path(penv, "esptool")
    esptool.parent.mkdir(parents=True)
    esptool.touch()
    return penv


def test_prune_removes_what_pioarduino_left_in_the_core_dir(core_dir: Path) -> None:
    esp32 = _platform(core_dir, "espressif32", [])
    penv = _pioarduino_penv(core_dir)
    entries = [core_dir / name for name in ("tools", "dist", "root_managed_components")]
    for entry in entries:
        entry.mkdir()
    record = core_dir / "idf-env.json"
    record.write_text("{}", encoding="utf-8")
    appstate = core_dir / "appstate.json"
    appstate.write_text("{}", encoding="utf-8")

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert set(removed) == {esp32, penv, record, *entries}
    for path in removed:
        assert not path.exists()
    assert appstate.is_file()
    assert not _leftovers(core_dir)


def test_prune_keeps_a_python_environment_that_is_not_pioarduinos(
    core_dir: Path,
) -> None:
    """PlatformIO's own installer puts PlatformIO itself in the same directory."""
    penv = core_dir / "penv"
    pio = get_python_env_executable_path(penv, "pio")
    pio.parent.mkdir(parents=True)
    pio.touch()
    # The official ESP32 platform kept one environment per ESP-IDF version in it
    espidf = penv / ".espidf-4.4.7"
    espidf.mkdir()
    leftover = _pioarduino_penv(core_dir, f"{_PRUNING}penv")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [leftover, espidf]
    assert pio.is_file()


def test_prune_keeps_the_libretiny_environment_inside_pioarduinos(
    core_dir: Path,
) -> None:
    """LibreTiny keeps its Python environment in a hidden folder of the same directory."""
    penv = _pioarduino_penv(core_dir)
    # "bin", or "Scripts" on Windows
    scripts = get_python_env_executable_path(penv, "esptool").parent.name
    for name in ("pyvenv.cfg", ".gitignore", ".lock"):
        (penv / name).touch()
    (penv / "lib").mkdir()
    try:
        (penv / "lib64").symlink_to("lib", target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")
    libretiny = penv / ".libretiny"
    (libretiny / "bin").mkdir(parents=True)
    interrupted = penv / f"{_PRUNING}include"
    interrupted.mkdir()

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert set(removed) == {
        interrupted,
        *(
            penv / name
            for name in (scripts, "lib", "lib64", "pyvenv.cfg", ".gitignore", ".lock")
        ),
    }
    # Last: without esptool the rest could not be told to be pioarduino's
    assert removed[-1] == penv / scripts
    assert [path.name for path in penv.iterdir()] == [".libretiny"]
    assert (libretiny / "bin").is_dir()

    # Nothing of pioarduino's is left to find
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []


def test_prune_keeps_the_python_environment_esphome_runs_from(
    core_dir: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """ESPHome installed into PlatformIO's environment brings esptool with it."""
    penv = _pioarduino_penv(core_dir)
    (penv / ".espidf-5.5.0").mkdir()
    monkeypatch.setattr("sys.prefix", str(penv))

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert get_python_env_executable_path(penv, "esptool").is_file()


def test_prune_keeps_the_core_dir_when_the_native_esp_idf_install_is_in_it(
    core_dir: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Its ``tools`` folder is then the native install's, not pioarduino's."""
    tools = core_dir / "tools"
    tools.mkdir(parents=True)
    penv = _pioarduino_penv(core_dir)
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(core_dir))

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert tools.is_dir()
    assert penv.is_dir()


def test_prune_hides_what_it_is_removing_from_platformio(core_dir: Path) -> None:
    """PlatformIO takes any other folder name in its package folders for an install."""
    esp32 = _platform(core_dir, "espressif32", ["framework-espidf"])
    package = _package(core_dir, "framework-espidf")

    with (
        patch("esphome.build_helpers.prune.rmtree", side_effect=OSError("killed")),
        pytest.raises(OSError, match="killed"),
    ):
        prune_platformio_installs(DEPRECATED_PLATFORMS)

    hidden = package.with_name(f"{_PRUNING}framework-espidf")
    assert hidden.is_dir()
    assert not package.exists()
    # The platform is still there to say which packages were its own
    assert esp32.is_dir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [hidden, esp32]
    assert not _leftovers(core_dir)


def test_prune_leaves_an_empty_python_environment_directory(core_dir: Path) -> None:
    (core_dir / "penv").mkdir(parents=True)

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert (core_dir / "penv").is_dir()


def test_prune_keeps_pioarduinos_files_for_a_symlinked_espressif32(
    core_dir: Path,
) -> None:
    checkout = core_dir / "my-platform-espressif32"
    checkout.mkdir(parents=True)
    (checkout / "platform.json").write_text('{"packages": {}}', encoding="utf-8")
    link = core_dir / "platforms" / "espressif32"
    link.parent.mkdir()
    try:
        link.symlink_to(checkout, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")
    penv = _pioarduino_penv(core_dir)
    (core_dir / "tools").mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert penv.is_dir()


def test_prune_clears_the_download_cache(core_dir: Path) -> None:
    cache = core_dir / ".cache"
    (cache / "downloads").mkdir(parents=True)
    (cache / "downloads" / "toolchain.tar.gz").write_bytes(b"x")
    leftover = core_dir / f"{_PRUNING}.cache"
    leftover.mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [cache, leftover]
    assert not cache.exists()
    assert not leftover.exists()

    # PlatformIO makes the directory again; an empty one is nothing to prune
    cache.mkdir()
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []


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

    tools = core_dir / "tools"
    tools.mkdir()
    leftover = core_dir / "platforms" / f"{_PRUNING}espressif8266"
    leftover.mkdir()

    # The platform stays too: it says which packages are its own
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [leftover]
    for path in (esp32, package, tools):
        assert path.is_dir()
    assert "Keeping all PlatformIO platforms and packages" in caplog.text


def test_prune_ignores_an_install_platformio_did_not_finish(core_dir: Path) -> None:
    esp32 = _platform(core_dir, "espressif32", ["framework-espidf"])
    package = _package(core_dir, "framework-espidf")
    unfinished = core_dir / "platforms" / "_tmp_installing-abc123"
    unfinished.mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [package, esp32]
    assert unfinished.is_dir()


def test_prune_removes_an_unreadable_platform_without_its_packages(
    core_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    esp32 = core_dir / "platforms" / "espressif32"
    esp32.mkdir(parents=True)
    package = _package(core_dir, "framework-espidf")

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [esp32]
    assert package.is_dir()
    assert f"Could not tell which packages {esp32} installed" in caplog.text


def test_prune_finishes_an_interrupted_prune(core_dir: Path) -> None:
    """Leftovers go even when the platform that listed them is already gone."""
    leftover_platform = core_dir / "platforms" / f"{_PRUNING}espressif32"
    leftover_platform.mkdir(parents=True)
    leftover_package = _package(core_dir, f"{_PRUNING}framework-espidf")
    tools = core_dir / "tools"
    tools.mkdir()

    removed = prune_platformio_installs(DEPRECATED_PLATFORMS)

    assert set(removed) == {leftover_platform, leftover_package, tools}
    assert not _leftovers(core_dir)
    assert not tools.exists()


def test_prune_leaves_a_symlinked_platform_and_what_it_uses(core_dir: Path) -> None:
    checkout = core_dir / "my-platform-espressif32"
    checkout.mkdir(parents=True)
    (checkout / "platform.json").write_text(
        json.dumps({"packages": {"framework-espidf": {"optional": True}}}),
        encoding="utf-8",
    )
    old = _platform(core_dir, "espressif32@src-0123", ["framework-espidf", "old-gcc"])
    link = core_dir / "platforms" / "espressif32"
    try:
        link.symlink_to(checkout, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")
    package = _package(core_dir, "framework-espidf")
    copy = _libretiny_package(
        core_dir, "framework-espidf@src-0001", _record("https://example.com/idf.zip")
    )
    old_gcc = _package(core_dir, "old-gcc")
    tools = core_dir / "tools"
    tools.mkdir()

    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == [old_gcc, old]
    assert link.is_symlink()
    assert package.is_dir()
    assert copy.is_dir()
    assert tools.is_dir()


def test_prune_without_platformio_does_nothing(tmp_path: Path) -> None:
    with patch("esphome.platformio.prune.get_platformio_config", return_value=None):
        assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []


def test_prune_without_an_install_does_nothing(core_dir: Path) -> None:
    assert prune_platformio_installs(DEPRECATED_PLATFORMS) == []
    assert not core_dir.exists()
