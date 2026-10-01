"""ESP-IDF direct build API for ESPHome."""

from dataclasses import dataclass, field
import fnmatch
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess

from esphome.build_helpers.tool_runner import run_build_tool
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
from esphome.espidf.component_mirror import component_mirror_env, sync_component_mirror
from esphome.espidf.framework import check_esp_idf_install, get_framework_env
from esphome.espidf.size_summary import print_summary
from esphome.helpers import add_git_ceiling_directory, get_bool_env, rmtree, write_file

_LOGGER = logging.getLogger(__name__)

DOMAIN = "espidf_toolchain"
# The -D that tells the generated CMakeLists to skip the in-tree bootloader
# build; also read back from CMakeCache.txt to identify a tree.
SKIP_BOOTLOADER_DEFINE = "ESPHOME_SKIP_BOOTLOADER"


@dataclass
class _CacheData:
    paths: dict[str, tuple] = field(default_factory=dict)
    env: dict[str, dict[str, str]] = field(default_factory=dict)
    cmake_output: dict[Path, str] = field(default_factory=dict)
    skip_bootloader: bool | None = None
    cmake_tools: dict[Path, dict[str, Path]] = field(default_factory=dict)
    mirror_sync_failed: bool = False


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


def _esphome_manages_idf() -> bool:
    """A checkout supplied through IDF_PATH is the user's, not ESPHome's."""
    return "IDF_PATH" not in os.environ


def _get_idf_env(version: str | None = None) -> dict[str, str]:
    """Get environment variables needed for ESP-IDF build."""
    version = version or _get_core_framework_version()
    env_cache = _cache().env
    if version not in env_cache:
        env_cache[version] = os.environ.copy()
        # Do not leak PYTHONPATH into child env
        env_cache[version].pop("PYTHONPATH", None)

        # Use provided IDF framework if available
        if _esphome_manages_idf():
            env_cache[version] |= get_framework_env(
                *_get_esphome_esp_idf_paths(version)
            )
            # Serve the component manager from the local registry mirror.
            env_cache[version] |= component_mirror_env()

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

        # Resolve to an absolute path: Windows locates a child process
        # through the parent's PATH, not the env passed to it.
        cmd = [_get_idf_tool("cmake"), "-LA", "-N", "."]

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


# Lines dropped from cmake and ninja output unless ``-v`` is given; matched
# with ``re.match`` against the line without ANSI codes or trailing space.
FILTER_IDF_LINES: list[str] = [
    # Full component path and linker script lists, one giant line each.
    r"-- Component paths:",
    r"-- Adding linker script ",
    r"-- Components:",
    # Component manager notices; progress dots can precede them.
    r"\.*NOTICE: ",
    # esp_idf_size banner and trailing note around the size table.
    r"\s*Memory Type Usage Summary",
    r"Note: The reported total sizes may be smaller than those in the",
    r"\s*$",
    # esphome-libs tarballs have no .git, so IDF's commit probes fail noisily.
    r"-- git rev-parse returned ",
    r"fatal: not a git repository",
    r"Stopping at filesystem boundary",
]

# click's boolean spellings, which idf.py applies to IDF_CCACHE_ENABLE.
_CLICK_TRUE = frozenset({"1", "true", "t", "yes", "y", "on"})
_CMAKECACHE_LINE = re.compile(r"^([^#/:=]+):([^:=]+)=(.*)$")


@dataclass(frozen=True, kw_only=True)
class _IdfPyContract:
    """How the pinned idf.py drives cmake and ninja (tools/idf_py_actions)."""

    binary_dir_arg: bool  # cmake gets -B <build dir>
    ccache_as_bool: bool  # CCACHE_ENABLE=True/False instead of 1/0
    size_ng: bool  # size target gets ESP_IDF_SIZE_NG=1


_IDF_PY_5 = _IdfPyContract(
    binary_dir_arg=False,
    ccache_as_bool=False,
    size_ng=True,
)
_IDF_PY_6 = _IdfPyContract(
    binary_dir_arg=True,
    ccache_as_bool=True,
    size_ng=False,
)


def _idf_py() -> _IdfPyContract:
    from esphome.components.esp32 import idf_version
    import esphome.config_validation as cv

    return _IDF_PY_6 if idf_version() >= cv.Version(6, 0, 0) else _IDF_PY_5


def _build_dir() -> Path:
    """The CMake binary dir; idf.py resolves the project dir the same way."""
    return Path(os.path.realpath(CORE.build_path)) / "build"


