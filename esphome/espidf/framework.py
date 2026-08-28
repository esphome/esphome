"""ESP-IDF framework tools for ESPHome."""

from collections.abc import Callable
from ctypes.util import find_library
import json
import logging
import os
from pathlib import Path
import platform
import re
import shutil
from typing import Any, NoReturn

from esphome.build_helpers.ccache import (
    ccache_defaults_env,
    parse_enable_env,
    resolve_ccache_path,
)
from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.core import Version
from esphome.framework_helpers import (
    PathType,
    create_venv,
    download_and_extract,
    download_from_mirrors,
    failure_reason,
    get_python_env_executable_path,
    get_system_python_path,
    resume_fetch_job,
    rmdir,
    run_batch_downloads,
    run_command,
    run_command_ok,
    str_to_lst_of_str,
    tool_version_runs,
    warn_prefetch_failures,
)
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

_SCRIPTS_DIR = Path(__file__).parent


ESPHOME_STAMP_FILE = ".esphome.stamp.json"

# Cache-buster baked into the stamp file. Bump this whenever a change would
# make pre-existing stamped installs invalid, e.g.:
#   - the inlined Python helpers (_get_idf_version, _get_idf_tool_paths) are
#     rewritten in a way that's incompatible with prior installs
#   - the stamp_info schema changes (keys added/renamed/removed)
#   - the tool selection or env-construction logic changes meaning
# Bumping triggers a full reinstall on every user's next run.
STAMP_SCHEMA_VERSION = "0"

ESPHOME_IDF_DEFAULT_TARGETS = str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TARGETS", "all")
)
# An explicitly set ESPHOME_IDF_DEFAULT_TARGETS overrides the per-variant
# targets a caller requests, so a builder image can still pre-warm every
# target with one env var.
_IDF_DEFAULT_TARGETS_EXPLICIT = bool(os.environ.get("ESPHOME_IDF_DEFAULT_TARGETS"))

ESPHOME_IDF_DEFAULT_TOOLS = str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TOOLS", "cmake;ninja")
)

ESPHOME_IDF_DEFAULT_TOOLS_FORCE = str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TOOLS_FORCE", "required")
)

ESPHOME_IDF_DEFAULT_FEATURES = str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_FEATURES", "core")
)

ESPHOME_IDF_FRAMEWORK_MIRRORS = str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_FRAMEWORK_MIRRORS")
    or [
        "https://github.com/esphome-libs/esp-idf/releases/download/v{VERSION}/esp-idf-v{VERSION}.tar.xz",
        "https://github.com/esphome-libs/esp-idf/releases/download/v{SHORT_VERSION}/esp-idf-v{SHORT_VERSION}.tar.xz",
    ]
)

ESP_IDF_CONSTRAINTS_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESP_IDF_CONSTRAINTS_MIRRORS",
        "https://dl.espressif.com/dl/esp-idf/espidf.constraints.v{VERSION}.txt",
    )
)


def get_idf_tools_path() -> Path:
    """
    Get the path to the ESP-IDF tools directory.

    Returns:
        Path object pointing to the ESP-IDF tools directory
    """
    # Machine-global so all projects share the multi-GB install instead of
    # a per-config-directory copy; see build_helpers.tools_cache.tools_cache_path
    # for the env-override and normalization rules.
    return tools_cache_path(*IDF_TOOLS_CACHE)


# Windows' default MAX_PATH is 260 characters. ESP-IDF toolchains nest deeply
# below the IDF tools directory: the longest file on disk (picolibc C++
# headers) sits ~209 characters down, but the operative number is worse -- gcc
# probes its multilib include dirs via un-normalized self-relative paths
# ("bin/../lib/gcc/<target>/<ver>/../../../../<target>/include/..."), and
# Windows checks the path string as given, before collapsing "..". Measured
# worst case (riscv32, esp-15.2.0, longest multilib + no-rtti, probing
# bits/c++config.h): ~243 characters below the tools directory. Exceeding the
# limit surfaces as cryptic build failures -- missing headers ("fatal error:
# bits/c++config.h: No such file or directory") or partial extraction
# ("cannot execute 'as'"). Warn up front so the user can shorten the path or
# enable long path support.
_WINDOWS_MAX_PATH = 260
# Measured 243 plus a small safety margin for future toolchain growth.
_TOOLCHAIN_NESTED_PATH_LEN = 245


def _windows_long_paths_enabled() -> bool:
    """Return True if Windows long path support is enabled in the registry."""
    try:
        import winreg  # pylint: disable=import-error  # Windows-only module

        with winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SYSTEM\CurrentControlSet\Control\FileSystem",
        ) as key:
            value, _ = winreg.QueryValueEx(key, "LongPathsEnabled")
            return value == 1
    except OSError:
        return False


def _check_windows_path_length() -> None:
    """Warn when the install path is too long for Windows' MAX_PATH limit.

    No-op off Windows or when long path support is enabled. Otherwise warns if
    the deepest toolchain file would exceed the 260-character limit, which makes
    ESP-IDF toolchains extract incompletely and fail to build.
    """
    if platform.system() != "Windows" or _windows_long_paths_enabled():
        return
    tools_path = str(get_idf_tools_path())
    projected = len(tools_path) + _TOOLCHAIN_NESTED_PATH_LEN
    if projected <= _WINDOWS_MAX_PATH:
        return
    _LOGGER.warning(
        "ESP-IDF tools path is too long for the default Windows path limit:\n"
        "  %s (%d characters)\n"
        "ESP-IDF toolchain paths reach up to ~%d characters deeper (including the\n"
        "compiler's internal 'bin/../lib/...' relative paths), projecting to ~%d\n"
        "characters -- over the %d-character limit. This causes cryptic build\n"
        "failures such as:\n"
        "  fatal error: bits/c++config.h: No such file or directory\n"
        "  cannot execute 'as': CreateProcess: No such file or directory\n"
        "To fix, either:\n"
        "  - Enable Windows long path support, then reboot. In an elevated\n"
        "    PowerShell run:\n"
        "      Set-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\FileSystem' LongPathsEnabled 1\n"
        "    Details: https://learn.microsoft.com/windows/win32/fileio/maximum-file-path-limitation\n"
        "  - Or set ESPHOME_ESP_IDF_PREFIX to a shorter path (e.g. C:\\ESPHome\\idf)\n"
        "Then delete the ESP-IDF tools directory above so the toolchain "
        "reinstalls cleanly.",
        tools_path,
        len(tools_path),
        _TOOLCHAIN_NESTED_PATH_LEN,
        projected,
        _WINDOWS_MAX_PATH,
    )


