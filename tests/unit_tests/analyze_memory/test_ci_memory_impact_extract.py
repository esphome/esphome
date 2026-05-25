"""Tests for script/ci_memory_impact_extract.py path handling."""

from pathlib import Path
import sys

from esphome.const import PLATFORMIO_ENV_NAME

# Add script directory to path so we can import the module.
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_extract import _find_elf_path, _idedata_candidates  # noqa: E402


def test_find_elf_path_uses_stable_platformio_env(tmp_path: Path) -> None:
    """CI detailed analysis should find firmware.elf under the stable PIO env."""
    build_path = tmp_path / "test-device"
    firmware_elf = build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.touch()

    assert _find_elf_path(build_path) == str(firmware_elf)


def test_find_elf_path_falls_back_to_legacy_platformio_env(tmp_path: Path) -> None:
    """Historical CI artifacts used the device name as the PIO env."""
    build_path = tmp_path / "test-device"
    firmware_elf = build_path / ".pioenvs" / "test-device" / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.touch()

    assert _find_elf_path(build_path) == str(firmware_elf)


def test_find_elf_path_uses_stable_libretiny_raw_firmware(tmp_path: Path) -> None:
    """The LibreTiny raw_firmware.elf also lives under the stable PIO env."""
    build_path = tmp_path / "test-device"
    raw_firmware_elf = (
        build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "raw_firmware.elf"
    )
    raw_firmware_elf.parent.mkdir(parents=True)
    raw_firmware_elf.touch()

    assert _find_elf_path(build_path) == str(raw_firmware_elf)


def test_idedata_candidates_check_stable_env_before_legacy(
    tmp_path: Path,
) -> None:
    """Detailed analysis should prefer current stable-env idedata."""
    build_path = tmp_path / "test-device"

    assert _idedata_candidates(build_path)[:2] == [
        build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "idedata.json",
        build_path / ".pioenvs" / "test-device" / "idedata.json",
    ]