def _cache_entries() -> dict[str, str]:
    """The ``-D`` entries idf.py passes to cmake, in idf.py's order."""
    entries = {}
    sdkconfig_path = CORE.relative_build_path(f"sdkconfig.{CORE.name}")
    if sdkconfig_path.is_file():
        entries["SDKCONFIG"] = str(sdkconfig_path)
    ccache = _get_idf_env().get("IDF_CCACHE_ENABLE", "").strip().lower() in _CLICK_TRUE
    entries["CCACHE_ENABLE"] = str(ccache if _idf_py().ccache_as_bool else int(ccache))
    return entries


def _parse_cmakecache(path: Path) -> dict[str, str]:
    """Map each ``NAME:TYPE=VALUE`` line of a CMakeCache.txt to NAME: VALUE."""
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if m := _CMAKECACHE_LINE.match(line):
            result[m.group(1)] = m.group(3)
    return result


def _cache_entries_changed() -> bool:
    """True when a ``-D`` entry is missing from or differs in CMakeCache.txt.

    idf.py reconfigures on this before every build; ESPHome's own staleness
    check does not cover it (for example ccache switched on or off). ESPHome
    also compares ``PYTHON``, so a moved IDF prefix reconfigures; idf.py
    stopped with an error instead.
    """
    cache_path = _build_dir() / "CMakeCache.txt"
    if not cache_path.is_file():
        return True
    cache = _parse_cmakecache(cache_path)
    return any(cache.get(k) != v for k, v in _configure_defines().items())


def tree_skips_bootloader(build_dir: Path) -> bool:
    """Whether a configured tree was set up to skip the bootloader build.

    Total: an unreadable tree reads as the stock full build.
    """
    try:
        cache = _parse_cmakecache(build_dir / "CMakeCache.txt")
    except (OSError, ValueError) as err:
        _LOGGER.debug("Cannot read %s, assuming a full build: %s", build_dir, err)
        return False
    return cache.get(SKIP_BOOTLOADER_DEFINE) == "1"


def _skip_bootloader() -> bool:
    """Whether this tree should not build a bootloader at all; per-run memo."""
    cache = _cache()
    if cache.skip_bootloader is None:
        cache.skip_bootloader = _compute_skip_bootloader()
    return cache.skip_bootloader


def _compute_skip_bootloader() -> bool:
    if not CORE.skip_bootloader:
        return False
    from esphome.build_gen.espidf import idf_macro_matches

    if not idf_macro_matches(_get_idf_path()):
        _LOGGER.warning(
            "--skip-bootloader ignored: IDF changed its bootloader macro; "
            "building the bootloader"
        )
        return False
    return True


def missing_image_hint() -> str | None:
    """Why an expected flash image is absent, for upload error messages."""
    if tree_skips_bootloader(_build_dir()):
        return "this build was compiled with --skip-bootloader; recompile without it"
    return None


def _configure_defines() -> dict[str, str]:
    """Every ``-D`` idf.py passes to cmake, in its order."""
    return {
        "PYTHON_DEPS_CHECKED": "1",
        "PYTHON": _get_idf_tool("python"),
        "ESP_PLATFORM": "1",
        **_cache_entries(),
        # ESPHome's own switch; idf.py never passes it and cmake keeps the
        # cached value, so idf.py runs against the tree stay in the same mode.
        SKIP_BOOTLOADER_DEFINE: "1" if _skip_bootloader() else "0",
    }


def _tool_env() -> dict[str, str]:
    """The IDF env plus color, as idf.py 6.x gives every tool.

    Also used on 5.x (which forced CLICOLOR_FORCE for ninja only); color
    changes only what is printed, and this way NO_COLOR is respected.
    """
    env = dict(_get_idf_env())
    if not env.get("NO_COLOR"):
        env.setdefault("CLICOLOR_FORCE", "1")
        env.setdefault("FORCE_COLOR", "1")
    return env


