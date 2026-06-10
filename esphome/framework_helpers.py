"""Generic toolchain installation helpers shared across framework implementations."""

from collections.abc import Iterable
from contextlib import ExitStack
import io
import logging
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import IO

import requests

from esphome.helpers import ProgressBar, rmtree

PathType = str | os.PathLike

_LOGGER = logging.getLogger(__name__)


def get_project_link_flags() -> list[str]:
    """Return the sorted -Wl, linker flags from the current build."""
    from esphome.core import CORE  # local import to avoid circular dependency

    return sorted(flag for flag in CORE.build_flags if flag.startswith("-Wl,"))


def get_project_compile_flags() -> list[str]:
    """Return the sorted -D and -W (non-linker) flags from the current build."""
    from esphome.core import CORE  # local import to avoid circular dependency

    return [
        flag
        for flag in sorted(CORE.build_flags)
        if flag.startswith("-D")
        or (flag.startswith("-W") and not flag.startswith("-Wl,"))
    ]


def str_to_lst_of_str(a: str | list[str]) -> list[str]:
    """
    Convert a string to a list of string

    Args:
        a: A string containing semicolon-separated values, or an already-split list

    Returns:
        list of strings
    """
    if isinstance(a, list):
        return a
    return [f.strip() for f in a.split(";") if f.strip()]


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
    if Path(directory).is_dir():
        try:
            if msg:
                _LOGGER.debug(msg)
            rmtree(directory)
        except OSError as e:
            raise RuntimeError(
                f"Error during {msg}: can't remove `{directory}`. Please remove it manually!"
            ) from e


def get_system_python_path() -> str:
    """
    Get the path to the Python executable.

    Returns:
        Path to Python executable as string
    """
    # Try to get PYTHONEXEPATH environment variable
    # Fallback to sys.executable if not set
    return os.environ.get("PYTHONEXEPATH", os.path.normpath(sys.executable))


def get_python_env_executable_path(root: PathType, binary: str) -> Path:
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