def _get_framework_path(version: str) -> Path:
    """
    Get the path to the ESPHome ESP-IDF framework directory for a specific version.

    Args:
        version: ESP-IDF version string

    Returns:
        Path object pointing to the framework directory
    """
    return get_idf_tools_path() / "frameworks" / f"{version}"


def _get_python_env_path(version: str) -> Path:
    """
    Get the path to the ESPHome ESP-IDF Python environment directory for a specific version.

    Args:
        version: ESP-IDF version string

    Returns:
        Path object pointing to the Python environment directory
    """
    return get_idf_tools_path() / "penvs" / f"{version}"


def _read_stamp(file: PathType) -> dict | None:
    """Return a stamp file's dict contents, or None if missing or invalid.

    A missing stamp is the normal first-install case and stays silent; the
    other branches indicate a real fault that forces a full reinstall on
    every build, so they warn.
    """
    try:
        with Path(file).open(encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        return None
    except json.JSONDecodeError as e:
        _LOGGER.warning("Ignoring corrupt stamp file %s: %s", file, e)
        return None
    except OSError as e:
        _LOGGER.warning("Could not read stamp file %s: %s", file, e)
        return None
    if not isinstance(data, dict):
        _LOGGER.warning(
            "Ignoring stamp file %s with unexpected type %s",
            file,
            type(data).__name__,
        )
        return None
    return data


def _check_stamp(file: PathType, data: dict[str, Any]) -> bool:
    """
    Check if a stamp file contains the expected data.

    Args:
        file: Path to the stamp file
        data: Dictionary containing expected data

    Returns:
        True if file exists and contains expected data, False otherwise
    """
    return _read_stamp(file) == data


def _stamps_match_except_targets(stored: dict, requested: dict) -> bool:
    """Whether two stamps agree on every field other than ``targets``.

    Compares whole dicts (minus ``targets``) rather than named keys so any
    stamp field added later participates in invalidation by default instead
    of being silently ignored.
    """

    def _strip(stamp: dict) -> dict:
        return {k: v for k, v in stamp.items() if k != "targets"}

    return _strip(stored) == _strip(requested)


def _stamp_covers(stored: dict | None, requested: dict) -> bool:
    """Return True if a stored framework stamp already covers this request.

    Every field except ``targets`` must match exactly. ``targets`` may be a
    superset of the requested ones: ``idf_tools.py install`` accumulates
    targets in idf-env.json across runs, so a framework installed for more
    targets than this build needs is still valid. A stored ``all`` covers
    every target.
    """
    if stored is None:
        return False
    if not _stamps_match_except_targets(stored, requested):
        return False
    stored_targets = stored.get("targets")
    if not isinstance(stored_targets, list):
        return False
    return "all" in stored_targets or set(requested["targets"]) <= set(stored_targets)


def _write_stamp(file: PathType, data: dict[str, Any]):
    """
    Write data to a stamp file in JSON format.

    Args:
        file: Path to the stamp file to write
        data: Dictionary containing data to write
    """
    with Path(file).open("w", encoding="utf8") as fp:
        json.dump(data, fp)


def _run_idf_tools_script(
    idf_framework_root: PathType,
    script_name: str,
    msg: str,
    args: list[str] | None = None,
    env: dict[str, str] | None = None,
) -> tuple[bool, str | None, str | None]:
    """Run one of the sibling idf_tools-backed helper scripts.

    The script is executed with the framework's ``tools`` directory on
    PYTHONPATH so it imports the framework's own ``idf_tools`` module.
    """
    cmd = [
        get_system_python_path(),
        str(_SCRIPTS_DIR / script_name),
        str(idf_framework_root),
        *(args or []),
    ]
    return run_command(
        cmd,
        msg=msg,
        env=(env or os.environ)
        | {"PYTHONPATH": str(Path(idf_framework_root) / "tools")},
    )


def _raise_script_failure(what: str, root: PathType, stderr: str | None) -> NoReturn:
    """Raise RuntimeError for a failed helper script, appending stderr detail."""
    detail = (stderr or "").strip()
    raise RuntimeError(
        f"Can't get {what} of {root}" + (f": {detail}" if detail else "")
    )


def _get_idf_version(
    idf_framework_root: PathType, env: dict[str, str] | None = None
) -> str:
    """
    Get the ESP-IDF version from the specified framework root.

    Args:
        idf_framework_root: Path to the ESP-IDF framework root directory
        env: Optional dictionary of environment variables to set

    Returns:
        String containing ESP-IDF version

    Raises:
        RuntimeError: If ESP-IDF version cannot be determined
    """

    success, stdout, stderr = _run_idf_tools_script(
        idf_framework_root, "get_idf_version.py", "ESP-IDF version", env=env
    )
    if stdout:
        stdout = stdout.strip()
    if not success or not stdout:
        _raise_script_failure("ESP-IDF version", idf_framework_root, stderr)
    return stdout


def _get_idf_tool_paths(
    idf_framework_root: PathType, env: dict[str, str] | None = None
) -> tuple[list[str], dict[str, str]]:
    """
    Get ESP-IDF tool paths and environment variables needed for building.

    Args:
        idf_framework_root: Path to the ESP-IDF framework root directory
        env: Optional dictionary of environment variables to set

    Returns:
        tuple containing (list of tool paths, dictionary of environment variables)

    Raises:
        RuntimeError: If ESP-IDF tool paths cannot be determined
    """

    success, stdout, stderr = _run_idf_tools_script(
        idf_framework_root, "get_idf_tool_paths.py", "ESP-IDF tool paths", env=env
    )
    if not success or not stdout:
        _raise_script_failure("ESP-IDF tool paths", idf_framework_root, stderr)

    # Extract json values
    try:
        data = json.loads(stdout)
        return data["paths_to_export"], data["export_vars"]
    except Exception as e:
        raise RuntimeError(
            f"Can't extract ESP-IDF tool paths of {idf_framework_root}"
        ) from e


def _get_python_version(
    python_executable: PathType,
    env: dict[str, str] | None = None,
    throw_exception=False,
) -> str | None:
    """
    Get the Python version from the specified executable.

    Args:
        python_executable: Path to the Python executable to check
        env: Optional dictionary of environment variables to set
        throw_exception: If True, raise RuntimeError when version can't be determined

    Returns:
        String containing Python version in "major.minor.patch" format, or None if failed
    """

    script = """
import sys
print(".".join([str(x) for x in sys.version_info]))
"""
    cmd = [python_executable, "-c", script]

    success, stdout, _ = run_command(cmd, msg="Python version", env=env)

    if stdout:
        stdout = stdout.strip()
    if throw_exception and (not success or not stdout):
        raise RuntimeError(f"Can't get Python version of {python_executable}")
    return stdout


_GITHUB_SHORTHAND_RE = re.compile(
    r"^github://([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+?)(?:[@#]([a-zA-Z0-9\-_.\./]+))?$"
)
_GITHUB_HTTPS_RE = re.compile(
    r"^(https://github\.com/[a-zA-Z0-9\-]+/[a-zA-Z0-9\-\._]+?\.git)(?:[@#]([a-zA-Z0-9\-_.\./]+))?$"
)


def _parse_git_source(source_url: str) -> tuple[str, str | None] | None:
    """Return ``(url, ref)`` for ``github://owner/repo[@ref]`` or
    ``https://github.com/owner/repo.git[@ref]``, else ``None``.

    The ref may be separated with ``@`` or ``#``; ``#`` matches the PlatformIO
    convention used for ``platform_version`` URLs."""
    if m := _GITHUB_SHORTHAND_RE.match(source_url):
        owner, repo, ref = m.group(1), m.group(2), m.group(3)
        # Tolerate a trailing ".git" on the shorthand repo so the
        # github://owner/repo.git form doesn't silently become repo.git.git.
        repo = repo.removesuffix(".git")
        return f"https://github.com/{owner}/{repo}.git", ref
    if m := _GITHUB_HTTPS_RE.match(source_url):
        return m.group(1), m.group(2)
    return None


def _clone_idf_with_submodules(
    framework_path: Path, git_url: str, ref: str | None
) -> None:
    """Shallow-clone ESP-IDF with submodules into ``framework_path``.

    GitHub's archive zip strips submodules, so vendored components
    (mbedtls, openthread, esptool, ...) come down empty and CMake fails.

    Uses clone + ``fetch FETCH_HEAD`` + ``reset --hard`` instead of
    ``--branch``: ``--branch`` only accepts branch or tag names, but a
    user can also point at a commit SHA. The fetch-then-reset pattern
    handles branches, tags, and SHAs uniformly (mirrors the approach in
    ``esphome.git.clone_or_update``).
    """
    from esphome.git import run_git_command, update_submodules

    key = f"{git_url}@{ref}" if ref else git_url
    _LOGGER.info("Cloning ESP-IDF from %s", key)
    run_git_command(
        ["git", "clone", "--depth=1", "--", git_url, str(framework_path)],
        network=True,
        retry_cleanup=framework_path,
    )
    if ref:
        run_git_command(
            ["git", "fetch", "--depth=1", "--", "origin", ref],
            git_dir=framework_path,
            network=True,
        )
        run_git_command(
            ["git", "reset", "--hard", "FETCH_HEAD"],
            git_dir=framework_path,
        )
    update_submodules(framework_path, key)

    # Sanity-check the resulting tree: a clone can exit 0 yet produce no
    # usable ESP-IDF checkout, which would otherwise be marked extracted and
    # stuck until ``esphome clean``.
    if not (framework_path / "tools" / "idf_tools.py").is_file():
        raise RuntimeError(
            f"Clone of {key} produced no usable ESP-IDF tree at {framework_path}"
        )


def _write_idf_version_txt(framework_path: Path, version: str) -> None:
    """Write <framework_path>/version.txt if missing.

    IDF's build.cmake picks the version it embeds in the firmware (and
    stamps onto the bootloader) in this order: ``${IDF_PATH}/version.txt``
    if present, else ``git describe`` against IDF_PATH, else the
    ``IDF_VERSION_MAJOR/MINOR/PATCH`` triplet from ``tools/cmake/version.cmake``.
    On a clean esphome-libs tarball ``.git`` is fully stripped, so
    git_describe returns ``HEAD-HASH-NOTFOUND`` (falsy) and the triplet
    wins -- correct by luck. But a *partial* ``.git`` (e.g. a custom
    framework.source pointed at a real git URL where build artifacts
    mark the tree dirty) makes git_describe return ``<hash>-dirty``,
    which is what then gets baked into the bootloader. Dropping
    version.txt forces the right answer regardless.
    """
    version_txt = framework_path / "version.txt"
    if version_txt.exists():
        return
    try:
        version_txt.write_text(f"v{version}\n", encoding="utf-8")
    except OSError as e:
        _LOGGER.warning(
            "Could not write %s (%s); bootloader version string may be incorrect.",
            version_txt,
            e,
        )


# Backport of espressif/esp-idf#18272: every ESPHome-supported IDF release
# through v6.0 ships a tools.json whose ninja 1.12.1 entry has no
# ``linux-arm64`` source. ``idf_tools.py`` then either fails to find a
# matching binary or grabs the x86_64 one, which can't execute on
# aarch64. cmake is already populated across the same release range; we
# only need to inject ninja. Values lifted verbatim from the IDF v6.0.1
# tools.json where the fix landed natively.
_NINJA_ARM64_BACKPORT: dict[str, dict[str, str | int]] = {
    "1.12.1": {
        "rename_dist": "ninja-linux-arm64-v1.12.1.zip",
        "sha256": "5c25c6570b0155e95fce5918cb95f1ad9870df5768653afe128db822301a05a1",
        "size": 121787,
        "url": "https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux-aarch64.zip",
    },
}


def _patch_tools_json(
    framework_path: Path,
    apply_patch: Callable[[dict], bool],
    patched_log: str,
) -> None:
    """Apply an in-place fixup to the framework's tools/tools.json.

    Shared plumbing for the tools.json patches below: a missing file is a
    no-op, an unparseable file logs a warning and skips, and when
    ``apply_patch`` reports a change the file is written back atomically.
    ``patched_log`` is the info log line, with a single ``%s`` placeholder
    for the tools.json path. Patches are idempotent and applied on every
    install check, so an already-extracted framework picks them up on the
    next build without forcing a clean.
    """
    tools_json = framework_path / "tools" / "tools.json"
    if not tools_json.is_file():
        return

    try:
        with tools_json.open(encoding="utf-8") as f:
            data = json.load(f)
        # apply_patch also raises inside the guard: a tools.json that is
        # valid JSON but not the expected shape (e.g. a top-level list)
        # must skip the patch, not crash the install check this patch is
        # meant to recover.
        changed = apply_patch(data)
    except (json.JSONDecodeError, OSError, AttributeError, TypeError, KeyError) as e:
        _LOGGER.warning(
            "Could not apply tools.json patch to %s (%s); skipping. A clean "
            "reinstall of the framework directory may be needed.",
            tools_json,
            e,
        )
        return

    if changed:
        # write_file_if_changed stages a tempfile in the destination dir
        # and atomically replaces — safe against mid-write interruption
        # and concurrent invocations.
        write_file_if_changed(tools_json, json.dumps(data, indent=2) + "\n")
        _LOGGER.info(patched_log, tools_json)


def _patch_tools_json_for_linux_arm64(framework_path: Path) -> None:
    """Inject ninja linux-arm64 entries into the framework's tools.json on aarch64.

    A tools.json that already has the entry, or a host that isn't aarch64,
    is a no-op.
    """
    if platform.machine() != "aarch64":
        return

    def apply_patch(data: dict) -> bool:
        changed = False
        for tool in data.get("tools", []):
            if tool.get("name") != "ninja":
                continue
            for ver in tool.get("versions", []):
                entry = _NINJA_ARM64_BACKPORT.get(ver.get("name"))
                if entry is None or ver.get("linux-arm64"):
                    continue
                ver["linux-arm64"] = entry
                changed = True
        return changed

    _patch_tools_json(
        framework_path,
        apply_patch,
        "Patched %s to add ninja linux-arm64 download "
        "(espressif/esp-idf#18272 backport).",
    )


# Tools marked ``install: always`` in tools.json that no ESPHome build ever
# runs. openocd-esp32 is a JTAG debug server (its post-install check also
# fails outright on systems without libusb-1.0, #17685). The gdb bundles are
# debuggers used only by ``idf.py gdb``/``idf.py monitor`` flows ESPHome never
# invokes; stack decoding uses addr2line from the compiler toolchains instead.
# esp32ulp-elf is the ULP coprocessor toolchain, and ESPHome excludes the IDF
# ``ulp`` component from every build. esp-rom-elfs stays required: the cmake
# gdbinit generation reads ESP_ROM_ELF_DIR during every configure and warns
# when it is missing.
_UNUSED_IDF_TOOLS: tuple[str, ...] = (
    "esp32ulp-elf",
    "openocd-esp32",
    "riscv32-esp-elf-gdb",
    "xtensa-esp-elf-gdb",
)

# tools.json also lists riscv32-esp-elf as supported on the xtensa chips
# because the S2/S3 ULP coprocessor is a RISC-V core, so installing for an
# S2/S3 target pulls in the whole riscv compiler (~290MB download, 2GB disk)
# just for ULP programs — which ESPHome never builds (the IDF ``ulp``
# component is excluded by default; a user who re-enables it via
# ``include_builtin_idf_components: [ulp]`` on an S2/S3 and hits a missing
# riscv compiler can set ESPHOME_IDF_DEFAULT_TARGETS=all to install it).
# Removing the xtensa chips from its supported targets keeps it out of
# xtensa-only installs; building a RISC-V variant still installs it. Add any
# future Xtensa chip here; a missing entry only costs the download, while a
# wrongly listed RISC-V chip would strip its own compiler.
_XTENSA_TARGETS: tuple[str, ...] = ("esp32", "esp32s2", "esp32s3")


def _patch_tools_json_demote_unused_tools(framework_path: Path) -> None:
    """Demote tools ESPHome never runs from ``install: always`` to ``on_request``.

    ``idf_tools.py install required`` downloads every tool marked ``always``
    in tools.json and validates each one after extraction by running its
    version command. Demoting the tools in ``_UNUSED_IDF_TOOLS`` drops them
    from the ``required`` set: they are no longer downloaded or validated,
    and the tool-path export treats a missing ``on_request`` tool as fine.
    Besides the download and disk savings, this makes the openocd libusb
    validation failure (#17685) impossible; because this runs on every
    install check, an install stuck in that failing state (which never wrote
    its stamp file) heals on the next build without a clean. A user who
    wants one of these tools can still name it explicitly in
    ESPHOME_IDF_DEFAULT_TOOLS; explicit names bypass install-type filtering.

    Also removes the xtensa chips from riscv32-esp-elf's supported targets
    (see ``_XTENSA_TARGETS``) so xtensa-only installs don't pull in the
    RISC-V compiler for ULP programs ESPHome never builds.
    """

    def apply_patch(data: dict) -> bool:
        changed = False
        for tool in data.get("tools", []):
            if (
                tool.get("name") in _UNUSED_IDF_TOOLS
                and tool.get("install") == "always"
            ):
                tool["install"] = "on_request"
                changed = True
            if tool.get("name") == "riscv32-esp-elf":
                targets = tool.get("supported_targets")
                # Guard the type so unexpected JSON here cannot abort the
                # other demotions; this patch is best-effort. Log it so a
                # silently resumed riscv download is diagnosable.
                if not isinstance(targets, list):
                    _LOGGER.warning(
                        "Unexpected supported_targets for riscv32-esp-elf "
                        "in tools.json (%s); not excluding it from xtensa "
                        "installs",
                        type(targets).__name__,
                    )
                    continue
                if any(t in targets for t in _XTENSA_TARGETS):
                    tool["supported_targets"] = [
                        t for t in targets if t not in _XTENSA_TARGETS
                    ]
                    changed = True
        return changed

    _patch_tools_json(
        framework_path,
        apply_patch,
        "Patched %s to skip installing tools ESPHome does not use "
        "(openocd, gdb, ULP toolchains).",
    )


def _prefetch_idf_tool_archives(
    framework_path: Path,
    targets_str: str,
    tools: list[str],
    env: dict[str, str] | None,
) -> None:
    """Pre-download the tool archives ``idf_tools.py install`` would fetch.

    ``idf_tools.py``'s own downloader restarts from byte zero on every retry,
    which makes large archives effectively impossible to fetch on unstable
    connections (#17703). This asks the framework's idf_tools (via
    ``get_tool_downloads.py``) which archives the coming install needs, then
    downloads them into ``<IDF_TOOLS_PATH>/dist`` with
    ``download_with_resume``, a few at a time under one combined progress
    bar. The installer then finds the verified archives already in place
    ("file ... is already downloaded") and never touches the network.

    Strictly best-effort: any failure here just logs and returns, leaving
    ``idf_tools.py install`` to download whatever is missing exactly as
    before. Leftover ``.part`` files live in ``dist/`` and are removed by the
    post-install cache prune.
    """
    try:
        success, stdout, stderr = _run_idf_tools_script(
            framework_path,
            "get_tool_downloads.py",
            "ESP-IDF tool download list",
            args=[targets_str, *tools],
            env=env,
        )
        if not success or not stdout:
            _LOGGER.warning(
                "Could not determine ESP-IDF tool downloads: %s",
                (stderr or "").strip(),
            )
            return
        dist_path = get_idf_tools_path() / "dist"
        entries = []
        seen_dests: set[str] = set()
        for entry in json.loads(stdout):
            if (dist_path / entry["dest"]).is_file():
                continue
            # Never download unverified: an entry without sha256/size is
            # left to the installer, which fails loudly on a bad archive.
            # Checked before the dedupe so it cannot shadow a verifiable
            # duplicate of the same dest.
            if not (entry.get("sha256") and entry.get("size")):
                _LOGGER.warning(
                    "Tool %s has no sha256/size in the download list; "
                    "leaving it to the installer",
                    entry["name"],
                )
                continue
            if entry["dest"] in seen_dests:
                # Two workers on one .part file would interleave
                # seek/truncate writes; mirror the library prefetch's dedupe
                continue
            seen_dests.add(entry["dest"])
            entries.append(entry)
        if not entries:
            return
        _LOGGER.info(
            "Downloading %d ESP-IDF tool archive(s): %s",
            len(entries),
            ", ".join(entry["name"] for entry in entries),
        )

        # No sequential fallback here: skipping the prefetch would lose the
        # resume workaround for #17703, and every entry has a size (above).
        # A failed archive is retried by the installer itself (without
        # resume); keep prefetching the rest.
        failures = run_batch_downloads(
            "Downloading ESP-IDF tools",
            [
                (
                    entry["name"],
                    entry["size"],
                    resume_fetch_job(
                        entry["url"],
                        dist_path / entry["dest"],
                        sha256=entry["sha256"],
                        size=entry["size"],
                    ),
                )
                for entry in entries
            ],
        )
        warn_prefetch_failures(failures)
        if len(failures) == len(entries):
            # A systematic fault, not one flaky mirror: the resume
            # workaround (#17703) is off for this whole install
            _LOGGER.error(
                "Every ESP-IDF tool prefetch failed; the installer will "
                "download without resume"
            )
    except Exception as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # The installer downloads anything missing itself; never let the
        # prefetch become a new way for the install to fail.
        _LOGGER.warning("ESP-IDF tool prefetch failed: %s", failure_reason(e))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)