def run_reconfigure(verbose: bool = False) -> int:
    """Run the CMake configure, with the arguments idf.py uses."""
    build_dir = _build_dir()
    build_dir.mkdir(parents=True, exist_ok=True)
    if _skip_bootloader() and not tree_skips_bootloader(build_dir):
        # Flipping into skip mode: full-mode leftovers are stale for
        # OTA --bootloader and downloads, and a partial cleanup would
        # poison the flip back (deleted byproducts never regenerate).
        for stale in ("bootloader", "bootloader-prefix"):
            if (path := build_dir / stale).is_dir():
                rmtree(path)
        get_factory_firmware_path().unlink(missing_ok=True)
    # First, so the configure (even a first solve) installs from the mirror.
    _sync_component_mirror()
    cmd = [_get_idf_tool("cmake"), "-G", "Ninja"]
    if _idf_py().binary_dir_arg:
        cmd += ["-B", str(build_dir)]
    cmd += [f"-D{name}={value}" for name, value in _configure_defines().items()]
    cmd.append(str(build_dir.parent))
    log_path = build_dir / "log" / "cmake_output.log"
    rc = run_build_tool(
        cmd,
        cwd=build_dir,
        env=_tool_env(),
        filter_lines=None if verbose else FILTER_IDF_LINES,
        log_path=log_path,
    )
    if rc != 0:
        # As idf.py does: a partial cache must not look configured.
        (build_dir / "CMakeCache.txt").unlink(missing_ok=True)
        _LOGGER.error("CMake configure failed with exit code %d", rc)
        _print_hints(log_path)
        return rc
    # Mirror what the solve added to the lock (transitive dependencies).
    _sync_component_mirror()
    return rc


def _size_env() -> dict[str, str]:
    """Environment idf.py gives the ``size`` target."""
    env = {"ESP_IDF_SIZE_FORCE_TERMINAL": "1", "SIZE_OUTPUT_FORMAT": "default"}
    if _idf_py().size_ng:
        env["ESP_IDF_SIZE_NG"] = "1"
    return env


def _build_jobs(config) -> int | None:
    """Ninja's -j: compile_process_limit, else IDF_PY_BUILD_JOBS as idf.py read it."""
    if (limit := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT)) is not None:
        return limit
    if not (value := os.environ.get("IDF_PY_BUILD_JOBS")):
        return None
    try:
        jobs = int(value)
    except ValueError:
        jobs = 0
    if jobs <= 0:
        raise EsphomeError("IDF_PY_BUILD_JOBS must be a positive integer")
    return jobs


def _run_ninja(
    target: str,
    *,
    verbose: bool,
    jobs: int | None,
    progress: bool = False,
    extra_env: dict[str, str] | None = None,
) -> int:
    """Build one ninja target, with the flags and env idf.py uses."""
    cmd = [_get_idf_tool("ninja")]
    if jobs is not None:
        cmd += ["-j", str(jobs)]
    if verbose:
        cmd.append("-v")
    cmd.append(target)
    log_path = _build_dir() / "log" / f"ninja_{Path(target).name}_output.log"
    rc = run_build_tool(
        cmd,
        cwd=_build_dir(),
        env={**_tool_env(), **(extra_env or {})},
        filter_lines=None if verbose else FILTER_IDF_LINES,
        progress=progress and not verbose,
        log_path=log_path,
    )
    if rc != 0:
        _LOGGER.error("ninja %s failed with exit code %d", target, rc)
        _print_hints(log_path)
    return rc


# Runs IDF's own hint matcher (hints.yml plus its hint modules) on a failed
# tool's output, as idf.py did; it only lives in the IDF venv.
_HINTS_SCRIPT = """
import sys
sys.path.insert(0, sys.argv[1])
from idf_py_actions.tools import generate_hints
for hint in generate_hints(sys.argv[2]):
    print(hint)
"""


def _print_hints(log_path: Path) -> None:
    """Print ESP-IDF's advice for a failed build; never fails the build itself."""
    try:
        result = subprocess.run(
            [
                _get_idf_tool("python"),
                "-c",
                _HINTS_SCRIPT,
                str(_get_idf_path() / "tools"),
                str(log_path),
            ],
            env=_get_idf_env(),
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )
    except (OSError, subprocess.SubprocessError, EsphomeError) as err:
        _LOGGER.debug("Could not get ESP-IDF hints: %s", err)
        return
    if result.returncode != 0:
        _LOGGER.debug("Could not get ESP-IDF hints:\n%s", result.stderr)
        return
    if hints := result.stdout.strip():
        _LOGGER.warning("%s", hints)


def _sync_component_mirror() -> None:
    """Best-effort update of the local registry mirror; never fails the build."""
    if not _esphome_manages_idf():
        # _get_idf_env injects no mirror env, so nothing would read a sync.
        return
    cache = _cache()
    if cache.mirror_sync_failed:
        return
    if not sync_component_mirror(
        CORE.relative_build_path("dependencies.lock"),
        CORE.relative_build_path("src/idf_component.yml"),
        lambda: _get_idf_tool("python"),
        _get_idf_env,
    ):
        # One failed attempt (e.g. offline) is enough per run.
        cache.mirror_sync_failed = True


