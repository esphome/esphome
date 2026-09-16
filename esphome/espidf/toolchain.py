"""ESP-IDF direct build API for ESPHome."""

from dataclasses import dataclass, field
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess

from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    KEY_ESP32,
    KEY_FLASH_SIZE,
    KEY_IDF_VERSION,
    KEY_VARIANT,
)
from esphome.core import CORE, EsphomeError
from esphome.espidf import variant_to_idf_target
from esphome.espidf.framework import check_esp_idf_install, get_framework_env
from esphome.espidf.size_summary import print_summary
from esphome.helpers import add_git_ceiling_directory, write_file

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


def _get_framework_source_override() -> str | None:
    """Return the user-supplied esp32.framework.source override, if any.

    The override lets a user point the IDF tarball download at a custom URL
    (mirror, fork, local server). Substitutions like ``{VERSION}`` /
    ``{MAJOR}`` etc. work the same as in the default mirror list.
    """
    if CORE.config is None:
        return None
    return CORE.config.get(KEY_ESP32, {}).get(CONF_FRAMEWORK, {}).get(CONF_SOURCE)


def _get_configured_targets() -> list[str] | None:
    """Return the IDF install target for the configured variant, if known.

    Limiting the toolchain install to the variant being built skips the other
    architecture's compiler entirely (several hundred MB of download and 1-2GB
    of disk). idf_tools.py accumulates targets across runs, so building a
    second variant later installs just its toolchain incrementally. None (no
    variant stored, e.g. tooling outside a build) falls back to the default
    inside check_esp_idf_install.

    CI always installs every target (None falls through to the "all"
    default): runners share one toolchain cache across jobs that build
    different variants, so a full install keeps the cached tree identical
    everywhere instead of per-variant supersets invalidating each other.
    """
    if os.environ.get("CI"):
        return None
    variant = CORE.data.get(KEY_ESP32, {}).get(KEY_VARIANT)
    return [variant_to_idf_target(variant)] if variant else None


def _get_esphome_esp_idf_paths(
    version: str | None = None,
) -> tuple[os.PathLike, os.PathLike]:
    version = version or _get_core_framework_version()
    paths = _cache().paths
    if version not in paths:
        paths[version] = check_esp_idf_install(
            version,
            targets=_get_configured_targets(),
            source_url=_get_framework_source_override(),
        )
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
        # Do not leak PYTHONPATH into child env
        env_cache[version].pop("PYTHONPATH", None)

        # Use provided IDF framework if available
        if "IDF_PATH" not in os.environ:
            env_cache[version] |= get_framework_env(
                *_get_esphome_esp_idf_paths(version)
            )

        # Cap git's repo search at the config directory so ESP-IDF's
        # `git describe` for the app version can't error out on an
        # uninitialized or corrupt git repo in a parent directory.
        add_git_ceiling_directory(env_cache[version], CORE.config_dir)
    return env_cache[version]


def _get_cmake_output(build_dir) -> str:
    cmake_output_cache = _cache().cmake_output
    if build_dir not in cmake_output_cache:
        # Check the build before resolving the env: _get_idf_env() runs
        # check_esp_idf_install(), which can download and install the whole
        # framework. Never start that for a build that isn't there. Callers
        # such as the log stack-trace decoder run against devices that were
        # never compiled on this machine.
        if not (Path(build_dir) / "CMakeCache.txt").is_file():
            raise EsphomeError(f"No ESP-IDF build found in {build_dir}")

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
    *args,
    cwd: Path | None = None,
    capture_output: bool = False,
    jobs: int | None = None,
) -> int | str:
    """Run idf.py with the given arguments."""
    idf_path = _get_idf_path()
    if idf_path is None:
        raise EsphomeError("ESP-IDF not found")

    env = _get_idf_env()
    if jobs is not None:
        env = {**env, "IDF_PY_BUILD_JOBS": str(jobs)}
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


