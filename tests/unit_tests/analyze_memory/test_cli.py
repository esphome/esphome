from pathlib import Path

from esphome.analyze_memory.cli import _find_firmware_elf
from esphome.const import PLATFORMIO_ENV_NAME


def test_find_firmware_elf_uses_stable_platformio_env(tmp_path: Path) -> None:
    """Standalone analyzer should find PlatformIO's stable env output."""
    build_path = tmp_path / ".esphome" / "build" / "my-device"
    firmware_elf = build_path / ".pioenvs" / PLATFORMIO_ENV_NAME / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.write_bytes(b"elf")

    assert _find_firmware_elf(build_path) == str(firmware_elf)


def test_find_firmware_elf_keeps_legacy_device_env_compatibility(
    tmp_path: Path,
) -> None:
    """Existing build directories with device-named envs remain analyzable."""
    build_path = tmp_path / ".esphome" / "build" / "my-device"
    firmware_elf = build_path / ".pioenvs" / "my-device" / "firmware.elf"
    firmware_elf.parent.mkdir(parents=True)
    firmware_elf.write_bytes(b"elf")

    assert _find_firmware_elf(build_path) == str(firmware_elf)
