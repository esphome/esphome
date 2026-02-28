"""ESP-IDF direct build API for ESPHome."""

import json
import logging
import os
from pathlib import Path
import shutil
import subprocess

from esphome.components.esp32.const import KEY_ESP32, KEY_FLASH_SIZE
from esphome.core import CORE, EsphomeError
from esphome.espidf_framework import check_esp_idf_install, get_framework_env

_LOGGER = logging.getLogger(__name__)

# Caches
_esphome_esp_idf_paths_cache = {}
_idf_env_cache = {}


def _get_esphome_esp_idf_paths(
    version: str | None = None,
) -> tuple[os.PathLike, os.PathLike]:
    if version not in _esphome_esp_idf_paths_cache:
        _esphome_esp_idf_paths_cache[version] = check_esp_idf_install(version)
    return _esphome_esp_idf_paths_cache[version]


def _get_idf_path(version: str | None = None) -> Path | None:
    """Get IDF_PATH from environment or common locations."""
    # Use provided IDF framework if available
    if "IDF_PATH" in os.environ:
        return Path(os.environ["IDF_PATH"])
    return Path(_get_esphome_esp_idf_paths(version)[0])


def _get_idf_env(version: str | None = None) -> dict[str, str]:
    """Get environment variables needed for ESP-IDF build.

    Requires the user to have sourced export.sh before running esphome.
    """
    if version not in _idf_env_cache:
        _idf_env_cache[version] = os.environ

        # Use provided IDF framework if available
        if "IDF_PATH" not in os.environ:
            _idf_env_cache[version] |= get_framework_env(
                *_get_esphome_esp_idf_paths(version)
            )
    return _idf_env_cache[version]


def run_idf_py(
    *args, cwd: Path | None = None, capture_output: bool = False
) -> int | str:
    """Run idf.py with the given arguments."""
    idf_path = _get_idf_path()
    if idf_path is None:
        raise EsphomeError("ESP-IDF not found")

    env = _get_idf_env()
    python_executable = shutil.which("python", path=env.get("PATH", None))
    idf_py = idf_path / "tools" / "idf.py"

    cmd = [python_executable, str(idf_py)] + list(args)

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


def has_outdated_files():
    """Check if the build configuration is stale.

    Returns True if required build files are missing or if configuration inputs
    are newer than the generated CMake/Ninja build artifacts.
    """
    cmakecache_txt_path = CORE.relative_build_path("build/CMakeCache.txt")

    cmakelists_txt_build_path = CORE.relative_build_path("CMakeLists.txt")
    cmakelists_txt_src_path = CORE.relative_src_path("CMakeLists.txt")
    build_config_path = CORE.relative_build_path("build/config")
    sdkconfig_internal_path = CORE.relative_build_path(
        f"sdkconfig.{CORE.name}.esphomeinternal"
    )
    dependency_lock_path = CORE.relative_build_path("dependencies.lock")
    build_ninja_path = CORE.relative_build_path("build/build.ninja")

    if not os.path.isdir(build_config_path) or not os.listdir(build_config_path):
        return True
    if not os.path.isfile(cmakecache_txt_path):
        return True
    if not os.path.isfile(build_ninja_path):
        return True
    if os.path.isfile(dependency_lock_path) and os.path.getmtime(
        dependency_lock_path
    ) > os.path.getmtime(build_ninja_path):
        return True

    cmakecache_txt_mtime = os.path.getmtime(cmakecache_txt_path)
    return any(
        os.path.getmtime(f) > cmakecache_txt_mtime
        for f in [
            _get_idf_path(),
            cmakelists_txt_build_path,
            cmakelists_txt_src_path,
            sdkconfig_internal_path,
            build_config_path,
        ]
        if f and os.path.exists(f)
    )


def need_reconfigure() -> bool:
    from esphome.build_gen.espidf import has_discovered_components

    # We need to reconfigure either if the files are outdated or if there is no component discovered
    return has_outdated_files() or not has_discovered_components()


def run_compile(config, verbose: bool) -> int:
    """Compile the ESP-IDF project.

    Uses two-phase configure to auto-discover available components:
    1. If no previous build, configure with minimal REQUIRES to discover components
    2. Regenerate CMakeLists.txt with discovered components
    3. Run full build
    """
    from esphome.build_gen.espidf import write_project

    # Check if we need to do discovery phase
    if need_reconfigure():
        _LOGGER.info("Discovering available ESP-IDF components...")
        write_project(minimal=True)
        rc = run_reconfigure()
        if rc != 0:
            _LOGGER.error("Component discovery failed")
            return rc
        _LOGGER.info("Regenerating CMakeLists.txt with discovered components...")
        write_project(minimal=False)

    # Build
    args = []

    if verbose:
        args.append("-v")

    args.append("build")

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

    env = _get_idf_env()
    python_executable = shutil.which("python", path=env.get("PATH", None))

    cmd = [
        python_executable,
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
    result = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)

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