def _check_esphome_idf_framework_install(
    version: str,
    targets: list[str],
    tools: list[str],
    force: bool = False,
    env: dict[str, str] | None = None,
    source_url: str | None = None,
) -> tuple[Path, bool]:
    """
    Check and install ESP-IDF framework.

    Args:
        version: ESP-IDF version to check/install
        targets: Target platforms to install
        tools: list of tools to install
        force: If True, force reinstallation
        env: Optional dictionary of environment variables to set
        source_url: Optional override URL for the framework tarball. Supports
            the same ``{VERSION}`` / ``{MAJOR}`` / ``{MINOR}`` / ``{PATCH}`` /
            ``{EXTRA}`` / ``{SHORT_VERSION}`` substitutions as
            ESPHOME_IDF_FRAMEWORK_MIRRORS (``{EXTRA}`` includes its leading
            ``-``, e.g. ``-rc1``, or is empty; ``{SHORT_VERSION}`` is ``x.y``
            plus any extra and only available for x.y.0 versions — a URL
            referencing it is skipped for other versions). When set, it
            replaces the default mirror list — no implicit fallback, so a
            misspelled or skipped URL fails loudly with an EsphomeError naming
            the URL.

    Returns:
        tuple of (framework_path, fresh_extract_flag). The flag is True only
        when the framework tree was downloaded and extracted this run, not
        when tools were installed into an existing tree.
    """

    # Sanitize inputs
    targets = sorted(set(targets))
    tools = sorted(set(tools))

    stamp_info = {}
    stamp_info["schema_version"] = STAMP_SCHEMA_VERSION
    stamp_info["targets"] = targets
    stamp_info["tools"] = tools
    # TODO: Add stamp with this module version

    # 1. Get framework path and stamp file path
    framework_path = _get_framework_path(version)
    extracted_marker = framework_path / ".esphome_extracted"
    env_stamp_file = framework_path / ESPHOME_STAMP_FILE
    idf_tools_path = framework_path / "tools" / "idf_tools.py"
    _LOGGER.info("Checking ESP-IDF %s framework ...", version)
    # Logged every invocation (not just on install) so the user can verify the
    # override. A changed URL needs ``esphome clean-all`` to force a re-download
    # (``esphome clean`` only wipes the build dir, not the extracted framework
    # under the global install dir's ``frameworks/<version>``).
    if source_url:
        _LOGGER.info("Using framework source override: %s", source_url)

    # 2. Download and extract the framework if not already extracted.
    # The marker is written last after extraction succeeds, so its presence
    # is the authoritative "extraction complete" signal — no half-extracted
    # tree can pass for installed. Extracting directly into framework_path
    # avoids post-extraction renames that race with antivirus on Windows.
    # Tool install state is tracked separately by the stamp file in step 3,
    # so we only re-extract when extraction itself is missing or incomplete.
    fresh_extract = force or not extracted_marker.is_file()
    if fresh_extract:
        rmdir(framework_path, msg=f"Clean up ESP-IDF {version} framework")

        git_source = _parse_git_source(source_url) if source_url else None
        if git_source is not None:
            git_url, ref = git_source
            _clone_idf_with_submodules(framework_path, git_url, ref)
        else:
            _LOGGER.info("Downloading ESP-IDF %s framework ...", version)

            # Create substitutions for the URLs. SHORT_VERSION (x.y with
            # optional -extra) is only provided for x.y.0 releases, since
            # the vX.Y release tags only exist for those; templates that
            # reference it are skipped for other versions by
            # download_from_mirrors.
            substitutions = {"VERSION": version}
            try:
                ver = Version.parse(version)
                substitutions["MAJOR"] = str(ver.major)
                substitutions["MINOR"] = str(ver.minor)
                substitutions["PATCH"] = str(ver.patch)
                substitutions["EXTRA"] = f"-{ver.extra}" if ver.extra else ""
                if ver.patch == 0:
                    substitutions["SHORT_VERSION"] = (
                        f"{ver.major}.{ver.minor}{substitutions['EXTRA']}"
                    )
            except ValueError:
                _LOGGER.warning(
                    "ESP-IDF version '%s' is not a valid version number; "
                    "only the {VERSION} substitution is available for "
                    "mirror URLs",
                    version,
                )

            mirrors = [source_url] if source_url else ESPHOME_IDF_FRAMEWORK_MIRRORS
            # Download to a persistent file in the tool download cache (not
            # a temp file) so an interrupted download resumes on the next
            # run; the cache is pruned after a successful install anyway.
            tarball_path = get_idf_tools_path() / "dist" / f"esp-idf-{version}.tar.xz"
            download_and_extract(
                mirrors,
                substitutions,
                tarball_path,
                framework_path,
                progress_header="Extracting",
            )
        extracted_marker.touch()

    # Idempotent post-extract patch: written every invocation so a build
    # dir extracted before this fix gets the file too, without forcing a
    # clean. Skips when version.txt already exists.
    _write_idf_version_txt(framework_path, version)

    # Apply the ninja linux-arm64 backport on every invocation, not just on
    # fresh extracts — idempotent and cheap, and lets a build dir carrying
    # a pre-patch tools.json get fixed up without forcing a clean.
    _patch_tools_json_for_linux_arm64(framework_path)

    # Drop tools ESPHome never runs from the required tool set on every
    # invocation, so an install that previously failed on the openocd libusb
    # check recovers on the next build.
    _patch_tools_json_demote_unused_tools(framework_path)

    # 3. Check if the framework tools are the same and correctly installed
    stored_stamp = None if fresh_extract else _read_stamp(env_stamp_file)
    install = fresh_extract
    if not install:
        install = True
        if _stamp_covers(stored_stamp, stamp_info):
            _LOGGER.info("Checking ESP-IDF %s framework installation ...", version)
            # Validate via the managed tool-path resolution, not ``idf_tools.py check``:
            # ``check`` probes tools on the system PATH and aborts if any fail to run (e.g. a
            # broken Homebrew openocd), which forced a toolchain reinstall on every build.
            try:
                _get_idf_tool_paths(framework_path, env)
                install = False
            except RuntimeError as err:
                _LOGGER.debug(
                    "ESP-IDF %s tool resolution failed, reinstalling: %s", version, err
                )

    # 4. Install framework tools if not installed or needs update
    if install:
        _LOGGER.info("Installing ESP-IDF %s framework ...", version)
        targets_str = ",".join(targets)
        _prefetch_idf_tool_archives(framework_path, targets_str, tools, env)
        cmd = [
            get_system_python_path(),
            str(idf_tools_path),
            "--non-interactive",
            "install",
            f"--targets={targets_str}",
        ] + tools
        if not run_command_ok(
            cmd,
            msg=f"ESP-IDF {version} framework installation",
            env=env,
            stream_output=True,
        ):
            if platform.system() == "Linux" and find_library("usb-1.0") is None:
                _LOGGER.error(
                    "libusb-1.0.so.0 was not found on this system. If the error "
                    "above mentions it (openocd fails its install check without "
                    "it), install the libusb 1.0 package, e.g. libusb-1.0-0 "
                    "(Debian/Ubuntu), libusb1 (Fedora) or libusb (Alpine/Arch), "
                    "then run the build again."
                )
            raise RuntimeError(f"ESP-IDF {version} framework installation failure")

        # idf_tools.py extracts tool archives from <IDF_TOOLS_PATH>/dist into tools/; the
        # archives are not needed afterward and, already compressed, dominate the cached install.
        # Best-effort: a failure to prune must not fail an otherwise successful install.
        try:
            rmdir(
                get_idf_tools_path() / "dist", msg="Remove ESP-IDF tool download cache"
            )
        except RuntimeError as err:
            _LOGGER.debug("Could not remove ESP-IDF tool download cache: %s", err)

        # Record the union of every target installed so far, not just this
        # build's. idf_tools.py accumulates targets in idf-env.json and the
        # ``required`` metapackage installs tools for all of them, so the
        # union is what is actually on disk — and it keeps two variants
        # alternating between builds from re-running the installer each time.
        # Merge only when everything except targets matches: a reinstall
        # triggered by a schema or tools change ran the installer for this
        # build's targets alone, so carrying the old targets forward would
        # let later builds of those variants skip the reinstall they need.
        if (
            stored_stamp
            and isinstance(stored_stamp.get("targets"), list)
            and _stamps_match_except_targets(stored_stamp, stamp_info)
        ):
            merged = set(stamp_info["targets"]) | set(stored_stamp["targets"])
            stamp_info["targets"] = ["all"] if "all" in merged else sorted(merged)
        _write_stamp(env_stamp_file, stamp_info)

    return framework_path, fresh_extract


