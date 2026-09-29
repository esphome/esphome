"""Machine-global bootloader cache for the native ESP-IDF toolchain.

Only a bootloader built twice with identical bytes is cached; secure and
signing builds keep the stock in-tree path.
"""

# pylint: disable=protected-access
# This module is part of the espidf toolchain and shares its private helpers.

from __future__ import annotations

import contextlib
from dataclasses import dataclass
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import shutil
import tempfile
import time

from esphome.build_helpers.ccache import parse_enable_env
from esphome.build_helpers.tool_runner import run_build_tool
from esphome.core import CORE, EsphomeError
from esphome.espidf import framework, parse_sdkconfig, toolchain
from esphome.framework_helpers import _rename_with_retry
from esphome.helpers import (
    copy_file_if_changed,
    file_compare,
    read_json_file,
    rmtree,
    write_file,
)

_LOGGER = logging.getLogger(__name__)

BOOTLOADER_CACHE_ENV = "ESPHOME_BOOTLOADER_CACHE"
# Bump when the build or storage recipe changes; old entries then miss.
_CACHE_SCHEMA = "1"
_SECURE_OPTION = re.compile(r"CONFIG_(SECURE_|FLASH_ENCRYPTION)")
_BOOTLOADER_PROPERTY = re.compile(
    r"BOOTLOADER_(EXTRA_COMPONENT_DIRS|IGNORE_EXTRA_COMPONENT)"
)
_DISABLED_VALUES = frozenset(("n", "0", ""))
# Marks a config name the app's sdkconfig does not define.
_ABSENT = "\x00absent"
_OUTPUTS = ("bootloader.bin", "bootloader.elf", "bootloader.map")


def bootloader_cache_enabled() -> bool:
    """Whether this build may take its bootloader from the cache."""
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
        # The snapshot holds what was configured; the live sdkconfig is
        # rewritten by kconfgen with always-on *_SUPPORTED constants.
        snapshot = CORE.relative_build_path(f"sdkconfig.{CORE.name}.esphomeinternal")
        if not snapshot.is_file() or _snapshot_blocks_cache(snapshot):
            return False
        if CORE.relative_build_path("bootloader_components").exists():
            # Project-local bootloader overrides are inputs the key can't see.
            return False
        if _managed_bootloader_hook() is not None:
            return False
        if not os.access(tools := framework.get_idf_tools_path(), os.W_OK):
            # A read-only shared prefix would fail the cache on every build.
            _LOGGER.info("Bootloader cache off: %s is not writable", tools)
            return False
        from esphome.build_gen.espidf import idf_macro_matches

        if not idf_macro_matches():
            _LOGGER.info("IDF changed its bootloader macro; building in-tree")
            return False
        return True
    except (OSError, EsphomeError) as err:
        _LOGGER.info("Bootloader cache disabled: %s", err)
        return False
    except (KeyError, ValueError) as err:
        # More likely a regression than the environment; still fail safe.
        _LOGGER.warning("Bootloader cache disabled: %s", err)
        return False


def _managed_bootloader_hook() -> str | None:
    """The managed component wiring the bootloader build, or None.

    Checked again after configure: a cold tree has no managed_components
    yet when the enable check first runs.
    """
    managed = CORE.relative_build_path("managed_components")
    hooks = managed.glob("*/project_include.cmake") if managed.is_dir() else ()
    for hook in hooks:
        if _BOOTLOADER_PROPERTY.search(hook.read_text(encoding="utf-8")):
            _LOGGER.info("Bootloader cache off: %s customizes it", hook.parent.name)
            return hook.parent.name
    return None


def _snapshot_blocks_cache(sdkconfig_path: Path) -> bool:
    """Whether the configured options rule the cache out up front.

    Secure options block because key file contents are inputs the key cannot
    see; reproducible build off blocks to skip the doomed probe every compile.
    """
    config = parse_sdkconfig(sdkconfig_path)
    if any(
        _SECURE_OPTION.match(name) and value not in _DISABLED_VALUES
        for name, value in config.items()
    ):
        return True
    return config.get("CONFIG_APP_REPRODUCIBLE_BUILD", "") in _DISABLED_VALUES