def _builtin_component_cache_path() -> Path | None:
    """Cache file for this build's built-in component list.

    The file lives inside the extracted framework directory so it is
    discarded together with that exact checkout (re-extract, source
    override, clean-all); the target and the EXCLUDE_COMPONENTS set name it.
    The sdkconfig is not part of the key: IDF components register regardless
    of CONFIG_* options and only gate their sources on them. A checkout
    supplied through IDF_PATH is not managed by ESPHome and is never cached.
    """
    if "IDF_PATH" in os.environ:
        return None
    target = variant_to_idf_target(CORE.data[KEY_ESP32][KEY_VARIANT])
    excluded = CORE.cmake_args.get("EXCLUDE_COMPONENTS", "")
    excluded_key = hashlib.sha256(excluded.encode()).hexdigest()[:12]
    return (
        _get_idf_path() / ".esphome_component_lists" / f"{target}-{excluded_key}.json"
    )


def load_cached_builtin_components() -> list[str] | None:
    """Return the cached built-in component list for this build, if valid.

    Every name must still exist under ``$IDF_PATH/components`` so a stale
    entry is treated as a miss instead of failing the configure.
    """
    if (path := _builtin_component_cache_path()) is None:
        return None
    try:
        components = json.loads(path.read_text(encoding="utf-8"))
        present = {
            entry.name
            for entry in (path.parents[1] / "components").iterdir()
            if entry.is_dir()
        }
    except (OSError, ValueError):
        return None
    if (
        isinstance(components, list)
        and all(isinstance(c, str) for c in components)
        and present.issuperset(components)
    ):
        return components
    return None


def save_cached_builtin_components(components: list[str]) -> None:
    """Store a built-in component list that just configured successfully."""
    if not components or (path := _builtin_component_cache_path()) is None:
        return
    try:
        write_file(path, json.dumps(components, separators=(",", ":")))
    except EsphomeError as err:
        _LOGGER.warning("Could not write component list cache %s: %s", path, err)


def _write_project_and_reconfigure(builtin_components: list[str] | None) -> int:
    """Write the full CMakeLists.txt and run the configure for it."""
    from esphome.build_gen.espidf import write_project

    _LOGGER.info("Writing CMakeLists.txt with the built-in component list...")
    write_project(minimal=False, builtin_components=builtin_components)
    # Explicit reconfigure: ninja only re-runs cmake when CMakeLists.txt
    # is strictly newer than build.ninja, which fails on coarse-mtime
    # filesystems (#18682). Also keeps idf.py from regenerating memory.ld
    # in testing mode.
    return run_reconfigure()


def _configure_project() -> int:
    """Configure the project, discovering the built-in components if needed.

    A cached component list skips the discovery configure. If the configure
    with a cached list fails the entry is dropped and discovery runs once; a
    list is only cached after it configured successfully.
    """
    from esphome.build_gen.espidf import get_available_components, write_project

    if (cached := load_cached_builtin_components()) is not None:
        _LOGGER.info("Using cached ESP-IDF component list")
        if _write_project_and_reconfigure(cached) == 0:
            return 0
        _LOGGER.warning("Cached component list failed; rediscovering")
        _builtin_component_cache_path().unlink(missing_ok=True)
    _LOGGER.info("Discovering available ESP-IDF components...")
    write_project(minimal=True)
    if (rc := run_reconfigure()) != 0:
        _LOGGER.error("Component discovery failed")
        return rc
    discovered = get_available_components()
    if not discovered:
        _LOGGER.error("Component discovery found no built-in ESP-IDF components")
        return 1
    if (rc := _write_project_and_reconfigure(discovered)) != 0:
        _LOGGER.error("Reconfigure with discovered components failed")
        return rc
    save_cached_builtin_components(discovered)
    return 0


