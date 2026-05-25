"""Tests for the analyze-memory command line helpers."""

from pathlib import Path

from esphome.analyze_memory.cli import _find_firmware_elf
from esphome.const import PLATFORMIO_ENV_NAME


def test_find_firmware_elf_uses_stable_platformio_env(tmp_path: Path) -> None:
    """The standalone CLI should find firmware.elf under the stable PIO env."""
    build_path = tmp_path / ".esphome" / "build" / "test-device"
    firmware_elf = build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.write_text("mock elf")

    assert _find_firmware_elf(build_path) == str(firmware_elf)


def test_find_firmware_elf_keeps_legacy_device_env_fallback(tmp_path: Path) -> None:
    """Existing build dirs with device-named PIO envs should still work."""
    build_path = tmp_path / ".esphome" / "build" / "test-device"
    firmware_elf = build_path / ".pioenvs" / "test-device" / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.write_text("mock elf")

    assert _find_firmware_elf(build_path) == str(firmware_elf)


def test_find_firmware_elf_uses_stable_libretiny_raw_firmware(
    tmp_path: Path,
) -> None:
    """The LibreTiny raw_firmware.elf should be found under the stable PIO env."""
    build_path = tmp_path / ".esphome" / "build" / "test-device"
    raw_firmware_elf = (
        build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "raw_firmware.elf"
    )
    raw_firmware_elf.parent.mkdir(parents=True)
    raw_firmware_elf.write_text("mock elf")

    assert _find_firmware_elf(build_path) == str(raw_firmware_elf)