def _builtin_component_cache_path() -> Path | None:
    """Cache file for this build's built-in component list.

    The file lives inside the extracted framework directory so it is
    discarded together with that exact checkout (re-extract, source
    override, clean-all); the target and the EXCLUDE_COMPONENTS set name it.
    The sdkconfig is not part of the key: IDF components register regardless
    of CONFIG_* options and only gate their sources on them. A checkout
    supplied through IDF_PATH is not managed by ESPHome and is never cached.
    """
    if not _esphome_manages_idf():
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


def _write_project_and_reconfigure(
    builtin_components: list[str] | None, verbose: bool = False
) -> int:
    """Write the full CMakeLists.txt and run the configure for it."""
    from esphome.build_gen.espidf import write_project

    _LOGGER.info("Writing CMakeLists.txt with the built-in component list...")
    write_project(minimal=False, builtin_components=builtin_components)
    # Explicit reconfigure: ninja only re-runs cmake when CMakeLists.txt
    # is strictly newer than build.ninja, which fails on coarse-mtime
    # filesystems (#18682). Also keeps ninja from regenerating memory.ld
    # in testing mode.
    return run_reconfigure(verbose)


def _configure_project(verbose: bool = False) -> int:
    """Configure the project, discovering the built-in components if needed.

    A cached component list skips the discovery configure. If the configure
    with a cached list fails the entry is dropped and discovery runs once; a
    list is only cached after it configured successfully.
    """
    from esphome.build_gen.espidf import get_available_components, write_project

    if (cached := load_cached_builtin_components()) is not None:
        _LOGGER.info("Using cached ESP-IDF component list")
        if _write_project_and_reconfigure(cached, verbose) == 0:
            return 0
        _LOGGER.warning("Cached component list failed; rediscovering")
        _builtin_component_cache_path().unlink(missing_ok=True)
    _LOGGER.info("Discovering available ESP-IDF components...")
    write_project(minimal=True)
    if (rc := run_reconfigure(verbose)) != 0:
        _LOGGER.error("Component discovery failed")
        return rc
    discovered = get_available_components()
    if not discovered:
        _LOGGER.error("Component discovery found no built-in ESP-IDF components")
        return 1
    if (rc := _write_project_and_reconfigure(discovered, verbose)) != 0:
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


_LDGEN_FRAGMENTS_RE = re.compile(r'--fragments-list\s+"([^"]+)"')
_LDGEN_ARCHIVE_RE = re.compile(r"^\s*archive:\s*(\S+)", re.MULTILINE)


def _fragment_maps_app_archive(text: str) -> bool:
    """True when an archive: spec selects libsrc.a, the archive of the src
    component excluded as idf::src/__idf_src in build_gen/espidf.py.

    The bare * is IDF's stock catch-all; its archive-level entries resolve
    in the linker against all link inputs, so it stays safe when the
    archive is excluded from ldgen's own inputs.
    """
    return any(
        value != "*" and fnmatch.fnmatch("libsrc.a", value)
        for value in _LDGEN_ARCHIVE_RE.findall(text)
    )


def _ldgen_check_skip(msg: str, strict: bool) -> None:
    """A skipped fragment check is debug for users, fatal under strict."""
    if strict:
        raise EsphomeError(f"ldgen fragment check: {msg} (ESPHOME_LDGEN_STRICT)")
    _LOGGER.debug("Skipping ldgen fragment check: %s", msg)


def _warn_if_app_archive_mapped() -> None:
    """Belt for the ldgen exclusion (see build_gen/espidf.py): warn if any
    linker fragment names the app archive, since ldgen would silently skip
    remapping it rather than fail.
    """
    strict = get_bool_env("ESPHOME_LDGEN_STRICT")
    build_ninja = CORE.relative_build_path("build", "build.ninja")
    try:
        ninja_text = build_ninja.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        _ldgen_check_skip(f"could not read {build_ninja}: {e}", strict)
        return
    match = _LDGEN_FRAGMENTS_RE.search(ninja_text)
    if match is None:
        _ldgen_check_skip(f"no --fragments-list in {build_ninja}", strict)
        return
    for fragment in match.group(1).split(";"):
        try:
            text = Path(fragment).read_text(encoding="utf-8", errors="replace")
        except OSError as e:
            _ldgen_check_skip(f"could not read {fragment}: {e}", strict)
            continue
        if _fragment_maps_app_archive(text):
            msg = (
                f"Linker fragment {fragment} maps the app archive; its "
                "entries may be skipped. Set ESPHOME_LDGEN_FULL_DEPS=1 "
                "and rebuild."
            )
            if strict:
                raise EsphomeError(msg)
            _LOGGER.warning("%s", msg)
            return


