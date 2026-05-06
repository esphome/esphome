"""ESP-IDF framework tools for ESPHome."""

from collections.abc import Iterable
from contextlib import ExitStack
import io
import json
import logging
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import IO

import requests

from esphome.config_validation import Version
from esphome.core import CORE
from esphome.helpers import ProgressBar, get_str_env, rmtree

PathType = str | os.PathLike

_LOGGER = logging.getLogger(__name__)

_SCRIPTS_DIR = Path(__file__).parent


def _str_to_lst_of_str(a: str) -> list[str]:
    """
    Convert a string to a list of string

    Args:
        a: A string containing semicolon-separated values

    Returns:
        list of strings
    """
    return list(f.strip() for f in a.split(";") if f.strip())


ESPHOME_STAMP_FILE = ".esphome.stamp.json"

# Cache-buster baked into the stamp file. Bump this whenever a change would
# make pre-existing stamped installs invalid, e.g.:
#   - the inlined Python helpers (_get_idf_version, _get_idf_tool_paths) are
#     rewritten in a way that's incompatible with prior installs
#   - the stamp_info schema changes (keys added/renamed/removed)
#   - the tool selection or env-construction logic changes meaning
# Bumping triggers a full reinstall on every user's next run.
STAMP_SCHEMA_VERSION = "0"

ESPHOME_IDF_DEFAULT_TARGETS = _str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TARGETS", "all")
)

ESPHOME_IDF_DEFAULT_TOOLS = _str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TOOLS", "cmake;ninja")
)

ESPHOME_IDF_DEFAULT_TOOLS_FORCE = _str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_TOOLS_FORCE", "required")
)

ESPHOME_IDF_DEFAULT_FEATURES = _str_to_lst_of_str(
    os.environ.get("ESPHOME_IDF_DEFAULT_FEATURES", "core")
)

ESPHOME_IDF_FRAMEWORK_MIRRORS = _str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_IDF_FRAMEWORK_MIRRORS",
        "https://github.com/espressif/esp-idf/releases/download/v{VERSION}/esp-idf-v{VERSION}.zip;https://github.com/espressif/esp-idf/releases/download/v{MAJOR}.{MINOR}/esp-idf-v{MAJOR}.{MINOR}.zip",
    )
)

ESP_IDF_CONSTRAINTS_MIRRORS = _str_to_lst_of_str(
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
        return Path(get_str_env("ESPHOME_ESP_IDF_PREFIX", None)).expanduser()
    return CORE.data_dir / "idf"


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


def rmdir(directory: PathType, msg: str | None = None):
    """
    Remove a directory and its contents recursively if it exists.

    Args:
        directory: Path to the directory to be removed
        msg: Optional debug message to log before removal or it an error occurs

    Returns:
        None

    Raises:
        RuntimeError: If directory removal fails
    """
    if os.path.isdir(directory):
        try:
            if msg:
                _LOGGER.debug(msg)
            rmtree(directory)
        except OSError as e:
            raise RuntimeError(
                f"Error during {msg}: can't remove `{directory}`. Please remove it manually!"
            ) from e


def _get_pythonexe_path() -> str:
    """
    Get the path to the Python executable.

    Returns:
        Path to Python executable as string
    """
    # Try to get PYTHONEXEPATH environment variable
    # Fallback to sys.executable if not set
    return os.environ.get("PYTHONEXEPATH", os.path.normpath(sys.executable))


def _get_python_env_executable_path(root: PathType, binary: str) -> Path:
    """
    Get the path to a Python environment executable file.

    Args:
        root: Root directory of the Python environment
        binary: Name of the executable binary

    Returns:
        Path object pointing to the executable file
    """
    if os.name == "nt":
        return Path(root) / "Scripts" / f"{binary}.exe"
    return Path(root) / "bin" / binary


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
        with open(file, encoding="utf-8") as f:
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
    with open(file, "w", encoding="utf8") as fp:
        json.dump(data, fp)


def _exec(
    cmd: list[str],
    msg: str | None = None,
    env: dict[str, str] | None = None,
    stream_output: bool = False,
) -> tuple[bool, str | None, str | None]:
    """
    Execute a command and return results.

    Args:
        cmd: list of command arguments
        msg: Optional custom message for logging
        env: Optional dictionary of environment variables to set
        stream_output: If True, inherit parent stdio so the subprocess prints
            directly to the terminal (useful for commands that produce their
            own progress output). stdout/stderr are not captured in this mode.

    Returns:
        tuple of (success: bool, stdout: str or None, stderr: str or None).
        When stream_output is True, stdout and stderr are always None.
    """
    cmd_str = msg or " ".join(cmd)
    try:
        _LOGGER.debug("%s - running ...", cmd_str)

        run_env = os.environ.copy()
        if env:
            run_env.update(env)

        if stream_output:
            result = subprocess.run(cmd, check=False, env=run_env)
            stdout = stderr = None
        else:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False,
                env=run_env,
            )
            stdout = result.stdout
            stderr = result.stderr

        if result.returncode != 0:
            if stream_output:
                _LOGGER.error("%s - failed (returncode=%s)", cmd_str, result.returncode)
            else:
                tail = (stderr or stdout or "").strip()[-1000:]
                _LOGGER.error(
                    "%s - failed (returncode=%s). Tail:\n%s",
                    cmd_str,
                    result.returncode,
                    tail,
                )
            return False, stdout, stderr

        _LOGGER.debug("%s - executed successfully", cmd_str)
        return True, stdout, stderr

    except (subprocess.SubprocessError, OSError) as e:
        _LOGGER.error("%s - error: %s", cmd_str, str(e))
        return False, None, None