def tree_uses_cached_bootloader(build_dir: Path) -> bool:
    """Whether a configured tree was set up for the cached bootloader."""
    cmakecache = build_dir / "CMakeCache.txt"
    if not cmakecache.is_file():
        return False
    cache = toolchain._parse_cmakecache(cmakecache)
    return cache.get(toolchain.USE_CACHED_BOOTLOADER_DEFINE) == "1"


def _cache_root() -> Path:
    return (
        framework.get_idf_tools_path()
        / "bootloaders"
        / toolchain._get_core_framework_version()
        / toolchain._idf_target()
    )


def _load_config_names() -> list[str] | None:
    """The known union of config option names the bootloader consumes."""
    path = _cache_root() / "config_names.json"
    names = read_json_file(path)
    if isinstance(names, list) and names and all(isinstance(n, str) for n in names):
        return names
    if names is not None:
        # The next merge rewrites the full union; this only costs a miss.
        _LOGGER.info("Ignoring corrupt %s", path)
    return None


def _merge_config_names(names) -> list[str]:
    """Union-merge harvested names; a corrupt file is replaced outright.

    write_file renames into place, and re-merging until the file covers our
    names keeps the union monotonic when two builds publish at once.
    """
    path = _cache_root() / "config_names.json"
    merged = sorted(names)
    for _ in range(3):
        merged = sorted(set(_load_config_names() or ()) | set(merged))
        write_file(path, json.dumps(merged, separators=(",", ":")))
        if set(_load_config_names() or ()) >= set(merged):
            break
    return merged


@dataclass(frozen=True, kw_only=True)
class _SubprojectContract:
    """How the pinned IDF's bootloader ExternalProject configures the subproject."""

    self_append_bootloader_dir: bool  # EXTRA_COMPONENT_DIRS gets the bootloader dir
    forward_idf_build_v2: bool  # IDF_BUILD_V2 forwarded (empty: never opted in)


_SUBPROJECT_5 = _SubprojectContract(
    self_append_bootloader_dir=True,
    forward_idf_build_v2=False,
)
_SUBPROJECT_6 = _SubprojectContract(
    self_append_bootloader_dir=False,
    forward_idf_build_v2=True,
)


def _subproject() -> _SubprojectContract:
    from esphome.components.esp32 import idf_version
    import esphome.config_validation as cv

    return _SUBPROJECT_6 if idf_version() >= cv.Version(6, 0, 0) else _SUBPROJECT_5


def _subproject_cmake_args(
    cmake: str, python: str, sdkconfig: str, idf_path: str, project_dir: str
) -> list[str]:
    """The configure argv, mirroring IDF's bootloader ExternalProject args."""
    contract = _subproject()
    extra_dirs = (
        f"{idf_path}/components/bootloader"
        if contract.self_append_bootloader_dir
        else ""
    )
    args = [
        cmake,
        "-G",
        "Ninja",
        f"-DSDKCONFIG={sdkconfig}",
        f"-DIDF_PATH={idf_path}",
        f"-DIDF_TARGET={toolchain._idf_target()}",
        "-DPYTHON_DEPS_CHECKED=1",
        f"-DPYTHON={python}",
        f"-DEXTRA_COMPONENT_DIRS={extra_dirs}",
    ]
    if contract.forward_idf_build_v2:
        args.append("-DIDF_BUILD_V2=")
    args += [
        f"-DPROJECT_SOURCE_DIR={project_dir}",
        "-DIGNORE_EXTRA_COMPONENT=",
        f"{idf_path}/components/bootloader/subproject",
    ]
    return args


