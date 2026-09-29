"""Machine-global bootloader cache for the native ESP-IDF toolchain.

The bootloader is its own CMake project that depends only on the IDF version,
target, compiler and the config subset its reduced Kconfig universe defines,
so one build serves every device that shares those. With
``CONFIG_APP_REPRODUCIBLE_BUILD`` (always on) the result is byte identical to
the in-tree ExternalProject build. Builds that configure signing, secure boot
or flash encryption never use the cache: their bootloader can depend on key
file contents, and they keep the stock in-tree path.
"""

from __future__ import annotations

import hashlib
import json
import logging
import os
from pathlib import Path
import re
import shutil
import time

from esphome.build_helpers.ccache import parse_enable_env
from esphome.build_helpers.tool_runner import run_build_tool
from esphome.const import KEY_ESP32, KEY_VARIANT
from esphome.core import CORE, EsphomeError
from esphome.espidf import toolchain, variant_to_idf_target
from esphome.helpers import write_file

_LOGGER = logging.getLogger(__name__)

BOOTLOADER_CACHE_ENV = "ESPHOME_BOOTLOADER_CACHE"
# Bump on any change to how cached bootloaders are built or stored; old
# entries then miss instead of being served stale.
_CACHE_SCHEMA = "1"
_SECURE_OPTION = re.compile(r"CONFIG_(SECURE_|FLASH_ENCRYPTION)")
_MACRO = re.compile(
    r"macro\(__build_process_project_includes\)(.*?)endmacro\(\)", re.DOTALL
)
# Marks a config name the app's sdkconfig does not define; distinct from
# every real value.
_ABSENT = "\x00absent"
_OUTPUTS = ("bootloader.bin", "bootloader.elf", "bootloader.map")


def bootloader_cache_enabled() -> bool:
    """Whether this build may take its bootloader from the cache.

    False keeps IDF's in-tree bootloader build, exactly as before.
    """
    cache = toolchain._cache()
    if cache.bootloader_enabled is None:
        cache.bootloader_enabled = _compute_enabled()
    return cache.bootloader_enabled


def _compute_enabled() -> bool:
    if parse_enable_env(BOOTLOADER_CACHE_ENV) is False:
        return False
    if "IDF_PATH" in os.environ:
        # An unmanaged IDF checkout can change under the cache.
        return False
    try:
        # The .esphomeinternal snapshot holds exactly what ESPHome and the
        # user configured; the live sdkconfig is rewritten by kconfgen with
        # every resolved option, including always-on *_SUPPORTED constants.
        snapshot = CORE.relative_build_path(f"sdkconfig.{CORE.name}.esphomeinternal")
        if not snapshot.is_file() or _has_secure_options(snapshot):
            return False
        return idf_macro_matches()
    except (OSError, KeyError, ValueError, EsphomeError) as err:
        _LOGGER.debug("Bootloader cache disabled: %s", err)
        return False


def _has_secure_options(sdkconfig_path: Path) -> bool:
    """Whether any signing, secure boot or encryption option is enabled.

    Reads ESPHome's sdkconfig snapshot, so raw ``sdkconfig_options`` count.
    A disabled value (``n``/``0``/empty) is the compiled-out state ESPHome
    writes by default and is safe to cache; anything else bypasses.
    """
    for line in sdkconfig_path.read_text(encoding="utf-8").splitlines():
        name, _, value = line.partition("=")
        if _SECURE_OPTION.match(name.strip()) and value.strip().strip('"') not in (
            "n",
            "0",
            "",
        ):
            return True
    return False


def _normalized_macro(text: str) -> list[str] | None:
    """The macro body as comment-free, whitespace-collapsed lines."""
    if (match := _MACRO.search(text)) is None:
        return None
    return [
        re.sub(r"\s+", " ", line)
        for raw in match.group(1).splitlines()
        if (line := raw.split("#", 1)[0].strip())
    ]


def idf_macro_matches() -> bool:
    """Whether IDF's macro still matches the copy the override replays.

    A mismatch after an IDF update silently falls back to the in-tree
    bootloader build; script/check_idf_py_equivalence.py reports it loudly.
    """
    from esphome.build_gen.espidf import (
        BOOTLOADER_OVERRIDE_ADDED_LINE,
        IDF_BOOTLOADER_OVERRIDE,
    )

    expected = _normalized_macro(IDF_BOOTLOADER_OVERRIDE)
    expected.remove(BOOTLOADER_OVERRIDE_ADDED_LINE)
    build_cmake = toolchain._get_idf_path() / "tools" / "cmake" / "build.cmake"
    live = _normalized_macro(build_cmake.read_text(encoding="utf-8"))
    return live == expected


def _idf_target() -> str:
    return variant_to_idf_target(CORE.data[KEY_ESP32][KEY_VARIANT])


def _cache_root() -> Path:
    from esphome.espidf.framework import get_idf_tools_path

    return (
        get_idf_tools_path()
        / "bootloaders"
        / toolchain._get_core_framework_version()
        / _idf_target()
    )


