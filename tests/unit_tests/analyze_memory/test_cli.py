"""Tests for memory analyzer CLI helpers."""

import os
from pathlib import Path
import time

from esphome.analyze_memory.cli import (
    _INI_AUTO_GENERATE_BEGIN,
    _INI_AUTO_GENERATE_END,
    _find_firmware_elf,
)


def test_find_firmware_elf_uses_generated_platformio_env(tmp_path: Path) -> None:
    """Generated PlatformIO env names are no longer tied to device names."""
    build_path = tmp_path / "build" / "kitchen-display"
    generated_env = build_path / ".pioenvs" / "esp32-arduino-abc123"
    generated_env.mkdir(parents=True)
    firmware_elf = generated_env / "firmware.elf"
    firmware_elf.write_bytes(b"elf")

    assert _find_firmware_elf(build_path) == firmware_elf


def test_find_firmware_elf_prefers_active_generated_env(tmp_path: Path) -> None:
    """When several generated envs exist, use platformio.ini's active env."""
    build_path = tmp_path / "build" / "kitchen-display"
    old_elf = build_path / ".pioenvs" / "kitchen-display" / "firmware.elf"
    new_elf = build_path / ".pioenvs" / "esp32-arduino-new" / "firmware.elf"
    build_path.mkdir(parents=True)
    old_elf.parent.mkdir(parents=True)
    new_elf.parent.mkdir(parents=True)
    old_elf.write_bytes(b"old")
    new_elf.write_bytes(b"new")
    (build_path / "platformio.ini").write_text(
        f"{_INI_AUTO_GENERATE_BEGIN}\n"
        "[env:kitchen-display]\n"
        "platform = espressif32\n"
        f"{_INI_AUTO_GENERATE_END}\n"
    )

    old_time = time.time() - 10
    os.utime(old_elf, (old_time, old_time))

    assert _find_firmware_elf(build_path) == old_elf
