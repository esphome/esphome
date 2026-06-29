"""ESP-IDF framework tools for ESPHome."""

import json
import logging
import os
from pathlib import Path
import platform
import re
import shutil
import tempfile

from esphome.config_validation import Version
from esphome.core import CORE
from esphome.framework_helpers import (
    PathType,
    archive_extract_all,
    create_venv,
    download_from_mirrors,
    get_python_env_executable_path,
    get_system_python_path,
    rmdir,
    run_command,
    run_command_ok,
    str_to_lst_of_str,
)
from esphome.helpers import get_bool_env, get_str_env, write_file_if_changed

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
        "https://github.com/esphome-libs/esp-idf/releases/download/v{MAJOR}.{MINOR}{EXTRA}/esp-idf-v{MAJOR}.{MINOR}{EXTRA}.tar.xz",
    ]
)

ESP_IDF_CONSTRAINTS_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESP_IDF_CONSTRAINTS_MIRRORS",
        "https://dl.espressif.com/dl/esp-idf/espidf.constraints.v{VERSION}.txt",
    )
)


def _get_idf_tools_path() -> Path:
    """
    Get the path to the ESP-IDF tools directory.

    Returns:
        Path object pointing to the ESP-IDF tools directory
    """
    if "ESPHOME_ESP_IDF_PREFIX" in os.environ:
        path = Path(get_str_env("ESPHOME_ESP_IDF_PREFIX", None)).expanduser()
    else:
        path = CORE.data_dir / "idf"
    # Resolve so an unnormalized config path (e.g. compiling ``../config/x.yaml``)
    # doesn't leave ``..`` segments in the IDF_TOOLS_PATH handed to idf.py, which
    # otherwise warns that the venv interpreter path doesn't match the install.
    return path.resolve()


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
    tools_path = str(_get_idf_tools_path())
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
        "  - Enable Windows long path support: set\n"
        "    HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled\n"
        "    to 1 and reboot, or\n"
        "  - Move your ESPHome project to a shorter path\n"
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
    return _get_idf_tools_path() / "frameworks" / f"{version}"


def _get_python_env_path(version: str) -> Path:
    """
    Get the path to the ESPHome ESP-IDF Python environment directory for a specific version.

    Args:
        version: ESP-IDF version string

    Returns:
        Path object pointing to the Python environment directory
    """
    return _get_idf_tools_path() / "penvs" / f"{version}"


def _check_stamp(file: PathType, data: dict[str, str]) -> bool:
    """
    Check if a stamp file contains the expected data.

    Args:
        file: Path to the stamp file
        data: Dictionary containing expected data

    Returns:
        True if file exists and contains expected data, False otherwise
    """
    if not Path(file).is_file():
        return False

    try:
        with Path(file).open(encoding="utf-8") as f:
            return json.load(f) == data
    except (json.JSONDecodeError, OSError):
        return False