def _check_esp_idf_python_env_install(
    version: str,
    features: list[str],
    force: bool = False,
    env: dict[str, str] | None = None,
) -> tuple[Path, bool]:
    """
    Check and install ESP-IDF Python environment.

    Args:
        version: ESP-IDF version to check/install
        features: Features to install
        force: If True, force reinstallation
        env: Environment variables to use

    Returns:
        tuple of (python_env_path, install_flag)
    """

    # Sanitize inputs
    features = sorted(set(features))

    stamp_info = {}
    stamp_info["schema_version"] = STAMP_SCHEMA_VERSION
    stamp_info["features"] = features

    framework_path = _get_framework_path(version)
    python_env_path = _get_python_env_path(version)
    env_stamp_file = python_env_path / ESPHOME_STAMP_FILE
    env_python_path = get_python_env_executable_path(python_env_path, "python")

    _LOGGER.info("Checking ESP-IDF %s Python environment ...", version)
    install = force or not python_env_path.is_dir() or not env_python_path.is_file()
    if not install:
        # Check it against the stamp file
        install = True
        python_version = _get_python_version(env_python_path, env=env)
        if python_version:
            stamp_info["python_version"] = python_version
            if _check_stamp(env_stamp_file, stamp_info):
                install = False

    if install:
        rmdir(python_env_path, msg=f"Clean up ESP-IDF {version} Python environment")

        create_venv(python_env_path, msg=f"ESP-IDF {version}")

        esp_idf_version = _get_idf_version(framework_path, env=env)
        constraint_file_path = (
            get_idf_tools_path() / f"espidf.constraints.v{esp_idf_version}.txt"
        )
        _LOGGER.debug("ESP-IDF version %s", esp_idf_version)

        _LOGGER.info("Downloading constraints file for ESP-IDF %s ...", esp_idf_version)
        download_from_mirrors(
            ESP_IDF_CONSTRAINTS_MIRRORS,
            {"VERSION": esp_idf_version},
            constraint_file_path,
        )

        cmd_pip_install = [
            str(env_python_path),
            "-m",
            "pip",
            "install",
            "--upgrade",
            "--constraint",
            constraint_file_path,
        ]

        _LOGGER.info("Installing ESP-IDF %s Python dependencies ...", version)
        cmd = cmd_pip_install + [
            "pip",
            "setuptools",
        ]
        if not run_command_ok(
            cmd,
            msg=f"Upgrade ESP-IDF {version} Python environment packages",
            env=env,
        ):
            raise RuntimeError(
                f"Upgrade ESP-IDF {version} Python environment packages failure"
            )

        for feature in features:
            requirements_file = (
                framework_path
                / "tools"
                / "requirements"
                / f"requirements.{feature}.txt"
            )
            cmd = cmd_pip_install + [
                "-r",
                str(requirements_file),
            ]
            if not run_command_ok(
                cmd,
                msg=f"Install ESP-IDF {version} Python dependencies for {feature}",
                env=env,
            ):
                raise RuntimeError(
                    f"Install ESP-IDF {version} Python dependencies for {feature} failure"
                )

        stamp_info["python_version"] = _get_python_version(
            env_python_path, env=env, throw_exception=True
        )
        _write_stamp(env_stamp_file, stamp_info)

    return python_env_path, install


