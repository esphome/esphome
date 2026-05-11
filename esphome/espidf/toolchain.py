"""ESP-IDF direct build API for ESPHome."""

from dataclasses import dataclass, field
import json
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess

from esphome.components.esp32.const import KEY_ESP32, KEY_FLASH_SIZE, KEY_IDF_VERSION
from esphome.core import CORE, EsphomeError
from esphome.espidf.framework import check_esp_idf_install, get_framework_env

_LOGGER = logging.getLogger(__name__)

DOMAIN = "espidf_toolchain"


@dataclass
class _CacheData:
    paths: dict[str, tuple] = field(default_factory=dict)
    env: dict[str, dict[str, str]] = field(default_factory=dict)
    cmake_output: dict[Path, str] = field(default_factory=dict)
    cmake_tools: dict[Path, dict[str, Path]] = field(default_factory=dict)


def _cache() -> _CacheData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _CacheData()
    return CORE.data[DOMAIN]


def _get_core_framework_version():
    return str(CORE.data[KEY_ESP32][KEY_IDF_VERSION])


def _get_esphome_esp_idf_paths(
    version: str | None = None,
) -> tuple[os.PathLike, os.PathLike]:
    version = version or _get_core_framework_version()
    paths = _cache().paths
    if version not in paths:
        paths[version] = check_esp_idf_install(version)
    return paths[version]


def _get_idf_path(version: str | None = None) -> Path | None:
    """Get IDF_PATH from environment or common locations."""
    # Use provided IDF framework if available
    if "IDF_PATH" in os.environ:
        return Path(os.environ["IDF_PATH"])
    return Path(_get_esphome_esp_idf_paths(version)[0])


def _get_idf_env(version: str | None = None) -> dict[str, str]:
    """Get environment variables needed for ESP-IDF build."""
    version = version or _get_core_framework_version()
    env_cache = _cache().env
    if version not in env_cache:
        env_cache[version] = os.environ.copy()

        # Use provided IDF framework if available
        if "IDF_PATH" not in os.environ:
            env_cache[version] |= get_framework_env(
                *_get_esphome_esp_idf_paths(version)
            )
    return env_cache[version]