def _write_stamp(file: PathType, data: dict[str, str]):
    """
    Write data to a stamp file in JSON format.

    Args:
        file: Path to the stamp file to write
        data: Dictionary containing data to write
    """
    with Path(file).open("w", encoding="utf8") as fp:
        json.dump(data, fp)


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

    cmd = [
        get_system_python_path(),
        str(_SCRIPTS_DIR / "get_idf_version.py"),
        str(idf_framework_root),
    ]

    success, stdout, stderr = run_command(
        cmd,
        msg="ESP-IDF version",
        env=(env or os.environ)
        | {"PYTHONPATH": str(Path(idf_framework_root) / "tools")},
    )
    if stdout:
        stdout = stdout.strip()
    if not success or not stdout:
        detail = (stderr or "").strip()
        raise RuntimeError(
            f"Can't get ESP-IDF version of {idf_framework_root}"
            + (f": {detail}" if detail else "")
        )
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

    cmd = [
        get_system_python_path(),
        str(_SCRIPTS_DIR / "get_idf_tool_paths.py"),
        str(idf_framework_root),
    ]

    success, stdout, stderr = run_command(
        cmd,
        msg="ESP-IDF tool paths",
        env=(env or os.environ)
        | {"PYTHONPATH": str(Path(idf_framework_root) / "tools")},
    )
    if not success or not stdout:
        detail = (stderr or "").strip()
        raise RuntimeError(
            f"Can't get ESP-IDF tool paths of {idf_framework_root}"
            + (f": {detail}" if detail else "")
        )

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
    from esphome.git import run_git_command

    _LOGGER.info("Cloning ESP-IDF from %s%s", git_url, f"@{ref}" if ref else "")
    run_git_command(["git", "clone", "--depth=1", "--", git_url, str(framework_path)])
    if ref:
        run_git_command(
            ["git", "fetch", "--depth=1", "--", "origin", ref],
            git_dir=framework_path,
        )
        run_git_command(
            ["git", "reset", "--hard", "FETCH_HEAD"],
            git_dir=framework_path,
        )
    run_git_command(
        [
            "git",
            "submodule",
            "update",
            "--init",
            "--recursive",
            "--depth=1",
        ],
        git_dir=framework_path,
    )

    # Sanity-check the resulting tree. run_git_command only raises when
    # stderr is non-empty, so a clone that silently produces no working
    # tree would otherwise be marked extracted and stuck until
    # ``esphome clean``.
    if not (framework_path / "tools" / "idf_tools.py").is_file():
        raise RuntimeError(
            f"Clone of {git_url} produced no usable ESP-IDF tree at {framework_path}"
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


def _patch_tools_json_for_linux_arm64(framework_path: Path) -> None:
    """Inject ninja linux-arm64 entries into the framework's tools.json on aarch64.

    Idempotent: a tools.json that already has the entry, or a host that
    isn't aarch64, is a no-op. Applied unconditionally on every install
    check so a build dir extracted before the backport got fixed up
    without forcing a clean.
    """
    if platform.machine() != "aarch64":
        return

    tools_json = framework_path / "tools" / "tools.json"
    if not tools_json.is_file():
        return

    try:
        with tools_json.open(encoding="utf-8") as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        _LOGGER.warning(
            "Could not parse %s for linux-arm64 backport (%s); "
            "skipping. A clean reinstall of the framework directory "
            "may be needed.",
            tools_json,
            e,
        )
        return

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

    if changed:
        # write_file_if_changed stages a tempfile in the destination dir
        # and atomically replaces — safe against mid-write interruption
        # and concurrent invocations.
        write_file_if_changed(tools_json, json.dumps(data, indent=2) + "\n")
        _LOGGER.info(
            "Patched %s to add ninja linux-arm64 download "
            "(espressif/esp-idf#18272 backport).",
            tools_json,
        )


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
            ``{EXTRA}`` substitutions as ESPHOME_IDF_FRAMEWORK_MIRRORS
            (``{EXTRA}`` includes its leading ``-``, e.g. ``-rc1``, or is empty).
            When set, it replaces the default mirror list — no implicit fallback,
            so a misspelled URL fails loudly.

    Returns:
        tuple of (framework_path, install_flag)
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
    # under <data_dir>/idf/frameworks/<version>).
    if source_url:
        _LOGGER.info("Using framework source override: %s", source_url)

    # 2. Download and extract the framework if not already extracted.
    # The marker is written last after extraction succeeds, so its presence
    # is the authoritative "extraction complete" signal — no half-extracted
    # tree can pass for installed. Extracting directly into framework_path
    # avoids post-extraction renames that race with antivirus on Windows.
    # Tool install state is tracked separately by the stamp file in step 3,
    # so we only re-extract when extraction itself is missing or incomplete.
    install = force or not extracted_marker.is_file()
    if install:
        rmdir(framework_path, msg=f"Clean up ESP-IDF {version} framework")

        git_source = _parse_git_source(source_url) if source_url else None
        if git_source is not None:
            git_url, ref = git_source
            _clone_idf_with_submodules(framework_path, git_url, ref)
        else:
            # Download in temporary file
            with tempfile.NamedTemporaryFile() as tmp:
                _LOGGER.info("Downloading ESP-IDF %s framework ...", version)

                # Create substitutions for the URLs
                substitutions = {"VERSION": version}
                try:
                    ver = Version.parse(version)
                    substitutions["MAJOR"] = str(ver.major)
                    substitutions["MINOR"] = str(ver.minor)
                    substitutions["PATCH"] = str(ver.patch)
                    substitutions["EXTRA"] = f"-{ver.extra}" if ver.extra else ""
                except ValueError:
                    pass

                mirrors = [source_url] if source_url else ESPHOME_IDF_FRAMEWORK_MIRRORS
                download_from_mirrors(mirrors, substitutions, tmp.file)

                _LOGGER.info("Extracting ESP-IDF %s framework ...", version)
                archive_extract_all(
                    tmp.file, framework_path, progress_header="Extracting"
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

    # 3. Check if the framework tools are the same and correctly installed
    if not install:
        install = True
        if _check_stamp(env_stamp_file, stamp_info):
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
            raise RuntimeError(f"ESP-IDF {version} framework installation failure")

        _write_stamp(env_stamp_file, stamp_info)

    return framework_path, install


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
            _get_idf_tools_path() / f"espidf.constraints.v{esp_idf_version}.txt"
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
    env["IDF_TOOLS_PATH"] = str(_get_idf_tools_path())
    env["IDF_PATH"] = ""

    targets = targets or ESPHOME_IDF_DEFAULT_TARGETS

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
    framework_path, installed = _check_esphome_idf_framework_install(
        version, targets, tools, force=force, env=env, source_url=source_url
    )

    features = features or ESPHOME_IDF_DEFAULT_FEATURES

    # 2) Python env
    python_env_path, installed = _check_esp_idf_python_env_install(
        version, features, force=force or installed, env=env
    )

    return framework_path, python_env_path


def _ccache_env() -> dict[str, str]:
    """Return ccache settings for ESP-IDF compiles.

    Enabled by default whenever the ``ccache`` binary is on PATH; set
    ``IDF_CCACHE_ENABLE=0`` in the environment to opt out. The cache lives under
    the IDF tools path. How widely it is shared depends on where that resolves:
    across projects (and surviving ``clean-all``) when it is a common location
    (``ESPHOME_ESP_IDF_PREFIX`` or the add-on ``/data``), but per-project under
    ``.esphome/idf`` for a default pip install, where ``clean-all`` clears it
    along with the framework.

    Depend mode keeps cache-miss overhead low (hashes the compiler's depfiles
    instead of preprocessing). ``CCACHE_BASEDIR`` rewrites the per-build
    absolute paths (generated ``sdkconfig`` include, etc.) so different devices
    share framework cache entries; it is scoped to the build dir on purpose --
    a broader base would also rewrite the shared IDF path under the cache dir
    and lose those hits.

    Only values the user has not already set in the environment are returned, so
    a custom ``CCACHE_DIR`` / ``CCACHE_MAXSIZE`` / etc. is respected.
    """
    # Honor an explicit choice already in the environment (opt-out or opt-in).
    if "IDF_CCACHE_ENABLE" in os.environ:
        if not get_bool_env("IDF_CCACHE_ENABLE"):
            return {}
    elif shutil.which("ccache") is None:
        # ESP-IDF silently skips ccache without the binary; don't enable it.
        return {}

    # ccache is enabled past here. build_path is set during preload for every
    # config-loading command, so it being unset means a caller built the IDF env
    # too early -- fail loudly rather than silently drop CCACHE_BASEDIR (which
    # would quietly cost cross-device cache hits).
    if CORE.build_path is None:
        raise ValueError(
            "CORE.build_path must be set before constructing the ESP-IDF build "
            "environment"
        )

    defaults = {
        "IDF_CCACHE_ENABLE": "1",
        "CCACHE_DIR": str(_get_idf_tools_path() / "ccache"),
        "CCACHE_NOHASHDIR": "true",
        "CCACHE_DEPEND": "1",
        "CCACHE_BASEDIR": str(Path(CORE.build_path).resolve()),
    }
    # Don't override CCACHE_* values the user already set in their environment.
    return {k: v for k, v in defaults.items() if k not in os.environ}


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
    env["IDF_TOOLS_PATH"] = str(_get_idf_tools_path())
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