def check_esp_idf_install(
    version: str,
    targets: list[str] | None = None,
    tools: list[str] | None = None,
    features: list[str] | None = None,
    force: bool = False,
    source_url: str | None = None,
) -> tuple[Path, Path]:
    """
    Check and install ESP-IDF framework and Python environment.

    Args:
        version: ESP-IDF version to check/install
        targets: Target platforms to install
        tools: list of tools to install
        features: Features to install
        force: If True, force reinstallation
        source_url: Optional override URL for the framework tarball. When
            set, it replaces the default mirror list (no fallback). Forwarded
            to ``_check_esphome_idf_framework_install``; supports the same URL
            substitutions.

    Returns:
        tuple of (framework_path, python_env_path)
    """
    _check_windows_path_length()

    env = {}
    env["IDF_TOOLS_PATH"] = str(get_idf_tools_path())
    env["IDF_PATH"] = ""

    # An explicit ESPHOME_IDF_DEFAULT_TARGETS wins over the caller's
    # per-variant request (builder-image pre-warm); otherwise the caller's
    # targets are used, falling back to the default when none were given.
    if _IDF_DEFAULT_TARGETS_EXPLICIT or not targets:
        targets = ESPHOME_IDF_DEFAULT_TARGETS

    # Determine which tools need to be installed if not provided
    if tools is None:
        tools = []
        for tool in set(ESPHOME_IDF_DEFAULT_TOOLS) | set(
            ESPHOME_IDF_DEFAULT_TOOLS_FORCE
        ):
            # Check if the tool exist
            if tool in ESPHOME_IDF_DEFAULT_TOOLS_FORCE or not shutil.which(tool):
                tools.append(tool)

    # 1) Framework
    framework_path, fresh_extract = _check_esphome_idf_framework_install(
        version, targets, tools, force=force, env=env, source_url=source_url
    )

    features = features or ESPHOME_IDF_DEFAULT_FEATURES

    # 2) Python env. Only a freshly extracted framework forces a rebuild —
    # the venv depends on the framework version and features, not on which
    # toolchains are installed, so adding a target to an existing tree must
    # not wipe it. It still self-validates against its own stamp.
    python_env_path, _ = _check_esp_idf_python_env_install(
        version, features, force=force or fresh_extract, env=env
    )

    return framework_path, python_env_path