def has_outdated_files():
    """Check if the build configuration is stale.

    Returns True if required build files are missing or if ESPHome's
    resolved build inputs are newer than CMakeCache.txt:

    - ``sdkconfig.<name>.esphomeinternal`` -- the canonical "what state
      did ESPHome resolve the YAML to" snapshot. Any change in build
      flags, enabled components, framework version, or target ends up
      rewriting it (we embed a ``# ESPHOME_IDF_VERSION=`` comment line
      for the version case where the option set would otherwise be
      identical).
    - ``src/idf_component.yml`` -- the project manifest. Managed
      component additions/removals (e.g. via ``add_idf_component``) can
      happen without any sdkconfig impact, and ``_write_idf_component_yml``
      already deletes ``dependencies.lock`` on a change but that signal
      gets lost as soon as the lock is missing.
    - ``exclude_components.esphomeinternal`` -- the resolved
      EXCLUDE_COMPONENTS set. Excluded components never register in
      ``project_description.json``, so re-including one needs a fresh
      discovery pass before it can appear in the builtin-components
      property that ``src`` REQUIRES.

    We deliberately don't watch:
    - The top-level/src ``CMakeLists.txt`` -- ESPHome owns those, and
      ninja already tracks them as configure-time deps. Including them
      causes a perpetual reconfigure loop because CMake doesn't restamp
      ``CMakeCache.txt`` when only ``idf_build_set_property`` values
      change between configures.
    - ``$IDF_PATH`` and CMake's ``build/config/`` -- both have mtime
      semantics that fire after the wrong configure (or not at all in
      common cases like in-place IDF version replacement). The sdkconfig
      and manifest hashes subsume the meaningful signal.
    """
    cmakecache_txt_path = CORE.relative_build_path("build/CMakeCache.txt")
    build_config_path = CORE.relative_build_path("build/config")
    sdkconfig_internal_path = CORE.relative_build_path(
        f"sdkconfig.{CORE.name}.esphomeinternal"
    )
    idf_component_yml_path = CORE.relative_build_path("src/idf_component.yml")
    exclude_components_path = CORE.relative_build_path(
        "exclude_components.esphomeinternal"
    )
    dependency_lock_path = CORE.relative_build_path("dependencies.lock")
    build_ninja_path = CORE.relative_build_path("build/build.ninja")

    if not build_config_path.is_dir() or not any(build_config_path.iterdir()):
        return True
    if not cmakecache_txt_path.is_file():
        return True
    if not build_ninja_path.is_file():
        return True
    if (
        dependency_lock_path.is_file()
        and dependency_lock_path.stat().st_mtime > build_ninja_path.stat().st_mtime
    ):
        return True

    cmakecache_txt_mtime = cmakecache_txt_path.stat().st_mtime
    return any(
        f.stat().st_mtime > cmakecache_txt_mtime
        for f in [
            sdkconfig_internal_path,
            idf_component_yml_path,
            exclude_components_path,
        ]
        if f.exists()
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
    1. If no previous build, configure with minimal REQUIRES to discover
       components (skipped when a cached list for this IDF/target/exclusion
       set exists)
    2. Regenerate CMakeLists.txt with discovered components
    3. Run full build
    """
    # Check if we need to do discovery phase
    if not need_reconfigure():
        _LOGGER.info("Build configuration is up to date")
    else:
        if (rc := _configure_project()) != 0:
            return rc
        # cmake does not rewrite CMakeCache.txt when only properties change,
        # so restamp it or every build repeats discovery. Only after success,
        # or a failed reconfigure would be marked fresh. build.ninja is
        # restamped too so the cache is not newer and ninja does not
        # re-run cmake.
        for name in ("build/CMakeCache.txt", "build/build.ninja"):
            path = CORE.relative_build_path(name)
            if path.is_file():
                os.utime(path)

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
    args.append("size")

    rc = run_idf_py(*args, jobs=config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT))
    if rc == 0:
        size_json = CORE.relative_build_path("build", "esp_idf_size.json")
        partitions = CORE.relative_build_path("partitions.csv")
        print_summary(size_json, partitions if partitions.is_file() else None)
    return rc


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


def get_idedata() -> dict | None:
    """Derive idedata from the build's compile_commands.json.

    The native ESP-IDF toolchain has no ``pio run -t idedata`` equivalent, but
    its CMake build emits ``build/compile_commands.json``. Parse that into the
    idedata fields IDE integrations and clang-tidy expect, cached alongside the
    PlatformIO idedata path. Returns None if the compile DB doesn't exist yet.
    """
    from esphome.build_helpers.idedata import load_or_build_idedata

    # No launcher: CMake excludes CMAKE_<LANG>_COMPILER_LAUNCHER (ccache)
    # from the exported compile database, unlike ninja's compdb dump.
    return load_or_build_idedata(
        CORE.relative_build_path("build", "compile_commands.json"),
        get_elf_path(),
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
    )


def create_factory_bin() -> bool:
    """Create factory.bin by merging bootloader, partition table, and app."""
    build_dir = CORE.relative_build_path("build")
    flasher_args_path = build_dir / "flasher_args.json"

    if not flasher_args_path.is_file():
        _LOGGER.warning("flasher_args.json not found, cannot create factory.bin")
        return False

    try:
        with flasher_args_path.open(encoding="utf-8") as f:
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
