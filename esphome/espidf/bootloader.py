"""Machine-global bootloader cache for the native ESP-IDF toolchain.

Only a bootloader proven byte reproducible (built twice on the first miss)
is cached; signing/secure/encryption builds keep the stock in-tree path.
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
from esphome.espidf import toolchain
from esphome.framework_helpers import _rename_with_retry
from esphome.helpers import read_json_file, rmtree, write_file

_LOGGER = logging.getLogger(__name__)

BOOTLOADER_CACHE_ENV = "ESPHOME_BOOTLOADER_CACHE"
# Bump when the build or storage recipe changes; old entries then miss.
_CACHE_SCHEMA = "1"
_SECURE_OPTION = re.compile(r"CONFIG_(SECURE_|FLASH_ENCRYPTION)")
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
        # The .esphomeinternal snapshot holds exactly what ESPHome and the
        # user configured; the live sdkconfig is rewritten by kconfgen with
        # every resolved option, including always-on *_SUPPORTED constants.
        snapshot = CORE.relative_build_path(f"sdkconfig.{CORE.name}.esphomeinternal")
        if not snapshot.is_file() or _snapshot_blocks_cache(snapshot):
            return False
        if CORE.relative_build_path("bootloader_components").exists():
            # Project-local bootloader overrides are inputs the key can't see.
            return False
        from esphome.espidf.framework import get_idf_tools_path

        if not os.access(get_idf_tools_path(), os.W_OK):
            # A read-only shared prefix would fail the cache on every build.
            return False
        from esphome.build_gen.espidf import idf_macro_matches

        if not idf_macro_matches():
            _LOGGER.info("IDF changed its bootloader macro; building in-tree")
            return False
        return True
    except (OSError, KeyError, ValueError, EsphomeError) as err:
        _LOGGER.info("Bootloader cache disabled: %s", err)
        return False


def _snapshot_blocks_cache(sdkconfig_path: Path) -> bool:
    """Whether the configured options rule the cache out up front.

    Secure options block because key files are inputs the cache key cannot
    see (rotating one at the same path changes the bootloader). Reproducible
    build off blocks so those users skip the doomed probe on every compile;
    the probe still guards against unknown nondeterminism.
    """
    reproducible = False
    for line in sdkconfig_path.read_text(encoding="utf-8").splitlines():
        name, _, value = line.partition("=")
        name = name.strip()
        value = value.strip().strip('"')
        if _SECURE_OPTION.match(name) and value not in _DISABLED_VALUES:
            return True
        if name == "CONFIG_APP_REPRODUCIBLE_BUILD":
            reproducible = value not in _DISABLED_VALUES
    return not reproducible


def tree_uses_cached_bootloader(build_dir: Path) -> bool:
    """Whether a configured tree was set up for the cached bootloader."""
    cmakecache = build_dir / "CMakeCache.txt"
    if not cmakecache.is_file():
        return False
    cache = toolchain._parse_cmakecache(cmakecache)
    return cache.get(toolchain.USE_CACHED_BOOTLOADER_DEFINE) == "1"


def _cache_root() -> Path:
    from esphome.espidf.framework import get_idf_tools_path

    return (
        get_idf_tools_path()
        / "bootloaders"
        / toolchain._get_core_framework_version()
        / toolchain._idf_target()
    )


def _load_config_names() -> list[str] | None:
    """The known union of config option names the bootloader consumes."""
    names = read_json_file(_cache_root() / "config_names.json")
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


def _key_payload(names: list[str], app_config: dict, compiler: str) -> dict:
    """Everything that can influence the built bootloader."""
    from esphome.espidf.framework import read_idf_version_txt

    return {
        "schema": _CACHE_SCHEMA,
        "idf": toolchain._get_core_framework_version(),
        "source": toolchain._get_framework_source_override() or "",
        "stamp": read_idf_version_txt(toolchain._get_idf_path()),
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


def _publish(build_dir: Path, key: str, payload: dict) -> Path:
    """Move the distilled outputs into the cache; losing a race is fine."""
    entry = _cache_root() / key
    stage = _work_dir(f".stage-{key}-")
    for name in _OUTPUTS:
        shutil.copy2(build_dir / name, stage / name)
    shutil.copy2(build_dir / "config" / "sdkconfig.json", stage / "sdkconfig.json")
    write_file(stage / "meta.json", json.dumps(payload, indent=2, sort_keys=True))
    stage.chmod(0o755)  # mkdtemp creates 0o700
    try:
        _rename_with_retry(stage, entry)
    except OSError:
        # Another process published the same key first.
        _remove_dir(stage)
        if not (entry / "bootloader.bin").is_file():
            raise
    return entry


def _remove_dir(path: Path) -> None:
    """Best-effort rmtree with the repo's read-only and retry hardening."""
    with contextlib.suppress(OSError):
        rmtree(path)