def _get_cmake_output(build_dir) -> str:
    cmake_output_cache = _cache().cmake_output
    if build_dir not in cmake_output_cache:
        cmd = ["cmake", "-LA", "-N", "."]

        env = _get_idf_env()
        result = subprocess.run(
            cmd,
            cwd=build_dir,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

        if result.returncode != 0:
            raise RuntimeError(f"CMake failed: {result.stderr}")

        cmake_output_cache[build_dir] = result.stdout
    return cmake_output_cache[build_dir]


def _get_cmake_tool_path(var_name: str) -> Path:
    build_dir = CORE.relative_build_path("build")
    cmake_output = _get_cmake_output(build_dir)

    cmake_tools_cache = _cache().cmake_tools
    if build_dir not in cmake_tools_cache:
        cmake_tools_cache[build_dir] = {}

    if var_name not in cmake_tools_cache[build_dir]:
        pattern = rf"^{var_name}:FILEPATH=(.+)$"
        match = re.search(pattern, cmake_output, re.MULTILINE)

        if not match:
            raise RuntimeError(f"{var_name} not found in CMake output")

        path = match.group(1).strip()
        cmake_tools_cache[build_dir][var_name] = Path(path)

    return cmake_tools_cache[build_dir][var_name]


def _get_idf_tool(name: str) -> str:
    """Return the path to an executable from the ESP-IDF environment PATH or raise if not found."""
    env = _get_idf_env()
    executable = shutil.which(name, path=env.get("PATH", None))
    if executable is None:
        raise EsphomeError(
            f"{name} executable not found in ESP-IDF environment. "
            "Check that the IDF environment is correctly set up."
        )
    return executable


def run_idf_py(
    *args, cwd: Path | None = None, capture_output: bool = False
) -> int | str:
    """Run idf.py with the given arguments."""
    idf_path = _get_idf_path()
    if idf_path is None:
        raise EsphomeError("ESP-IDF not found")

    env = _get_idf_env()
    python_executable = _get_idf_tool("python")
    idf_py = idf_path / "tools" / "idf.py"
    # Dispatch idf.py through esphome.espidf.runner, which wraps
    # sys.stdout/sys.stderr so ``isatty()`` reports True. This keeps CMake,
    # Ninja, and idf.py's own progress-bar code emitting TTY-format output
    # (``\r`` cursor moves, ANSI colors, fancy progress bars) even when our
    # real stdout is a pipe — e.g. when esphome is running under the Home
    # Assistant dashboard add-on. The runner is a plain script (not a
    # ``python -m`` module) because IDF's Python venv does not have the
    # esphome package installed.
    runner_py = Path(__file__).parent / "runner.py"

    cmd = [python_executable, str(runner_py), str(idf_py)] + list(args)

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


def _get_sdkconfig_args() -> list[str]:
    """Get cmake -D flags for the sdkconfig file, if it exists."""
    sdkconfig_path = CORE.relative_build_path(f"sdkconfig.{CORE.name}")
    if sdkconfig_path.is_file():
        return ["-D", f"SDKCONFIG={sdkconfig_path}"]
    return []


def run_reconfigure() -> int:
    """Run cmake reconfigure only (no build)."""
    return run_idf_py(*_get_sdkconfig_args(), "reconfigure")


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


def _patch_memory_segments():
    """Patch memory.ld to expand IRAM/DRAM for testing mode.

    Mirrors the PlatformIO iram_fix.py.script logic for native IDF builds.
    Must be called after cmake configure (which generates memory.ld) and
    before the build/link step.
    """
    # Same sizes as iram_fix.py.script
    testing_iram_size = 0x200000  # 2MB
    testing_dram_size = 0x200000  # 2MB

    memory_ld = CORE.relative_build_path(
        "build", "esp-idf", "esp_system", "ld", "memory.ld"
    )
    if not memory_ld.is_file():
        _LOGGER.warning("Could not find linker script at %s", memory_ld)
        return

    content = memory_ld.read_text()
    patches = []

    def _patch_segment(text, segment_name, new_size):
        pattern = rf"({re.escape(segment_name)}\s*\([^)]*\)\s*:\s*org\s*=\s*.+?,\s*len\s*=\s*)(\S+[^\n]*)"
        if match := re.search(pattern, text, re.DOTALL):
            replacement = f"{match.group(1)}{new_size:#x}"
            new_text = text[: match.start()] + replacement + text[match.end() :]
            if new_text != text:
                return new_text, True
        return text, False

    content, patched = _patch_segment(content, "iram0_0_seg", testing_iram_size)
    if patched:
        patches.append(f"IRAM={testing_iram_size:#x}")

    content, patched = _patch_segment(content, "dram0_0_seg", testing_dram_size)
    if patched:
        patches.append(f"DRAM={testing_dram_size:#x}")

    if patches:
        memory_ld.write_text(content)
        _LOGGER.info("Patched %s in %s for testing mode", ", ".join(patches), memory_ld)
    else:
        _LOGGER.warning("Could not patch memory segments in %s", memory_ld)


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
        if CORE.testing_mode:
            # Reconfigure again so cmake is up to date with the full component
            # list. This ensures idf.py build won't re-run cmake, which would
            # regenerate memory.ld and wipe the DRAM/IRAM patches applied below.
            rc = run_reconfigure()
            if rc != 0:
                _LOGGER.error("Reconfigure with discovered components failed")
                return rc

    # In testing mode, generate the linker script first, patch DRAM/IRAM sizes,
    # then build. memory.ld is regenerated by ninja during the build phase,
    # so we must patch after it's generated but before linking (same timing
    # as iram_fix.py.script's AddPreAction hook in the PlatformIO path).
    if CORE.testing_mode:
        memory_ld = CORE.relative_build_path(
            "build", "esp-idf", "esp_system", "ld", "memory.ld"
        )
        build_dir = CORE.relative_build_path("build")
        # Build just the memory.ld target - ninja needs the path relative to build dir
        memory_ld_target = os.path.relpath(str(memory_ld), str(build_dir))
        env = _get_idf_env()
        ninja_executable = _get_idf_tool("ninja")
        result = subprocess.run(
            [ninja_executable, "-C", str(build_dir), memory_ld_target],
            env=env,
            check=False,
        )
        if result.returncode != 0:
            _LOGGER.error("Failed to generate linker script")
            return result.returncode
        _patch_memory_segments()

    # Build
    args = []

    if verbose:
        args.append("-v")

    args.extend(_get_sdkconfig_args())
    args.append("build")

    return run_idf_py(*args)


def get_firmware_path() -> Path:
    """Get the path to the compiled firmware binary.

    This is the file idf.py writes directly (named after the project),
    not the copy used for OTA/factory downloads below.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / f"{CORE.name}.bin"


def get_factory_firmware_path() -> Path:
    """Get the path to the factory firmware (with bootloader).

    Uses the PlatformIO ``firmware.factory.bin`` naming convention so
    the dashboard's download handler — which requests files by name
    relative to ``firmware_bin_path.parent`` — finds it. Without this,
    the native IDF path produced ``<name>.factory.bin`` and the
    dashboard returned 500 trying to locate ``firmware.factory.bin``.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / "firmware.factory.bin"


def get_ota_firmware_path() -> Path:
    """Get the path to the OTA firmware binary.

    Uses the PlatformIO ``firmware.ota.bin`` naming convention for the
    same dashboard-compatibility reason as ``get_factory_firmware_path``.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / "firmware.ota.bin"


def get_elf_path() -> Path:
    """Get the path to the firmware ELF file.

    idf.py writes ``<build>/<name>.elf`` directly; this returns the
    ``<build>/firmware.elf`` copy created by ``create_elf_copy`` so
    the dashboard's "download ELF" link can find it under the
    PlatformIO-convention name.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / "firmware.elf"


def get_objdump_path() -> Path:
    return _get_cmake_tool_path("CMAKE_OBJDUMP")


def get_readelf_path() -> Path:
    return _get_cmake_tool_path("CMAKE_READELF")


def get_addr2line_path() -> Path:
    return _get_cmake_tool_path("CMAKE_ADDR2LINE")


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
    python_executable = _get_idf_tool("python")
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
    """Copy the firmware to firmware.ota.bin for ESPHome OTA compatibility."""
    firmware_path = get_firmware_path()
    ota_path = get_ota_firmware_path()

    if not firmware_path.is_file():
        _LOGGER.warning("Firmware not found: %s", firmware_path)
        return False

    shutil.copy(firmware_path, ota_path)
    _LOGGER.info("Created: %s", ota_path)
    return True


def create_elf_copy() -> bool:
    """Copy the ELF binary to firmware.elf for dashboard compatibility.

    idf.py writes the ELF at ``<build>/<name>.elf``; the dashboard's
    "download ELF" link requests the literal filename ``firmware.elf``
    (PlatformIO convention), so copy it to that name.
    """
    build_dir = CORE.relative_build_path("build")
    src_elf = build_dir / f"{CORE.name}.elf"
    dst_elf = get_elf_path()

    if not src_elf.is_file():
        _LOGGER.warning("ELF not found: %s", src_elf)
        return False

    shutil.copy(src_elf, dst_elf)
    _LOGGER.info("Created: %s", dst_elf)
    return True