def run_compile(config, verbose: bool) -> int:
    """Compile the ESP-IDF project.

    Uses two-phase configure to auto-discover available components:
    1. If no previous build, configure with minimal REQUIRES to discover
       components (skipped when a cached list for this IDF/target/exclusion
       set exists)
    2. Regenerate CMakeLists.txt with discovered components
    3. Run full build
    """
    jobs = _build_jobs(config)
    if need_reconfigure():
        if (rc := _configure_project(verbose)) != 0:
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
    elif _cache_entries_changed():
        _LOGGER.info("CMake cache options changed, reconfiguring")
        if (rc := run_reconfigure(verbose)) != 0:
            return rc
    else:
        _LOGGER.info("Build configuration is up to date")
        # Ninja can still re-run cmake on its own; keep the mirror current.
        _sync_component_mirror()

    if not get_bool_env("ESPHOME_LDGEN_FULL_DEPS"):
        _warn_if_app_archive_mapped()

    # In testing mode, generate the linker script first, patch DRAM/IRAM sizes,
    # then build. memory.ld is regenerated by ninja during the build phase,
    # so we must patch after it's generated but before linking (same timing
    # as iram_fix.py.script's AddPreAction hook in the PlatformIO path).
    if CORE.testing_mode:
        memory_ld = str(Path("esp-idf", "esp_system", "ld", "memory.ld"))
        if (rc := _run_ninja(memory_ld, verbose=verbose, jobs=jobs)) != 0:
            return rc
        _patch_memory_segments()

    from esphome.build_gen.espidf import write_pch_checksum

    write_pch_checksum()

    # idf.py's ``build size``, minus the second ``ninja all`` it runs first.
    rc = _run_ninja("all", verbose=verbose, jobs=jobs, progress=True)
    if rc == 0:
        rc = _run_ninja("size", verbose=verbose, jobs=jobs, extra_env=_size_env())
    if rc == 0:
        size_json = CORE.relative_build_path("build", "esp_idf_size.json")
        partitions = CORE.relative_build_path("partitions.csv")
        print_summary(size_json, partitions, get_built_elf_path())
    return rc


def get_firmware_path() -> Path:
    """Get the path to the compiled firmware binary.

    This is the file the build writes directly (named after the project),
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


def get_built_elf_path() -> Path:
    """Path to the ELF the build writes directly, ``<build>/<name>.elf``.

    Exists as soon as the build finishes, unlike the ``firmware.elf``
    copy that ``create_elf_copy`` makes later.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / f"{CORE.name}.elf"


def get_elf_path() -> Path:
    """Get the path to the firmware ELF file.

    The build writes ``<build>/<name>.elf`` directly; this returns the
    ``<build>/firmware.elf`` copy created by ``create_elf_copy`` so
    the dashboard's "download ELF" link can find it under the
    PlatformIO-convention name.
    """
    build_dir = CORE.relative_build_path("build")
    return build_dir / "firmware.elf"


def get_cmake_cache_value(var_name: str) -> str | None:
    """One entry of the configured build's CMake cache, or None when unset."""
    cmake_output = _get_cmake_output(CORE.relative_build_path("build"))
    match = re.search(rf"^{var_name}:\w+=(.*)$", cmake_output, re.MULTILINE)
    return match.group(1).strip() if match else None


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
    if tree_skips_bootloader(build_dir):
        # Nothing to merge, and nothing stale: the flip into skip mode
        # already removed the factory image and the sub-build.
        _LOGGER.info("Bootloader skipped; no factory image")
        return True
    if _merge_factory_bin(build_dir):
        return True
    # Never leave an image that does not match this build.
    get_factory_firmware_path().unlink(missing_ok=True)
    return False


def _merge_factory_bin(build_dir: Path) -> bool:
    """Run the esptool merge for a full-build tree."""
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
        if not file_path.is_file():
            # A partial factory image would not boot; never write one.
            _LOGGER.error("Flash file not found: %s", file_path)
            return False
        sections.extend([addr, str(file_path)])

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

    The build writes the ELF at ``<build>/<name>.elf``; the dashboard's
    "download ELF" link requests the literal filename ``firmware.elf``
    (PlatformIO convention), so copy it to that name.
    """
    src_elf = get_built_elf_path()
    dst_elf = get_elf_path()

    if not src_elf.is_file():
        _LOGGER.warning("ELF not found: %s", src_elf)
        return False

    shutil.copy(src_elf, dst_elf)
    _LOGGER.info("Created: %s", dst_elf)
    return True