def _key_payload(names: list[str], app_config: dict, compiler: str, stamp: str) -> dict:
    """Everything that can influence the built bootloader."""
    return {
        "schema": _CACHE_SCHEMA,
        "idf": toolchain._get_core_framework_version(),
        "source": toolchain._get_framework_source_override() or "",
        "stamp": stamp,
        "target": toolchain._idf_target(),
        "compiler": compiler,
        # Placeholder paths: any change to the invocation shape misses.
        "args": _subproject_cmake_args(
            "cmake", "@PYTHON@", "@SDKCONFIG@", "@IDF@", "@PROJECT@"
        ),
        "config": {n: app_config.get(n, _ABSENT) for n in sorted(names)},
    }


def _compute_key(payload: dict) -> str:
    """The cache entry name for this build's bootloader inputs."""
    text = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(text.encode()).hexdigest()[:16]


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


def _work_dir(prefix: str) -> Path:
    """A private work dir in the cache; unique across shared tools prefixes."""
    root = _cache_root()
    root.mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix=prefix, dir=root))


def _bin_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _entry_intact(entry: Path) -> bool:
    """Complete with the published bin bytes; anything else is a miss."""
    if not all((entry / name).is_file() for name in _OUTPUTS):
        return False
    meta = read_json_file(entry / "meta.json")
    expected = meta.get("bootloader_bin_sha256") if isinstance(meta, dict) else None
    return expected == _bin_sha256(entry / "bootloader.bin")


def _publish(build_dir: Path, key: str, payload: dict) -> Path:
    """Move the distilled outputs into the cache; losing a race is fine."""
    entry = _cache_root() / key
    if not _entry_intact(entry):
        # A partial or corrupted entry (manual cleanup) must not win.
        _remove_dir(entry)
    stage = _work_dir(f".stage-{key}-")
    try:
        for name in _OUTPUTS:
            shutil.copy2(build_dir / name, stage / name)
        # The resolved bootloader config, kept for debugging entries.
        shutil.copy2(build_dir / "config" / "sdkconfig.json", stage / "sdkconfig.json")
        meta = {
            **payload,
            "bootloader_bin_sha256": _bin_sha256(stage / "bootloader.bin"),
        }
        write_file(stage / "meta.json", json.dumps(meta, indent=2, sort_keys=True))
        stage.chmod(0o755)  # mkdtemp creates 0o700
        _rename_with_retry(stage, entry)
    except (OSError, EsphomeError) as err:
        # Failed to stage, or another process published the same key first.
        _remove_dir(stage)
        with contextlib.suppress(OSError):
            if _entry_intact(entry):
                _LOGGER.debug("Reusing published %s after: %s", entry, err)
                return entry
        raise  # keep the original error even if the intact check breaks
    return entry


def _remove_dir(path: Path) -> None:
    """Best-effort rmtree with the repo's read-only and retry hardening."""
    try:
        rmtree(path)
    except OSError as err:
        # Leftover work dirs are cleared by the daily prune; leave a trace.
        _LOGGER.debug("Could not remove %s: %s", path, err)


def _install_into_build(entry: Path) -> Path:
    """Copy the cached outputs to build/bootloader, where every consumer reads."""
    dest = toolchain._build_dir() / "bootloader"
    if (dest / "CMakeCache.txt").is_file():
        # A leftover in-tree sub-build; its ninja state must not linger.
        rmtree(dest)
    for name in _OUTPUTS:
        copy_file_if_changed(entry / name, dest / name)
    return dest


def _bootloader_offset(app_config: dict | None) -> int | None:
    """The flash offset the bootloader is written to, or None when unknown."""
    value = (app_config or {}).get("BOOTLOADER_OFFSET_IN_FLASH")
    return None if value is None else int(value)


def _check_bootloader_size(bin_path: Path, offset: int, table_offset: int) -> None:
    """The check_sizes.py bootloader check, without spawning a process."""
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
        paths = list(_cache_root().glob(".*"))
    except OSError as err:
        _LOGGER.debug("Prune skipped: %s", err)
        return
    for path in paths:
        # A racing build may remove its own dir mid-scan; skip just that one.
        with contextlib.suppress(OSError):
            if path.is_dir() and path.stat().st_mtime < cutoff:
                _remove_dir(path)