def _install_into_build(entry: Path) -> Path:
    """Copy the cached outputs to build/bootloader, where every consumer reads."""
    dest = toolchain._build_dir() / "bootloader"
    if (dest / "CMakeCache.txt").is_file():
        # A leftover in-tree sub-build; its ninja state must not linger.
        rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    for name in _OUTPUTS:
        if (src := entry / name).is_file():
            shutil.copy2(src, dest / name)
    return dest


def _bootloader_offset(app_config: dict) -> int:
    """The flash offset the bootloader is written to; callers check presence."""
    return int(app_config["BOOTLOADER_OFFSET_IN_FLASH"])


def _check_bootloader_size(bin_path: Path, app_config: dict) -> None:
    """The check_sizes.py bootloader check, without spawning a process."""
    offset = _bootloader_offset(app_config)
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
    with contextlib.suppress(OSError):
        for path in _cache_root().glob(".*"):
            if path.is_dir() and path.stat().st_mtime < cutoff:
                _remove_dir(path)


def ensure_cached_bootloader(verbose: bool = False) -> bool:
    """Put the cached bootloader into build/bootloader, building on a miss.

    False tells the caller to fall back to the in-tree build; a too-large
    bootloader raises instead, since in-tree would fail the same way.
    """
    app_config = toolchain._load_sdkconfig_json(toolchain._build_dir())
    compiler = toolchain._resolved_c_compiler()
    if (
        app_config is None
        or "BOOTLOADER_OFFSET_IN_FLASH" not in app_config
        or compiler is None
    ):
        _LOGGER.debug("Bootloader cache unusable: app configure outputs missing")
        return False
    try:
        dest = _install_cached(app_config, compiler, verbose)
    except (OSError, EsphomeError) as err:
        _LOGGER.warning("Bootloader cache failed: %s", err)
        return False
    if dest is None:
        return False
    # Outside the fail-safe net: the in-tree build would overflow the same way.
    _check_bootloader_size(dest / "bootloader.bin", app_config)
    return True


def _install_cached(app_config: dict, compiler: str, verbose: bool) -> Path | None:
    """The installed build/bootloader dir, building and publishing on a miss;
    None means fall back to the in-tree build."""
    if (names := _load_config_names()) is not None:
        entry = _cache_root() / _compute_key(_key_payload(names, app_config, compiler))
        if (entry / "bootloader.bin").is_file():
            return _install_into_build(entry)
    tmp = _work_dir(".build-")
    probe = _work_dir(".build-")
    try:
        if _build_standalone(tmp, verbose) != 0:
            return None
        if (built_config := toolchain._load_sdkconfig_json(tmp)) is None:
            _LOGGER.debug("Bootloader build produced no sdkconfig.json")
            return None
        # Only a build proven byte identical to a second one in a different
        # dir may be cached; timestamps or randomized signatures, present or
        # future, fail here instead of being served stale.
        if _build_standalone(probe, verbose) != 0:
            return None
        if (tmp / "bootloader.bin").read_bytes() != (
            probe / "bootloader.bin"
        ).read_bytes():
            _LOGGER.info("Bootloader build is not byte reproducible; not caching")
            return None
        names = _merge_config_names(built_config)
        payload = _key_payload(names, app_config, compiler)
        key = _compute_key(payload)
        entry = _publish(tmp, key, payload)
        _LOGGER.info("Cached bootloader %s for later builds", key)
    finally:
        _remove_dir(tmp)
        _remove_dir(probe)
    _prune_stale_dirs()
    return _install_into_build(entry)


def inject_bootloader_flash_file(flash_data: dict, build_dir: Path) -> None:
    """Add the cached bootloader to a flasher_args ``flash_files`` map.

    Gated on the tree's mode: IDF also omits the entry on purpose for some
    secure-boot builds, and those must stay exactly as IDF wrote them.
    """
    if not tree_uses_cached_bootloader(build_dir):
        return
    app_config = toolchain._load_sdkconfig_json(build_dir)
    if (
        app_config is None
        or "BOOTLOADER_OFFSET_IN_FLASH" not in app_config
        or not (build_dir / "bootloader" / "bootloader.bin").is_file()
    ):
        _LOGGER.error("Cached bootloader missing; factory image has no bootloader")
        return
    flash_files = flash_data.setdefault("flash_files", {})
    flash_files[hex(_bootloader_offset(app_config))] = "bootloader/bootloader.bin"