def _load_config_names() -> list[str] | None:
    """The known union of config option names the bootloader consumes."""
    try:
        names = json.loads(
            (_cache_root() / "config_names.json").read_text(encoding="utf-8")
        )
    except (OSError, ValueError):
        return None
    if isinstance(names, list) and names and all(isinstance(n, str) for n in names):
        return names
    return None


def _merge_config_names(names) -> list[str]:
    """Union-merge harvested names; the list only ever grows."""
    merged = sorted(set(_load_config_names() or ()) | set(names))
    write_file(
        _cache_root() / "config_names.json",
        json.dumps(merged, separators=(",", ":")),
    )
    return merged


def _load_build_config(build_dir: Path) -> dict | None:
    """A build's generated config/sdkconfig.json, or None."""
    try:
        return json.loads(
            (build_dir / "config" / "sdkconfig.json").read_text(encoding="utf-8")
        )
    except (OSError, ValueError):
        return None


def _compiler_id() -> str | None:
    """The app build's C compiler, resolved; the toolchain part of the key.

    IDF's toolchain file sets the compiler, so CMakeCache.txt has no plain
    CMAKE_C_COMPILER entry; the compile database names it instead. None
    means the cache cannot be trusted for this run and the caller falls
    back to the in-tree build.
    """
    try:
        commands = json.loads(
            (toolchain._build_dir() / "compile_commands.json").read_text(
                encoding="utf-8"
            )
        )
        compiler = commands[0]["command"].split()[0]
    except (OSError, ValueError, LookupError):
        cache_path = toolchain._build_dir() / "CMakeCache.txt"
        if not cache_path.is_file():
            return None
        # Same install dir and version as the compiler itself.
        compiler = toolchain._parse_cmakecache(cache_path).get("CMAKE_C_COMPILER_AR")
    return os.path.realpath(compiler) if compiler else None


def _subproject_cmake_args(
    cmake: str, python: str, sdkconfig: str, idf_path: str, project_dir: str
) -> list[str]:
    """The configure argv, mirroring the ExternalProject in IDF's
    components/bootloader/project_include.cmake for the pinned version."""
    from esphome.components.esp32 import idf_version
    import esphome.config_validation as cv

    args = [
        cmake,
        "-G",
        "Ninja",
        f"-DSDKCONFIG={sdkconfig}",
        f"-DIDF_PATH={idf_path}",
        f"-DIDF_TARGET={_idf_target()}",
        "-DPYTHON_DEPS_CHECKED=1",
        f"-DPYTHON={python}",
    ]
    if idf_version() >= cv.Version(6, 0, 0):
        # 6.x: no BOOTLOADER_EXTRA_COMPONENT_DIRS self-append; IDF_BUILD_V2
        # forwarded (empty: the env opt-in is never set).
        args += ["-DEXTRA_COMPONENT_DIRS=", "-DIDF_BUILD_V2="]
    else:
        args.append(f"-DEXTRA_COMPONENT_DIRS={idf_path}/components/bootloader")
    args += [
        f"-DPROJECT_SOURCE_DIR={project_dir}",
        "-DIGNORE_EXTRA_COMPONENT=",
        f"{idf_path}/components/bootloader/subproject",
    ]
    return args


def _version_stamp() -> str:
    """The installed framework's version.txt, part of the key."""
    try:
        return (
            (toolchain._get_idf_path() / "version.txt")
            .read_text(encoding="utf-8")
            .strip()
        )
    except OSError:
        return ""


def _compute_key(names: list[str], app_config: dict, compiler: str) -> str:
    """The cache entry name for this build's bootloader inputs."""
    payload = json.dumps(
        {
            "schema": _CACHE_SCHEMA,
            "idf": toolchain._get_core_framework_version(),
            "source": toolchain._get_framework_source_override() or "",
            "stamp": _version_stamp(),
            "target": _idf_target(),
            "compiler": compiler,
            # Placeholder paths: any change to the invocation shape misses.
            "args": _subproject_cmake_args(
                "cmake", "@PYTHON@", "@SDKCONFIG@", "@IDF@", "@PROJECT@"
            ),
            "config": {n: app_config.get(n, _ABSENT) for n in sorted(names)},
        },
        sort_keys=True,
        separators=(",", ":"),
    )
    return hashlib.sha256(payload.encode()).hexdigest()[:16]


def _build_standalone(build_dir: Path, verbose: bool) -> int:
    """Configure and build the bootloader subproject into ``build_dir``."""
    build_dir.mkdir(parents=True, exist_ok=True)
    args = _subproject_cmake_args(
        toolchain._get_idf_tool("cmake"),
        toolchain._get_idf_tool("python"),
        str(CORE.relative_build_path(f"sdkconfig.{CORE.name}")),
        str(toolchain._get_idf_path()),
        os.path.realpath(CORE.build_path),
    )
    filter_lines = None if verbose else toolchain.FILTER_IDF_LINES
    for name, cmd in (("cmake", args), ("ninja", [toolchain._get_idf_tool("ninja")])):
        log_path = build_dir / "log" / f"{name}_output.log"
        rc = run_build_tool(
            cmd,
            cwd=build_dir,
            env=toolchain._tool_env(),
            filter_lines=filter_lines,
            log_path=log_path,
        )
        if rc != 0:
            _LOGGER.error("Bootloader %s failed with exit code %d", name, rc)
            toolchain._print_hints(log_path)
            return rc
    return 0


