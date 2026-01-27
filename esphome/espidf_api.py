"""ESP-IDF direct build API for ESPHome."""

import json
import logging
import os
from pathlib import Path
import shutil
import subprocess

from esphome.components.esp32.const import KEY_ESP32, KEY_FLASH_SIZE
from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME
from esphome.core import CORE, EsphomeError

_LOGGER = logging.getLogger(__name__)


def _get_idf_path() -> Path | None:
    """Get IDF_PATH from environment or common locations."""
    # Check environment variable first
    if "IDF_PATH" in os.environ:
        path = Path(os.environ["IDF_PATH"])
        if path.is_dir():
            return path

    # Check common installation locations
    common_paths = [
        Path.home() / "esp" / "esp-idf",
        Path.home() / ".espressif" / "esp-idf",
        Path("/opt/esp-idf"),
    ]

    for path in common_paths:
        if path.is_dir() and (path / "tools" / "idf.py").is_file():
            return path

    return None


def _get_idf_env() -> dict[str, str]:
    """Get environment variables needed for ESP-IDF build.

    Requires the user to have sourced export.sh before running esphome.
    """
    env = os.environ.copy()

    idf_path = _get_idf_path()
    if idf_path is None:
        raise EsphomeError(
            "ESP-IDF not found. Please install ESP-IDF and source export.sh:\n"
            "  git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf\n"
            "  cd ~/esp-idf && ./install.sh\n"
            "  source ~/esp-idf/export.sh\n"
            "See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/"
        )

    env["IDF_PATH"] = str(idf_path)
    return env


def run_idf_py(
    *args, cwd: Path | None = None, capture_output: bool = False
) -> int | str:
    """Run idf.py with the given arguments."""
    idf_path = _get_idf_path()
    if idf_path is None:
        raise EsphomeError("ESP-IDF not found")

    env = _get_idf_env()
    idf_py = idf_path / "tools" / "idf.py"

    cmd = ["python", str(idf_py)] + list(args)

    if cwd is None:
        cwd = CORE.build_path

    _LOGGER.debug("Running: %s", " ".join(cmd))
    _LOGGER.debug("  in directory: %s", cwd)

    if capture_output:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            _LOGGER.error("idf.py failed:\n%s", result.stderr)
        return result.stdout
    result = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        check=False,
    )
    return result.returncode


def run_reconfigure() -> int:
    """Run cmake reconfigure only (no build)."""
    return run_idf_py("reconfigure")


def run_compile(config, verbose: bool) -> int:
    """Compile the ESP-IDF project.

    Uses two-phase configure to auto-discover available components:
    1. If no previous build, configure with minimal REQUIRES to discover components
    2. Regenerate CMakeLists.txt with discovered components
    3. Run full build
    """
    from esphome.build_gen.espidf import has_discovered_components, write_project

    # Check if we need to do discovery phase
    if not has_discovered_components():
        _LOGGER.info("Discovering available ESP-IDF components...")
        write_project(minimal=True)
        rc = run_reconfigure()
        if rc != 0:
            _LOGGER.error("Component discovery failed")
            return rc
        _LOGGER.info("Regenerating CMakeLists.txt with discovered components...")
        write_project(minimal=False)

    # Build
    args = ["build"]

    if verbose:
        args.append("-v")

    # Add parallel job limit if configured
    if CONF_COMPILE_PROCESS_LIMIT in config.get(CONF_ESPHOME, {}):
        limit = config[CONF_ESPHOME][CONF_COMPILE_PROCESS_LIMIT]
        args.extend(["-j", str(limit)])

    # Set the sdkconfig file
    sdkconfig_path = CORE.relative_build_path(f"sdkconfig.{CORE.name}")
    if sdkconfig_path.is_file():
        args.extend(["-D", f"SDKCONFIG={sdkconfig_path}"])

    return run_idf_py(*args)


def get_firmware_path() -> Path:
    """Get the path to the compiled firmware binary."""
    build_dir = CORE.relative_build_path("build")
    return build_dir / f"{CORE.name}.bin"


def get_factory_firmware_path() -> Path:
    """Get the path to the factory firmware (with bootloader)."""
    build_dir = CORE.relative_build_path("build")
    return build_dir / f"{CORE.name}.factory.bin"


def create_factory_bin() -> bool:
    """Create factory.bin by merging bootloader, partition table, and app."""
    build_dir = CORE.relative_build_path("build")
    flasher_args_path = build_dir / "flasher_args.json"

    if not flasher_args_path.is_file():
        _LOGGER.warning("flasher_args.json not found, cannot create factory.bin")
        return False

    try:
        with open(flasher_args_path, encoding="utf-8") as f:
            flash_data = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        _LOGGER.error("Failed to read flasher_args.json: %s", e)
        return False

    # Get flash size from config
    flash_size = CORE.data[KEY_ESP32][KEY_FLASH_SIZE]

    # Build esptool merge command
    sections = []
    for addr, fname in sorted(
        flash_data.get("flash_files", {}).items(), key=lambda kv: int(kv[0], 16)
    ):
        file_path = build_dir / fname
        if file_path.is_file():
            sections.extend([addr, str(file_path)])
        else:
            _LOGGER.warning("Flash file not found: %s", file_path)

    if not sections:
        _LOGGER.warning("No flash sections found")
        return False

    output_path = get_factory_firmware_path()
    chip = flash_data.get("extra_esptool_args", {}).get("chip", "esp32")

    cmd = [
        "python",
        "-m",
        "esptool",
        "--chip",
        chip,
        "merge_bin",
        "--flash_size",
        flash_size,
        "--output",
        str(output_path),
    ] + sections

    _LOGGER.info("Creating factory.bin...")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)

    if result.returncode != 0:
        _LOGGER.error("Failed to create factory.bin: %s", result.stderr)
        return False

    _LOGGER.info("Created: %s", output_path)
    return True


def create_ota_bin() -> bool:
    """Copy the firmware to .ota.bin for ESPHome OTA compatibility."""
    firmware_path = get_firmware_path()
    ota_path = firmware_path.with_suffix(".ota.bin")

    if not firmware_path.is_file():
        _LOGGER.warning("Firmware not found: %s", firmware_path)
        return False

    shutil.copy(firmware_path, ota_path)
    _LOGGER.info("Created: %s", ota_path)
    return True
