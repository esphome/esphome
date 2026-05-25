"""Tests for script/ci_memory_impact_extract.py path handling."""

import os
from pathlib import Path
import sys
import time

# Add script directory to path so we can import the module.
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_extract import (  # noqa: E402
    _INI_AUTO_GENERATE_BEGIN,
    _INI_AUTO_GENERATE_END,
    _find_elf_path,
    _idedata_candidates,
)


def _write_platformio_ini(build_path: Path, env_name: str) -> None:
    build_path.mkdir(parents=True, exist_ok=True)
    (build_path / "platformio.ini").write_text(
        f"{_INI_AUTO_GENERATE_BEGIN}\n"
        f"[env:{env_name}]\n"
        "platform = test\n"
        f"{_INI_AUTO_GENERATE_END}\n",
        encoding="utf-8",
    )


def test_find_elf_path_uses_generated_platformio_env(tmp_path: Path) -> None:
    """CI detailed analysis should find firmware.elf under the generated PIO env."""
    build_path = tmp_path / "test-device"
    _write_platformio_ini(build_path, "esp32-arduino-abc123")
    firmware_elf = build_path / ".pioenvs" / "esp32-arduino-abc123" / "firmware.elf"
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


def test_find_elf_path_scans_newest_platformio_env(tmp_path: Path) -> None:
    """Detailed analysis should still work without generated ini metadata."""
    build_path = tmp_path / "test-device"
    old_elf = build_path / ".pioenvs" / "old-env" / "firmware.elf"
    new_elf = build_path / ".pioenvs" / "new-env" / "firmware.elf"
    old_elf.parent.mkdir(parents=True)
    new_elf.parent.mkdir(parents=True)
    old_elf.touch()
    new_elf.touch()

    old_time = time.time() - 10
    new_time = time.time()
    os.utime(old_elf.parent, (old_time, old_time))
    os.utime(new_elf.parent, (new_time, new_time))

    assert _find_elf_path(build_path) == str(new_elf)


def test_find_elf_path_uses_generated_libretiny_raw_firmware(tmp_path: Path) -> None:
    """The LibreTiny raw_firmware.elf also lives under the generated PIO env."""
    build_path = tmp_path / "test-device"
    _write_platformio_ini(build_path, "bk72xx-arduino-abc123")
    raw_firmware_elf = (
        build_path / ".pioenvs" / "bk72xx-arduino-abc123" / "raw_firmware.elf"
    )
    raw_firmware_elf.parent.mkdir(parents=True)
    raw_firmware_elf.touch()

    assert _find_elf_path(build_path) == str(raw_firmware_elf)


def test_idedata_candidates_check_generated_env_before_legacy(
    tmp_path: Path,
) -> None:
    """Detailed analysis should prefer current generated-env idedata."""
    build_path = tmp_path / "test-device"
    _write_platformio_ini(build_path, "esp32-arduino-abc123")

    assert _idedata_candidates(build_path)[:2] == [
        build_path / ".pioenvs" / "esp32-arduino-abc123" / "idedata.json",
        build_path / ".pioenvs" / "test-device" / "idedata.json",
    ]