def _publish(build_dir: Path, key: str, meta: dict) -> Path:
    """Move the distilled outputs into the cache; losing a race is fine."""
    entry = _cache_root() / key
    if (entry / "bootloader.bin").is_file():
        return entry
    stage = _cache_root() / f".stage-{key}-{os.getpid()}"
    stage.mkdir(parents=True, exist_ok=True)
    for name in _OUTPUTS:
        shutil.copy2(build_dir / name, stage / name)
    shutil.copy2(build_dir / "config" / "sdkconfig.json", stage / "sdkconfig.json")
    write_file(stage / "meta.json", json.dumps(meta, indent=2, sort_keys=True))
    try:
        stage.rename(entry)
    except OSError:
        # Another process published the same key first.
        shutil.rmtree(stage, ignore_errors=True)
        if not (entry / "bootloader.bin").is_file():
            raise
    return entry


def _install_into_build(entry: Path) -> None:
    """Copy the cached outputs to build/bootloader, the path every consumer
    (factory image, OTA --bootloader, the CI guard) already reads."""
    dest = toolchain._build_dir() / "bootloader"
    if (dest / "CMakeCache.txt").is_file():
        # A leftover in-tree sub-build; its ninja state must not linger.
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    for name in _OUTPUTS:
        if (src := entry / name).is_file():
            shutil.copy2(src, dest / name)


def _check_bootloader_size(bin_path: Path, app_config: dict) -> None:
    """The check_sizes.py bootloader check, without spawning a process."""
    offset = int(app_config.get("BOOTLOADER_OFFSET_IN_FLASH", 0))
    table_offset = int(app_config.get("PARTITION_TABLE_OFFSET", 0x8000))
    size = bin_path.stat().st_size
    free = table_offset - offset - size
    if free < 0:
        raise EsphomeError(
            f"Bootloader binary size {size:#x} bytes is too large for "
            f"partition table offset {table_offset:#x} (overflows by "
            f"{-free:#x} bytes). Increase CONFIG_PARTITION_TABLE_OFFSET."
        )
    _LOGGER.info(
        "Bootloader binary size %#x bytes. %#x bytes (%d%%) free.",
        size,
        free,
        round(100 * free / (table_offset - offset)),
    )


def _prune_stale_dirs() -> None:
    """Drop day-old work dirs from crashed builds; live ones are younger."""
    cutoff = time.time() - 86400
    try:
        for path in _cache_root().glob(".*"):
            if path.is_dir() and path.stat().st_mtime < cutoff:
                shutil.rmtree(path, ignore_errors=True)
    except OSError:
        pass


def ensure_cached_bootloader(verbose: bool = False) -> int:
    """Put the cached bootloader into build/bootloader, building on a miss.

    Returns 0 on success. A nonzero tool exit code tells the caller to fall
    back to the in-tree build; a too-large bootloader raises instead, since
    the in-tree build would fail the same way.
    """
    app_config = _load_build_config(toolchain._build_dir())
    compiler = _compiler_id()
    if app_config is None or compiler is None:
        _LOGGER.debug("Bootloader cache unusable: app configure outputs missing")
        return 1
    entry = None
    if (names := _load_config_names()) is not None:
        candidate = _cache_root() / _compute_key(names, app_config, compiler)
        if (candidate / "bootloader.bin").is_file():
            entry = candidate
    if entry is None:
        tmp = _cache_root() / f".build-{os.getpid()}"
        try:
            if (rc := _build_standalone(tmp, verbose)) != 0:
                return rc
            if (built_config := _load_build_config(tmp)) is None:
                _LOGGER.debug("Bootloader build produced no sdkconfig.json")
                return 1
            names = _merge_config_names(built_config)
            key = _compute_key(names, app_config, compiler)
            entry = _publish(
                tmp,
                key,
                {
                    "schema": _CACHE_SCHEMA,
                    "idf": toolchain._get_core_framework_version(),
                    "target": _idf_target(),
                    "compiler": compiler,
                },
            )
            _LOGGER.info("Cached bootloader %s for later builds", key)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        _prune_stale_dirs()
    _install_into_build(entry)
    _check_bootloader_size(
        toolchain._build_dir() / "bootloader" / "bootloader.bin", app_config
    )
    return 0


def inject_bootloader_flash_file(flash_data: dict, build_dir: Path) -> None:
    """Add the cached bootloader to a flasher_args ``flash_files`` map.

    In cached mode IDF writes no bootloader entry; stock and bypass trees
    already carry one, which makes this a no-op there.
    """
    flash_files = flash_data.setdefault("flash_files", {})
    if any("bootloader/" in name for name in flash_files.values()):
        return
    if not (build_dir / "bootloader" / "bootloader.bin").is_file():
        return
    if (app_config := _load_build_config(build_dir)) is None:
        return
    offset = hex(int(app_config.get("BOOTLOADER_OFFSET_IN_FLASH", 0)))
    flash_files[offset] = "bootloader/bootloader.bin"