def run_command(
    cmd: list[str],
    msg: str | None = None,
    env: dict[str, str] | None = None,
    stream_output: bool = False,
    cwd: PathType | None = None,
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
        cwd: Optional working directory for the subprocess.

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
            result = subprocess.run(cmd, check=False, env=run_env, cwd=cwd)
            stdout = stderr = None
        else:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False,
                env=run_env,
                cwd=cwd,
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


def run_command_ok(*args, **kwargs) -> bool:
    """
    Execute a command and return only the success status.

    Args:
        *args: Positional arguments to pass to run_command
        **kwargs: Keyword arguments to pass to run_command

    Returns:
        True if command executed successfully, False otherwise
    """
    return run_command(*args, **kwargs)[0]


def create_venv(root: PathType, msg: str | None = None):
    """
    Create a Python virtual environment.

    Args:
        root: Path to the virtual environment directory
        msg: Optional message for logging

    Returns:
        None

    Raises:
        RuntimeError: If virtual environment creation fails
    """
    cmd = [get_system_python_path(), "-m", "venv", "--clear", root]
    if not run_command_ok(cmd, msg=f"Create Python virtual environment for {msg}"):
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

    # Tar extraction safety: os.path.realpath / commonpath / normpath have no
    # pathlib equivalents and Path.resolve() would follow symlinks unsafely.
    # Use os.path for the security-sensitive parts; the simple checks move to
    # Path.
    extract_dir = os.fspath(extract_dir)
    abs_dest = os.path.abspath(extract_dir)  # noqa: PTH100

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
            if Path(name).is_absolute() or (
                os.name == "nt" and ":" in name.split(os.sep)[0]  # noqa: PTH206
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
            target_path = os.path.realpath(os.path.join(abs_dest, name))  # noqa: PTH118
            if os.path.commonpath([abs_dest, target_path]) != abs_dest:
                continue

            # 5. Validate links properly
            if member.issym() or member.islnk():
                linkname = member.linkname

                # Reject absolute link targets
                if Path(linkname).is_absolute():
                    continue

                if member.islnk() and strip_prefix is not None:
                    # Hard-link linknames reference another archive member
                    # by its archive name. We've stripped the wrapper prefix
                    # from member.name above (step 3); strip it here too so
                    # tarfile._find_link_target can resolve the target during
                    # extraction. Symlink linknames are filesystem-relative
                    # paths, not archive-member references, so they don't
                    # need this treatment.
                    norm_link = linkname.replace("\\", "/")
                    if norm_link in (strip_root, strip_prefix):
                        continue
                    if not norm_link.startswith(strip_prefix):
                        continue
                    linkname = norm_link[len(strip_prefix) :]

                # Strip leading slashes
                linkname = os.path.normpath(linkname)

                if member.issym():
                    link_target = os.path.join(  # noqa: PTH118
                        abs_dest,
                        os.path.dirname(name),  # noqa: PTH120
                        linkname,
                    )
                else:
                    link_target = os.path.join(abs_dest, linkname)  # noqa: PTH118
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
                elif not (member.isdir() or member.issym()):
                    # Block special files. Directories and symlinks keep
                    # their masked-original mode — passing None here would
                    # crash tarfile.extract on Python <3.12 (its chmod
                    # path calls os.chmod unconditionally).
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

    # See note in _tar_extract_all: os.path is used intentionally for
    # the security-sensitive abspath/commonpath checks below.
    extract_dir = os.path.abspath(extract_dir)  # noqa: PTH100

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
            if Path(name).is_absolute() or (
                os.name == "nt" and ":" in name.split(os.sep)[0]  # noqa: PTH206
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
            target_path = os.path.abspath(os.path.join(extract_dir, name))  # noqa: PTH100, PTH118

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


def _rename_with_retry(src: Path, dst: Path, attempts: int = 5) -> None:
    """Rename ``src`` to ``dst`` with backoff retries on Windows sharing violations.

    Antivirus/indexer handles on freshly-written files can briefly block
    ``os.rename`` with ERROR_SHARING_VIOLATION / ERROR_ACCESS_DENIED. The
    handle is released within tens of ms in practice, so exponential backoff
    works.
    """
    for i in range(attempts):
        try:
            src.rename(dst)
            return
        except PermissionError:
            if i == attempts - 1:
                raise
            time.sleep(0.1 * (2**i))


def _7z_extract_all(
    data: io.BufferedIOBase,
    extract_dir: PathType = ".",
    progress_header: str | None = None,
):
    """
    Extract a 7z archive to the specified directory.

    py7zr only supports bulk extraction (no per-member rename hook like
    tarfile/zipfile), so we extract into a unique staging subdir of
    ``extract_dir`` and then move children up. This keeps everything on
    the same volume and sidesteps wrapper-vs-child name collisions
    (e.g. ``arm-zephyr-eabi/`` containing another ``arm-zephyr-eabi/``).

    Args:
        data: File-like object containing the 7z archive (must be seekable)
        extract_dir: Directory to extract contents to
        progress_header: If set, show a progress bar with this header
    """
    import py7zr

    extract_dir = os.path.abspath(extract_dir)  # noqa: PTH100
    Path(extract_dir).mkdir(parents=True, exist_ok=True)

    suffix = 0
    while True:
        staging = Path(extract_dir) / f".extract_tmp_{suffix}"
        if not staging.exists():
            break
        suffix += 1
    staging.mkdir()

    try:
        with py7zr.SevenZipFile(data, "r") as z:
            all_names = z.getnames()

            # Detect a single common top-level directory to flatten.
            strip_root = _detect_archive_root(all_names)

            # Validate names: reject absolute paths, Windows drives, and
            # path traversal. Filter via targets= since py7zr can't rename
            # per-member.
            safe_targets: list[str] = []
            for raw in all_names:
                name = raw.lstrip("/\\")
                if not name:
                    continue
                if Path(name).is_absolute() or (
                    os.name == "nt" and ":" in name.split(os.sep)[0]  # noqa: PTH206
                ):
                    continue
                target_path = os.path.abspath(os.path.join(staging, name))  # noqa: PTH100, PTH118
                if os.path.commonpath([str(staging), target_path]) != str(staging):
                    continue
                safe_targets.append(raw)

            progress = (
                ProgressBar(progress_header)
                if progress_header and safe_targets
                else None
            )

            if len(safe_targets) == len(all_names):
                z.extractall(path=staging)
            else:
                z.extract(path=staging, targets=safe_targets)

            if progress is not None:
                progress.update(1)

        src_root = staging / strip_root if strip_root else staging
        for item in src_root.iterdir():
            dest = Path(extract_dir) / item.name
            if dest.exists():
                if dest.is_dir():
                    rmtree(dest)
                else:
                    dest.unlink()
            _rename_with_retry(item, dest)
    finally:
        # staging is created before the try, so it always exists here; the
        # guard is defensive cleanup and its False branch is unreachable.
        if staging.exists():  # pragma: no cover
            rmtree(staging)


_ARCHIVE_MAGIC_MAP = {
    b"\x1f\x8b\x08": _tar_extract_all,
    b"\x42\x5a\x68": _tar_extract_all,
    b"\xfd\x37\x7a\x58\x5a\x00": _tar_extract_all,
    b"\x50\x4b\x03\x04": _zip_extract_all,
    b"\x37\x7a\xbc\xaf\x27\x1c": _7z_extract_all,
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
            archive_ref = stack.enter_context(Path(archive).open("rb"))
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
) -> str:
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
        ValueError: If mirrors list is empty.
        Exception: If all download attempts fail.
    """
    # 1. Open target file for writing if path given
    with ExitStack() as stack:
        if isinstance(target, (str, os.PathLike)):
            f = stack.enter_context(Path(target).open("wb"))
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

            except Exception as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
                _LOGGER.debug("Failed to download %s: %s", url, str(e))
                last_exception = e

        # 7. Raise last exception if all mirrors failed
        if last_exception:
            raise last_exception
        raise ValueError("download_from_mirrors called with an empty mirrors list")