def _ccache_env() -> dict[str, str]:
    """Return ccache settings for ESP-IDF compiles.

    Enabled by default whenever a runnable ``ccache`` binary is on PATH.
    ``IDF_CCACHE_ENABLE=0`` opts out and ``=1`` forces it on; when that knob
    is unset the shared ``ESPHOME_CCACHE_ENABLE`` applies (same 0/1 forms,
    unrecognized values warn and count as unset). The cache lives under
    the IDF tools path (the machine-global cache dir, or
    ``ESPHOME_ESP_IDF_PREFIX``), so it is shared across all projects and removed
    by ``esphome clean-all`` along with the framework.

    Depend mode keeps cache-miss overhead low (hashes the compiler's depfiles
    instead of preprocessing). ``CCACHE_BASEDIR`` rewrites the per-build
    absolute paths (generated ``sdkconfig`` include, etc.) so different devices
    share framework cache entries; it is scoped to the build dir on purpose --
    a broader base would also rewrite the shared IDF path under the cache dir
    and lose those hits.

    Only values the user has not already set in the environment are returned, so
    a custom ``CCACHE_DIR`` / ``CCACHE_MAXSIZE`` / etc. is respected.
    """
    # IDF_CCACHE_ENABLE (the backend-native knob) wins over the shared
    # ESPHOME_CCACHE_ENABLE.
    idf_knob = parse_enable_env("IDF_CCACHE_ENABLE")
    if idf_knob is False:
        # The raw value (e.g. "disable") is still inherited by idf.py via
        # os.environ, where a non-false-constant string reads as truthy;
        # export the canonical off spelling instead
        return {"IDF_CCACHE_ENABLE": "0"}
    if idf_knob is True:
        # Forced on ignores the runnability verdict, but the outcome is
        # worth saying out loud. Probed directly (not via the resolver,
        # whose failure message says "compiling without ccache" -- exactly
        # what forced-on does NOT do): only the truly-missing case means
        # idf.py compiles without ccache; a broken binary is still used,
        # since idf.py does its own PATH lookup.
        if (ccache := shutil.which("ccache")) is None:
            _LOGGER.warning(
                "IDF_CCACHE_ENABLE=1 but no ccache binary is on PATH; "
                "idf.py will compile without ccache"
            )
        else:
            # The probe warns with this message iff the binary fails
            tool_version_runs(
                ccache,
                "IDF_CCACHE_ENABLE=1 forces on the ccache at %s even though "
                "it failed to run; idf.py will use it anyway",
            )
    elif resolve_ccache_path() is None:
        # ESP-IDF silently skips ccache without the binary; export the
        # canonical off spelling so an unparsable inherited value (or a
        # probe-rejected ccache idf.py would still find) cannot enable it
        return {"IDF_CCACHE_ENABLE": "0"}

    env = ccache_defaults_env(get_idf_tools_path() / "ccache")
    # Exactly one canonical spelling ever reaches idf.py, whatever the
    # accepted input spelling was ("enable", "yes", ...)
    env["IDF_CCACHE_ENABLE"] = "1"
    return env


