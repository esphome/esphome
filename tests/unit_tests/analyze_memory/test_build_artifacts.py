"""Tests for locating build artifacts across the supported toolchain layouts."""

from pathlib import Path

import pytest

from esphome.analyze_memory.toolchain import (
    find_elf_path,
    find_idedata_path,
    idedata_candidates,
)
from esphome.build_helpers.idedata import _cc_path_from_cxx
from esphome.platformio.toolchain import IDEData


def _make_build_dir(tmp_path: Path, name: str = "mydevice") -> Path:
    """Create <tmp_path>/.esphome/build/<name>, mirroring a real data dir."""
    build_path = tmp_path / ".esphome" / "build" / name
    build_path.mkdir(parents=True)
    return build_path


def _touch(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("", encoding="utf-8")
    return path


def test_find_elf_path_native_esp_idf(tmp_path: Path) -> None:
    """The native ESP-IDF toolchain writes the ELF under build/."""
    build_path = _make_build_dir(tmp_path)
    elf = _touch(build_path / "build" / "firmware.elf")

    assert find_elf_path(build_path) == elf


def test_find_elf_path_platformio(tmp_path: Path) -> None:
    """The PlatformIO toolchain writes the ELF under .pioenvs/<name>/."""
    build_path = _make_build_dir(tmp_path)
    elf = _touch(build_path / ".pioenvs" / build_path.name / "firmware.elf")

    assert find_elf_path(build_path) == elf


def test_find_elf_path_libretiny(tmp_path: Path) -> None:
    """The LibreTiny toolchain names the unwrapped ELF raw_firmware.elf."""
    build_path = _make_build_dir(tmp_path)
    elf = _touch(build_path / ".pioenvs" / build_path.name / "raw_firmware.elf")

    assert find_elf_path(build_path) == elf


@pytest.mark.parametrize(
    "relative_elf",
    [
        # SDK < 2.9.2
        "zephyr/zephyr.elf",
        # SDK >= 2.9.2 nests the artifacts one level deeper
        "zephyr/zephyr/zephyr.elf",
    ],
)
def test_find_elf_path_zephyr(tmp_path: Path, relative_elf: str) -> None:
    """Zephyr (nRF52) keeps the ELF under .pioenvs/<name>/zephyr/."""
    build_path = _make_build_dir(tmp_path)
    elf = _touch(build_path / ".pioenvs" / build_path.name / relative_elf)

    assert find_elf_path(build_path) == elf


def test_find_elf_path_missing(tmp_path: Path) -> None:
    """An unknown layout resolves to None rather than a bogus path."""
    assert find_elf_path(_make_build_dir(tmp_path)) is None


def test_find_idedata_path_in_data_dir(tmp_path: Path) -> None:
    """The idedata cache sits in the data dir that holds the build dir."""
    build_path = _make_build_dir(tmp_path)
    idedata = _touch(tmp_path / ".esphome" / "idedata" / f"{build_path.name}.json")

    assert find_idedata_path(build_path) == idedata


def test_find_idedata_path_in_pioenvs(tmp_path: Path) -> None:
    """Test builds may keep idedata alongside the PlatformIO env."""
    build_path = _make_build_dir(tmp_path)
    idedata = _touch(build_path / ".pioenvs" / build_path.name / "idedata.json")

    assert find_idedata_path(build_path) == idedata


def test_find_idedata_path_missing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A missing idedata resolves to None."""
    # Keep the cwd/home fallbacks from finding an unrelated file on this machine
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(Path, "home", classmethod(lambda cls: tmp_path))

    assert find_idedata_path(_make_build_dir(tmp_path)) is None


def test_idedata_candidates_are_what_find_probes(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Every advertised candidate is one find_idedata_path actually accepts.

    The candidates are reported to the user when idedata is missing, so a list
    that drifts from the lookup would send someone hunting in the wrong place.
    """
    # Two candidates are relative to the cwd and to home; keep the test from
    # writing into the real ones.
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(Path, "home", classmethod(lambda cls: tmp_path))

    build_path = _make_build_dir(tmp_path)
    candidates = idedata_candidates(build_path)

    assert candidates, "no candidates advertised"
    for candidate in candidates:
        _touch(candidate)
        assert find_idedata_path(build_path) == candidate
        candidate.unlink()


@pytest.mark.parametrize(
    ("cxx_path", "expected"),
    [
        ("/tools/bin/xtensa-esp32-elf-g++", "/tools/bin/xtensa-esp32-elf-gcc"),
        ("/tools/bin/riscv32-esp-elf-g++", "/tools/bin/riscv32-esp-elf-gcc"),
        (
            r"C:\tools\bin\xtensa-esp32-elf-g++.exe",
            r"C:\tools\bin\xtensa-esp32-elf-gcc.exe",
        ),
        # Nothing to rewrite; leave the path alone
        ("/tools/bin/clang++", "/tools/bin/clang++"),
    ],
)
def test_cc_path_from_cxx(cxx_path: str, expected: str) -> None:
    """cc_path is derived from the C++ compiler that compile_commands.json names."""
    assert _cc_path_from_cxx(cxx_path) == expected


def test_native_idedata_resolves_toolchain_tools() -> None:
    """The binutils paths are derived from the native ESP-IDF cc_path.

    Without cc_path, IDEData.objdump_path raises EsphomeError and the
    memory analysis silently degrades to no component or symbol detail.
    """
    idedata = IDEData(
        {
            "cc_path": _cc_path_from_cxx("/tools/bin/xtensa-esp32-elf-g++"),
            "cxx_path": "/tools/bin/xtensa-esp32-elf-g++",
        }
    )

    assert idedata.objdump_path == "/tools/bin/xtensa-esp32-elf-objdump"
    assert idedata.readelf_path == "/tools/bin/xtensa-esp32-elf-readelf"