def _exec_ok(*args, **kwargs) -> bool:
    """
    Execute a command and return only the success status.

    Args:
        *args: Positional arguments to pass to _exec function
        **kwargs: Keyword arguments to pass to _exec function

    Returns:
        True if command executed successfully, False otherwise
    """
    return _exec(*args, **kwargs)[0]


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
        _get_pythonexe_path(),
        str(_SCRIPTS_DIR / "get_idf_version.py"),
        str(idf_framework_root),
    ]

    success, stdout, stderr = _exec(
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
        _get_pythonexe_path(),
        str(_SCRIPTS_DIR / "get_idf_tool_paths.py"),
        str(idf_framework_root),
    ]

    success, stdout, stderr = _exec(
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

    success, stdout, _ = _exec(cmd, msg="Python version", env=env)

    if stdout:
        stdout = stdout.strip()
    if throw_exception and (not success or not stdout):
        raise RuntimeError(f"Can't get Python version of {python_executable}")
    return stdout


def _create_venv(root: PathType, msg: str | None = None):
    """
    Create a Python virtual environment.

    Args:
        root: Path to the virtual environment directory
        msg: Optional message for logging

    Returns:
        None

    Raises:
        Exception: If virtual environment creation fails
    """
    cmd = [_get_pythonexe_path(), "-m", "venv", "--clear", root]
    if not _exec_ok(cmd, msg=f"Create Python virtual environment for {msg}"):
        raise RuntimeError(f"Can't create Python virtual environment for {msg}")


def _detect_archive_root(names: Iterable[str]) -> str | None:
    """Detect a single top-level directory shared by all archive entries.

    Returns the directory name if every non-empty entry sits under the same
    top-level directory, else ``None``. Extraction helpers use this to strip
    the wrapper directory commonly found in source archives during extraction
    rather than renaming it afterwards — post-extraction renames are
    unreliable on Windows because antivirus and the search indexer briefly
    hold handles on freshly written files.
    """
    root: str | None = None
    has_descendant = False
    for raw in names:
        name = raw.replace("\\", "/").strip("/")
        if not name:
            continue
        first, sep, _ = name.partition("/")
        if root is None:
            root = first
        elif root != first:
            return None
        if sep:
            has_descendant = True
    return root if has_descendant else None


def _tar_extract_all(
    data: io.BufferedIOBase,
    extract_dir: PathType = ".",
    progress_header: str | None = None,
):
    """
    Extract a TAR archive to the specified directory.

    Implementation is inspired by Python 3.12's tarfile data filtering logic.
    This can be replaced with the standard library implementation once
    support for Python 3.11 is no longer required.

    Args:
        data: File-like object containing the TAR archive
        extract_dir: Directory to extract contents to
        progress_header: If set, show a progress bar with this header
    """
    import stat
    import tarfile

    extract_dir = os.fspath(extract_dir)
    abs_dest = os.path.abspath(extract_dir)

    with tarfile.open(fileobj=data, mode="r") as tar_ref:
        all_members = tar_ref.getmembers()

        # Detect a single common top-level directory and strip it during
        # extraction so we don't have to flatten it via a rename afterwards.
        strip_root = _detect_archive_root(m.name for m in all_members)
        strip_prefix = f"{strip_root}/" if strip_root is not None else None

        safe_members = []

        for member in all_members:
            name = member.name

            # 1. Strip leading slashes
            name = name.lstrip("/" + os.sep)

            # 2. Reject absolute paths (incl. Windows drive)
            if os.path.isabs(name) or (
                os.name == "nt" and ":" in name.split(os.sep)[0]
            ):
                continue

            # 3. Strip wrapper directory if one was detected
            if strip_prefix is not None:
                norm = name.replace("\\", "/")
                if norm in (strip_root, strip_prefix):
                    continue
                if not norm.startswith(strip_prefix):
                    continue
                name = norm[len(strip_prefix) :]

            # 4. Compute final path
            target_path = os.path.realpath(os.path.join(abs_dest, name))
            if os.path.commonpath([abs_dest, target_path]) != abs_dest:
                continue

            # 5. Validate links properly
            if member.issym() or member.islnk():
                linkname = member.linkname

                # Reject absolute link targets
                if os.path.isabs(linkname):
                    continue

                # Strip leading slashes
                linkname = os.path.normpath(linkname)

                if member.issym():
                    link_target = os.path.join(
                        abs_dest, os.path.dirname(name), linkname
                    )
                else:
                    link_target = os.path.join(abs_dest, linkname)
                link_target = os.path.realpath(link_target)

                if os.path.commonpath([abs_dest, link_target]) != abs_dest:
                    continue

                # write back normalized linkname
                member.linkname = linkname

            # 6. Sanitize permissions
            mode = member.mode
            if mode is not None:
                # Strip high bits & group/other write bits
                mode &= (
                    stat.S_IRWXU
                    | stat.S_IRGRP
                    | stat.S_IXGRP
                    | stat.S_IROTH
                    | stat.S_IXOTH
                )
                if member.isfile() or member.islnk():
                    # remove exec bits unless explicitly user-executable
                    if not (mode & stat.S_IXUSR):
                        mode &= ~(stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
                    mode |= stat.S_IRUSR | stat.S_IWUSR
                elif member.isdir() or member.issym():
                    # Ignore mode for directories & symlinks
                    mode = None
                else:
                    # Block special files
                    continue

                member.mode = mode

            # 7. Strip ownership
            member.uid = None
            member.gid = None
            member.uname = None
            member.gname = None

            # 8. Assign sanitized name back
            member.name = name

            safe_members.append(member)

        total = len(safe_members)
        progress = (
            ProgressBar(progress_header) if progress_header and total > 0 else None
        )
        for i, member in enumerate(safe_members, 1):
            tar_ref.extract(member, abs_dest)
            if progress is not None:
                progress.update(i / total)
        if progress is not None:
            progress.update(1)


def _zip_extract_all(
    data: io.BufferedIOBase,
    extract_dir: PathType = ".",
    progress_header: str | None = None,
):
    """
    Extract a ZIP archive to the specified directory.

    Args:
        data: File-like object containing the ZIP archive
        extract_dir: Directory to extract contents to
        progress_header: If set, show a progress bar with this header
    """
    import zipfile

    extract_dir = os.path.abspath(extract_dir)

    with zipfile.ZipFile(data, "r") as zip_ref:
        all_members = zip_ref.infolist()

        # Detect a single common top-level directory and strip it during
        # extraction so we don't have to flatten it via a rename afterwards.
        strip_root = _detect_archive_root(m.filename for m in all_members)
        strip_prefix = f"{strip_root}/" if strip_root is not None else None

        total = len(all_members)
        progress = (
            ProgressBar(progress_header) if progress_header and total > 0 else None
        )

        for i, member in enumerate(all_members, 1):
            # 1. Normalize name
            name = member.filename.lstrip("/\\")

            # 2. Reject absolute paths / Windows drives
            if os.path.isabs(name) or (
                os.name == "nt" and ":" in name.split(os.sep)[0]
            ):
                continue

            # 3. Strip wrapper directory if one was detected
            if strip_prefix is not None:
                norm = name.replace("\\", "/")
                if norm in (strip_root, strip_prefix):
                    continue
                if not norm.startswith(strip_prefix):
                    continue
                name = norm[len(strip_prefix) :]

            # 4. Compute safe target path
            target_path = os.path.abspath(os.path.join(extract_dir, name))

            if os.path.commonpath([extract_dir, target_path]) != extract_dir:
                raise ValueError(f"Unsafe path detected: {member.filename}")

            # 5. Assign sanitized name back
            member.filename = name

            # 6. Extract
            zip_ref.extract(member, extract_dir)

            if progress is not None:
                progress.update(i / total)
        if progress is not None:
            progress.update(1)


_ARCHIVE_MAGIC_MAP = {
    b"\x1f\x8b\x08": _tar_extract_all,
    b"\x42\x5a\x68": _tar_extract_all,
    b"\xfd\x37\x7a\x58\x5a\x00": _tar_extract_all,
    b"\x50\x4b\x03\x04": _zip_extract_all,
}


def archive_extract_all(
    archive: PathType | io.RawIOBase | IO[bytes],
    extract_dir: PathType = ".",
    progress_header: str | None = None,
):
    """
    Extract an archive file to the specified directory.

    Args:
        archive: Path to archive file or file-like object
        extract_dir: Directory to extract contents to
        progress_header: If set, show a progress bar with this header

    Raises:
        TypeError: If archive is not a valid type
        ValueError: If archive format is unsupported
    """

    # 1. Handle different archive input types
    with ExitStack() as stack:
        archive_ref: io.BufferedIOBase
        if isinstance(archive, (str, os.PathLike)):
            archive_ref = stack.enter_context(open(archive, "rb"))
        elif isinstance(archive, (io.BufferedReader, io.BufferedRandom)):
            archive_ref = archive
        elif isinstance(archive, io.RawIOBase):
            archive_ref = io.BufferedReader(archive)
        else:
            raise TypeError(
                f"archive must be str, Path, or file-like object: {type(archive)}"
            )

        # 2. Detect archive format and select appropriate extraction function
        matched_fct = None
        magic_len = max(len(k) for k in _ARCHIVE_MAGIC_MAP)
        header = archive_ref.peek(magic_len)
        for magic, fct in _ARCHIVE_MAGIC_MAP.items():
            if header.startswith(magic):
                matched_fct = fct
                break
        if matched_fct is None:
            raise ValueError("Unsupported archive format")
        matched_fct(archive_ref, extract_dir, progress_header=progress_header)


def download_from_mirrors(
    mirrors: list[str],
    substitutions: dict[str, str],
    target: io.RawIOBase | IO[bytes] | PathType,
    timeout: int = 30,
) -> str | None:
    """
    Download file from multiple mirrors with substitution support.

    Args:
        mirrors: list of mirror URLs
        substitutions: Dictionary of substitutions to apply to URLs
        target: Target file path or file-like object
        timeout: Download timeout in seconds

    Returns:
        The source URL.

    Raises:
        Exception: If all download attempts fail
    """
    # 1. Open target file for writing if path given
    with ExitStack() as stack:
        if isinstance(target, (str, os.PathLike)):
            f = stack.enter_context(open(target, "wb"))
        elif isinstance(target, (io.RawIOBase, io.IOBase)):
            f = target
        else:
            raise TypeError(
                f"target must be str, Path, or file-like object: {type(target)}"
            )

        # 2. Try each mirror in order
        last_exception = None

        for mirror in mirrors:
            # 3. Apply substitutions to URL
            url = mirror.format(**substitutions)

            _LOGGER.debug("Trying downloading from %s", url)

            try:
                # 4. Reset file pointer and download
                f.seek(0)
                f.truncate(0)

                with requests.get(url, stream=True, timeout=timeout) as r:
                    r.raise_for_status()

                    total_size = int(r.headers.get("content-length", 0))
                    downloaded = 0

                    progress = ProgressBar("Downloading") if total_size > 0 else None

                    for chunk in r.iter_content(chunk_size=8192):
                        if chunk:
                            f.write(chunk)

                        downloaded += len(chunk)

                        if progress is not None:
                            progress.update(downloaded / total_size)

                    if progress is not None:
                        progress.update(1)

                _LOGGER.debug("Downloaded successfully from: %s", url)

                # 6. Reset file pointer and return
                f.seek(0)
                return url

            except Exception as e:  # pylint: disable=broad-exception-caught
                _LOGGER.debug("Failed to download %s: %s", url, str(e))
                last_exception = e

        # 7. Raise last exception if all mirrors failed
        if last_exception:
            raise last_exception
        return None


def _check_esphome_idf_framework_install(
    version: str,
    targets: list[str],
    tools: list[str],
    force: bool = False,
    env: dict[str, str] | None = None,
) -> tuple[Path, bool]:
    """
    Check and install ESP-IDF framework.

    Args:
        version: ESP-IDF version to check/install
        targets: Target platforms to install
        tools: list of tools to install
        force: If True, force reinstallation
        env: Optional dictionary of environment variables to set

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
                substitutions["EXTRA"] = ver.extra
            except ValueError:
                pass

            download_from_mirrors(
                ESPHOME_IDF_FRAMEWORK_MIRRORS, substitutions, tmp.file
            )

            _LOGGER.info("Extracting ESP-IDF %s framework ...", version)
            archive_extract_all(tmp.file, framework_path, progress_header="Extracting")
            extracted_marker.touch()

    # 3. Check if the framework tools are the same and correctly installed
    if not install:
        install = True
        if _check_stamp(env_stamp_file, stamp_info):
            _LOGGER.info("Checking ESP-IDF %s framework installation ...", version)
            cmd = [
                _get_pythonexe_path(),
                str(idf_tools_path),
                "--non-interactive",
                "check",
            ]
            if _exec_ok(cmd, msg=f"ESP-IDF {version} check", env=env):
                install = False

    # 4. Install framework tools if not installed or needs update
    if install:
        _LOGGER.info("Installing ESP-IDF %s framework ...", version)
        targets_str = ",".join(targets)
        cmd = [
            _get_pythonexe_path(),
            str(idf_tools_path),
            "--non-interactive",
            "install",
            f"--targets={targets_str}",
        ] + tools
        if not _exec_ok(
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
    env_python_path = _get_python_env_executable_path(python_env_path, "python")

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

        _create_venv(python_env_path, msg=f"ESP-IDF {version}")

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
        if not _exec_ok(
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
            if not _exec_ok(
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
) -> tuple[Path, Path]:
    """
    Check and install ESP-IDF framework and Python environment.

    Args:
        version: ESP-IDF version to check/install
        targets: Target platforms to install
        tools: list of tools to install
        features: Features to install
        force: If True, force reinstallation

    Returns:
        tuple of (framework_path, python_env_path)
    """
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
        version, targets, tools, force=force, env=env
    )

    features = features or ESPHOME_IDF_DEFAULT_FEATURES

    # 2) Python env
    python_env_path, installed = _check_esp_idf_python_env_install(
        version, features, force=force or installed, env=env
    )

    return framework_path, python_env_path


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
        python_path = _get_python_env_executable_path(python_env_path, "python")
        path_list.insert(0, str(python_path.parent))
        env["IDF_PYTHON_ENV_PATH"] = str(python_env_path)

    # 4. Set framework-specific environment variables
    env["IDF_PATH"] = str(framework_path)
    env["ESP_IDF_VERSION"] = _get_idf_version(framework_path, env)

    # 5. Get and add tool paths and environment variables
    paths_to_export, export_vars = _get_idf_tool_paths(framework_path, env)
    env.update(export_vars)
    env["PATH"] = os.pathsep.join(paths_to_export + path_list)

    return env