def get_framework_env(
    framework_path: PathType,
    python_env_path: PathType | None = None,
    env: dict[str, str] | None = None,
):
    """
    Get environment variables for ESP-IDF framework.

    Args:
        framework_path: Path to the ESP-IDF framework
        python_env_path: Optional path to Python environment
        env: Optional dictionary of environment variables to set

    Returns:
        Dictionary containing updated environment variables
    """
    # 1. Initialize base environment with extra ESP-IDF environment variables
    env = env.copy() if env else {}
    env["IDF_TOOLS_PATH"] = str(get_idf_tools_path())
    env["IDF_PATH"] = ""

    # 2. Get existing PATH from env or os.environ
    if "PATH" in env:
        path_list = env["PATH"].split(os.pathsep)
    else:
        path_list = os.environ["PATH"].split(os.pathsep)

    # 3. If Python environment path is provided, add it to PATH and set IDF_PYTHON_ENV_PATH
    if python_env_path:
        python_path = get_python_env_executable_path(python_env_path, "python")
        path_list.insert(0, str(python_path.parent))
        env["IDF_PYTHON_ENV_PATH"] = str(python_env_path)

    # 4. Set framework-specific environment variables
    env["IDF_PATH"] = str(framework_path)
    env["ESP_IDF_VERSION"] = _get_idf_version(framework_path, env)

    # 5. Get and add tool paths and environment variables
    paths_to_export, export_vars = _get_idf_tool_paths(framework_path, env)
    env.update(export_vars)
    env["PATH"] = os.pathsep.join(paths_to_export + path_list)

    # 6. Enable ccache for the compile toolchain (default on when available).
    env.update(_ccache_env())

    return env