def ensure_cached_bootloader(verbose: bool = False) -> bool:
    """Put the cached bootloader into build/bootloader, building on a miss.

    False tells the caller to fall back to the in-tree build; a too-large
    bootloader raises instead, since in-tree would fail the same way.
    """
    if _managed_bootloader_hook() is not None:
        # Configure just downloaded it; the pre-configure scan ran too early.
        return False
    app_config = toolchain._load_sdkconfig_json(toolchain._build_dir())
    offset = _bootloader_offset(app_config)
    table_offset = (app_config or {}).get("PARTITION_TABLE_OFFSET")
    compiler = toolchain._resolved_c_compiler()
    if offset is None or table_offset is None or compiler is None:
        _LOGGER.info(
            "Bootloader cache unusable: offset=%s table_offset=%s compiler=%s",
            offset,
            table_offset,
            compiler,
        )
        return False
    try:
        dest = _install_cached(app_config, compiler, verbose)
    except (OSError, EsphomeError) as err:
        _LOGGER.warning("Bootloader cache failed: %s", err)
        return False
    if dest is None:
        return False
    # Outside the fail-safe net: the in-tree build would overflow the same way.
    _check_bootloader_size(dest / "bootloader.bin", offset, int(table_offset))
    return True


def _install_cached(app_config: dict, compiler: str, verbose: bool) -> Path | None:
    """The installed build/bootloader dir, building and publishing on a miss;
    None means fall back to the in-tree build."""
    if not (stamp := framework.read_idf_version_txt(toolchain._get_idf_path())):
        _LOGGER.info("Framework version.txt missing; cannot key the bootloader")
        return None
    if (names := _load_config_names()) is not None:
        entry = _cache_root() / _compute_key(
            _key_payload(names, app_config, compiler, stamp)
        )
        if _entry_intact(entry):
            return _install_into_build(entry)
    with contextlib.ExitStack() as cleanup:
        tmp = _work_dir(".build-")
        cleanup.callback(_remove_dir, tmp)
        if _build_standalone(tmp, verbose) != 0:
            return None
        if (built_config := toolchain._load_sdkconfig_json(tmp)) is None:
            # After a successful build this most likely means an IDF change.
            _LOGGER.warning(
                "Bootloader build produced no %s", tmp / "config" / "sdkconfig.json"
            )
            return None
        # Only a build byte identical to a second one in a different dir may
        # be cached; any nondeterminism fails here instead of serving stale.
        probe = _work_dir(".build-")
        cleanup.callback(_remove_dir, probe)
        if _build_standalone(probe, verbose) != 0:
            return None
        if not file_compare(tmp / "bootloader.bin", probe / "bootloader.bin"):
            _LOGGER.info("Bootloader build is not byte reproducible; not caching")
            return None
        names = _merge_config_names(built_config)
        payload = _key_payload(names, app_config, compiler, stamp)
        key = _compute_key(payload)
        entry = _publish(tmp, key, payload)
        _LOGGER.info("Cached bootloader %s for later builds", key)
    _prune_stale_dirs()
    return _install_into_build(entry)


def inject_bootloader_flash_file(flash_data: dict, build_dir: Path) -> bool:
    """Add the cached bootloader to a flasher_args ``flash_files`` map.

    Mode gated: IDF omits the entry on purpose for some secure-boot builds.
    False means a cached-mode tree is missing pieces; the image would not boot.
    """
    if not tree_uses_cached_bootloader(build_dir):
        return True
    offset = _bootloader_offset(toolchain._load_sdkconfig_json(build_dir))
    if offset is None or not (build_dir / "bootloader" / "bootloader.bin").is_file():
        _LOGGER.error("Cached bootloader missing; factory image has no bootloader")
        return False
    flash_data.setdefault("flash_files", {})[hex(offset)] = "bootloader/bootloader.bin"
    return True
