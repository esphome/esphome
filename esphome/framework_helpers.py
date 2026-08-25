"""Generic toolchain installation helpers shared across framework implementations."""

from collections.abc import Callable, Iterable, Iterator
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, contextmanager, suppress
import hashlib
import io
import json
import logging
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
from typing import IO, TYPE_CHECKING

from esphome.helpers import ProgressBar, rmtree
from esphome.net_retry import (
    NETWORK_MAX_ATTEMPTS,
    http_request,
    is_transient_download_error,
)

if TYPE_CHECKING:
    import requests

PathType = str | os.PathLike

_LOGGER = logging.getLogger(__name__)


# Attempts per mirror URL before falling through to the next mirror; only
# mid-stream drops retry (resuming when the server gave a validator),
# connect errors move on to the next mirror immediately.
_MIRROR_ATTEMPTS = 3

# Passes over the whole mirror list when a transient network error is in
# the mix; shares net_retry's policy (3 tries, 2s/4s backoff), which in
# turn matches git.py's _NETWORK_MAX_ATTEMPTS.
_MIRROR_SWEEP_ATTEMPTS = NETWORK_MAX_ATTEMPTS


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


def get_project_cxx_compile_flags() -> list[str]:
    """Return the sorted flags that apply to C++ compiles only."""
    from esphome.core import CORE  # local import to avoid circular dependency

    return sorted(CORE.cxx_build_flags)


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
        # Do not leak PYTHONPATH
        run_env.pop("PYTHONPATH", None)
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

    Path-traversal, link, permission and ownership sanitization is delegated to
    the stdlib ``tarfile.data_filter`` (PEP 706). We keep the wrapper-directory
    stripping (no stdlib equivalent) and the absolute-path reject (data_filter's
    check is os.path-dependent and would miss a Windows drive path when
    extracting on POSIX).

    Args:
        data: File-like object containing the TAR archive
        extract_dir: Directory to extract contents to
        progress_header: If set, show a progress bar with this header
    """
    import tarfile

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
            # Strip leading slashes, then reject absolute / Windows-drive paths
            name = member.name.lstrip("/" + os.sep)
            if Path(name).is_absolute() or (
                os.name == "nt" and ":" in name.split(os.sep)[0]  # noqa: PTH206
            ):
                continue

            # Strip wrapper directory if one was detected
            if strip_prefix is not None:
                norm = name.replace("\\", "/")
                if norm in (strip_root, strip_prefix):
                    continue
                if not norm.startswith(strip_prefix):
                    continue
                name = norm[len(strip_prefix) :]
            member.name = name

            # Hard-link linknames reference another archive member by its
            # archive name; strip the wrapper prefix here too so
            # tarfile._find_link_target can resolve the target during
            # extraction. Symlink linknames are filesystem-relative paths,
            # not archive-member references, so they don't need this.
            if member.islnk() and strip_prefix is not None:
                norm_link = member.linkname.replace("\\", "/")
                if norm_link in (strip_root, strip_prefix):
                    continue
                if not norm_link.startswith(strip_prefix):
                    continue
                member.linkname = norm_link[len(strip_prefix) :]

            # Delegate traversal, link, permission and ownership sanitization
            # to the stdlib data filter; it raises FilterError for unsafe
            # members (path traversal, links outside dest, special files).
            try:
                member = tarfile.data_filter(member, abs_dest)
            except tarfile.FilterError:
                continue

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


def _rename_with_retry(
    src: Path, dst: Path, attempts: int = 5, overwrite: bool = False
) -> None:
    """Rename ``src`` to ``dst`` with backoff retries on Windows sharing violations.

    Antivirus/indexer handles on freshly-written files can briefly block
    ``os.rename`` with ERROR_SHARING_VIOLATION / ERROR_ACCESS_DENIED. The
    handle is released within tens of ms in practice, so exponential backoff
    works. With ``overwrite`` an existing ``dst`` is replaced instead of
    failing.
    """
    for i in range(attempts):
        try:
            if overwrite:
                src.replace(dst)
            else:
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

    with ExitStack() as stack:
        # 1. Handle different archive input types
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


def _open_ranged(
    url: str, offset: int, timeout: int, validator: str | None = None
) -> tuple["requests.Response | None", int]:
    """Open a streaming GET, asking the server to resume at ``offset``.

    ``validator`` is an ETag or Last-Modified value from the interrupted
    response; it is sent as ``If-Range`` so the server only honors the Range
    when the content is unchanged, replying 200 (full body, restart) if the
    file was replaced between requests — the resumed bytes can then never be
    stitched onto a different file's prefix.

    Returns ``(response, effective_offset)``. The response is None when the
    server answered 416 Range Not Satisfiable: the file holds every byte the
    server has (a previous attempt was interrupted after the last byte), so
    there is nothing to stream and the caller's verification decides whether
    the file is good. The offset drops to 0 when the server ignored the
    ``Range`` header (no 206), meaning the caller must restart the file.
    Raises on connect errors and HTTP error statuses; the response is closed
    on failure.
    """
    headers = {"Range": f"bytes={offset}-"} if offset else {}
    if offset and validator:
        headers["If-Range"] = validator
    resp = http_request("GET", url, stream=True, timeout=timeout, headers=headers)
    if offset and resp.status_code == 416:
        resp.close()
        return None, offset
    if offset and resp.status_code != 206:
        _LOGGER.debug(
            "Server did not resume %s (HTTP %s), restarting", url, resp.status_code
        )
        offset = 0
    if not resp.ok:
        resp.close()
    resp.raise_for_status()
    if offset:
        _LOGGER.info("Resuming download at %d bytes ...", offset)
    return resp, offset


def _verify_file(path: Path, sha256: str | None, size: int | None) -> None:
    """Raise EsphomeError when ``path`` fails an available sha256/size check."""
    from esphome.core import EsphomeError

    if size is not None and path.stat().st_size != size:
        raise EsphomeError(f"size mismatch: expected {size}, got {path.stat().st_size}")
    if sha256 is not None:
        with path.open("rb") as f:
            digest = hashlib.file_digest(f, "sha256").hexdigest()
        if digest != sha256:
            raise EsphomeError(f"sha256 mismatch: got {digest}")


def _load_download_meta(meta: Path, url: str) -> tuple[str | None, int]:
    """Return the ``(validator, total)`` a previous run recorded for ``url``.

    ``(None, 0)`` when there is no sidecar, it is unreadable, or it belongs
    to a different URL (e.g. a different mirror was tried last time).
    """
    try:
        with meta.open(encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None, 0
    if not isinstance(data, dict) or data.get("url") != url:
        return None, 0
    validator = data.get("validator")
    total = data.get("total")
    return (
        validator if isinstance(validator, str) else None,
        total if isinstance(total, int) else 0,
    )


def _write_download_meta(
    meta: Path, url: str, validator: str | None, total: int
) -> None:
    """Persist resume metadata next to the part file; best-effort.

    Without a validator there is nothing a later run could resume against,
    so any stale sidecar is removed instead.
    """
    try:
        if validator is None:
            meta.unlink(missing_ok=True)
        else:
            meta.write_text(
                json.dumps({"url": url, "validator": validator, "total": total}),
                encoding="utf-8",
            )
    except OSError as e:
        _LOGGER.debug("Could not update download metadata %s: %s", meta, e)


def _content_length(resp: "requests.Response") -> int:
    """Return the response's Content-Length, or 0 when absent or malformed.

    0 means "unknown", which downstream disables the progress bar and the
    resume/completeness logic — a garbage header from a broken proxy must
    degrade to a plain single-stream download, not crash the attempt.
    """
    try:
        return int(resp.headers.get("content-length", 0))
    except ValueError:
        return 0


def _response_validator(resp: "requests.Response") -> str | None:
    """Return the response's strong validator for ``If-Range`` resumes.

    Weak ETags (``W/...``) are not usable for byte-range conditionals, so
    fall back to Last-Modified, or None when the server offers neither.
    """
    etag = resp.headers.get("ETag")
    if etag and not etag.startswith("W/"):
        return etag
    return resp.headers.get("Last-Modified")


def _stream_response_to_file(
    resp: "requests.Response",
    f: IO[bytes],
    offset: int,
    size: int | None = None,
    progress: Callable[[int], None] | None = None,
) -> None:
    """Stream an open ``_open_ranged`` response body into ``f`` at ``offset``.

    Truncates ``f`` to ``offset`` first, so a server-rejected resume
    (effective offset 0) discards the stale bytes. ``offset`` also seeds the
    progress bar so a resumed download shows overall progress. ``size`` is
    the known full file size; when None it is derived from the response's
    content-length, and without either there is no bar. With ``progress``
    set no bar is drawn here; the callback gets the absolute byte count.
    """
    f.seek(offset)
    f.truncate(offset)
    total_size = size or offset + _content_length(resp)
    downloaded = offset
    own_bar: ProgressBar | None = None
    if progress is None:
        own_bar = ProgressBar("Downloading") if total_size > 0 else None
        progress = (
            (lambda done: own_bar.update(done / total_size))
            if own_bar
            else (lambda _: None)
        )
    progress(downloaded)
    for chunk in resp.iter_content(chunk_size=256 * 1024):
        if chunk:
            f.write(chunk)
            downloaded += len(chunk)
        progress(downloaded)
    if own_bar is not None:
        own_bar.update(1)


# Concurrent downloads per batch; enough to hide latency without
# hammering the host or the mirrors.
BATCH_DOWNLOAD_WORKERS = 4


def run_batch_downloads(
    header: str,
    jobs: list[tuple[str, int, Callable[[Callable[[int], None]], None]]],
    max_workers: int = BATCH_DOWNLOAD_WORKERS,
) -> list[tuple[str, BaseException]]:
    """Run ``(name, size, fetch)`` download jobs concurrently under one bar.

    Each ``fetch(tracker)`` reports absolute byte counts; the bar total is
    the sum of the sizes. Failures are returned after the bar is done so
    warnings never land on its row. Ctrl-C drops queued jobs and aborts
    in-flight ones at their next progress tick or backoff boundary (a
    parked socket read defers that by its timeout, and an in-progress
    archive extraction runs to completion); resumable destinations
    (``download_with_resume``) keep their fetched ``.part`` bytes.
    ``jobs`` must be non-empty.
    """
    progress = _BatchDownloadProgress(header, sum(size for _, size, _ in jobs))
    cancelled = threading.Event()

    def _run(
        name: str, fetch: Callable[[Callable[[int], None]], None]
    ) -> tuple[str, BaseException] | None:
        tracker = progress.tracker()

        def checked(done: int) -> None:
            if cancelled.is_set():
                raise _BatchDownloadCancelled
            tracker(done)

        try:
            fetch(checked)
        except (_BatchDownloadCancelled, Exception) as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            # The cancelled arm exists for the tracker rollback below; the
            # batch re-raises the interrupt, so the list is never returned
            # after Ctrl-C. A bar-frame write failure must not displace the
            # download error.
            with suppress(Exception):
                tracker(0)
            failure = (name, err)
        else:
            failure = None
        return failure

    ex = ThreadPoolExecutor(max_workers=max_workers)
    try:
        with progress.logging_guard():
            futures = [ex.submit(_run, name, fetch) for name, _, fetch in jobs]
            return [failure for f in futures if (failure := f.result()) is not None]
    except BaseException:
        # Without this the non-daemon workers download to completion before
        # the interpreter can exit, making Ctrl-C ineffective for minutes
        cancelled.set()
        raise
    finally:
        ex.shutdown(wait=True, cancel_futures=True)
        progress.done()


class _BatchDownloadCancelled(BaseException):
    """Raised inside a download job to abandon it after Ctrl-C.

    BaseException, like KeyboardInterrupt: a broad ``except Exception`` in
    the download layers must not convert an abort into a retry.
    """


class _BatchDownloadProgress:
    """One bar across several concurrent downloads, summing tracker bytes.

    The lock also serialises stderr writes so workers never interleave
    frames; a ``total`` of 0 draws nothing. Call ``done()`` at the end so a
    bar short of 100% still ends its line.
    """

    def __init__(self, header: str, total: int) -> None:
        self._bar = ProgressBar(header) if total > 0 else None
        self._total = total
        self._sum = 0
        self._lock = threading.Lock()

    def tracker(self) -> Callable[[int], None]:
        if self._bar is None:
            return lambda _: None
        last = 0

        def update(done: int) -> None:
            nonlocal last
            with self._lock:
                self._sum += done - last
                last = done
                # A bar-write failure (broken stderr pipe) must not surface
                # as a download failure and cost the .part file
                with suppress(Exception):
                    self._bar.update(min(self._sum / self._total, 1))

        return update

    def done(self) -> None:
        if self._bar is not None:
            self._bar.done()

    @contextmanager
    def logging_guard(self) -> Iterator[None]:
        r"""End a partial bar row before any log record while active.

        Worker warnings (mirror retries) share stderr with the bar's \r
        frames; without this the record lands mid-row and the next frame
        overwrites it. A handler-level filter runs just before emit, so
        only a tiny window remains for a concurrent frame.
        """
        the_bar = self._bar
        if the_bar is None:
            yield
            return
        lock = self._lock

        class _EndRow(logging.Filter):
            def filter(self, record: logging.LogRecord) -> bool:
                # Handler.handle() runs filters outside handleError's try; a
                # stderr write failure must not escape through the log call
                with lock, suppress(Exception):
                    the_bar.interrupt()
                return True

        end_row = _EndRow()
        handlers = logging.getLogger().handlers
        for handler in handlers:
            handler.addFilter(end_row)
        try:
            yield
        finally:
            for handler in handlers:
                handler.removeFilter(end_row)


def _part_path(dest: Path) -> Path:
    """The in-progress sidecar ``download_with_resume`` streams into."""
    return dest.with_name(dest.name + ".part")


def _cancellable_sleep(
    delay: float, progress: Callable[[int], None] | None, done: int
) -> None:
    """Backoff sleep that still observes a batch cancellation tick."""
    if progress is None:
        time.sleep(delay)
        return
    end = time.monotonic() + delay
    while (remaining := end - time.monotonic()) > 0:
        progress(done)  # raises when the batch was cancelled
        time.sleep(min(0.5, remaining))


def download_with_resume(
    url: str,
    dest: PathType,
    sha256: str | None = None,
    size: int | None = None,
    # More attempts than _MIRROR_ATTEMPTS: a single-URL download has no
    # mirror fallback, and each retry only re-fetches the remainder.
    attempts: int = 5,
    timeout: int = 30,
    retry_connect_errors: bool = True,
    progress: Callable[[int], None] | None = None,
) -> None:
    """Download ``url`` to ``dest``, resuming partial downloads.

    The body streams into ``<dest>.part``, which persists across attempts and
    esphome runs: a mid-stream connection drop only costs one attempt and the
    next continues from where it stopped, so an unstable connection converges
    on a complete file instead of restarting from zero each retry (#17703).
    When ``size`` / ``sha256`` are given the completed file is verified and a
    mismatch restarts from scratch; success renames the part file into place.
    An already-present ``dest`` that passes verification is kept as-is.

    Resuming a part file from an earlier run needs proof the content is
    unchanged: ``sha256`` when the caller has one, or otherwise the server's
    If-Range validator recorded in a ``<dest>.part.meta`` sidecar by the run
    that started the download — a size alone cannot detect a same-length
    content change on the server.

    With ``retry_connect_errors`` disabled, a failure before any body bytes
    flow (connect error, HTTP error status) propagates immediately instead
    of consuming attempts — for callers with their own fallback, like
    ``download_from_mirrors``.

    ``progress`` replaces the built-in bar: it receives the absolute bytes of
    ``dest`` obtained so far (see ``_BatchDownloadProgress``).

    Raises EsphomeError when all attempts are exhausted.
    """
    # Imported lazily: requests is a heavy import (~85ms) and is only needed
    # when actually downloading a toolchain, never during config validation.
    import requests

    from esphome.core import EsphomeError

    dest = Path(dest)
    part = _part_path(dest)
    meta = part.with_name(part.name + ".meta")
    dest.parent.mkdir(parents=True, exist_ok=True)
    last_error: Exception | None = None

    # An earlier run already completed this download. Only trust it when
    # there is something to verify it against; without sha/size the remote
    # content may have changed (e.g. a refreshed constraints file), so
    # re-download and atomically replace it.
    if dest.is_file() and (sha256 is not None or size is not None):
        try:
            _verify_file(dest, sha256, size)
            if progress is not None:
                progress(size if size is not None else dest.stat().st_size)
            return
        except EsphomeError:
            dest.unlink()

    # Adopt the validator/total the run that started this part file recorded,
    # so an unfinished download resumes across runs even without a sha256.
    validator, expected_total = _load_download_meta(meta, url)

    for _ in range(attempts):
        streamed = False
        try:
            offset = part.stat().st_size if part.is_file() else 0
            # A stitched resume needs two proofs: content identity (the
            # bytes being appended belong to the same file as the prefix)
            # and completeness. sha256 provides both, across runs. Without
            # it, identity needs this run's If-Range validator — a size
            # alone cannot detect a same-length content change, so a
            # leftover part file from an earlier run must restart — and
            # completeness needs a known total length.
            if (
                offset
                and sha256 is None
                and (validator is None or not (size or expected_total))
            ):
                _LOGGER.debug(
                    "Restarting %s from zero: cannot prove a resumed "
                    "file correct (no sha256, validator=%s, total=%s)",
                    url,
                    validator is not None,
                    size or expected_total,
                )
                offset = 0
            if size is None or offset < size:
                resp, offset = _open_ranged(url, offset, timeout, validator)
                # A None response means HTTP 416: the part file already holds
                # every byte the server has; fall through to verification.
                if resp is not None:
                    with resp, part.open("ab") as f:
                        streamed = True
                        if offset == 0:
                            validator = _response_validator(resp)
                            expected_total = _content_length(resp)
                            # Recorded so a later run can prove an If-Range
                            # resume of this part file safe.
                            _write_download_meta(meta, url, validator, expected_total)
                        _stream_response_to_file(resp, f, offset, size, progress)
            # else: a previous run already wrote every byte (or more) but
            # was killed before the rename below. Skip the network entirely
            # — a Range request past EOF would draw HTTP 416 — and let
            # verification decide whether to promote the file or discard it
            # and start over.

            expected_size = size if size is not None else expected_total
            _verify_file(part, sha256, expected_size or None)
            if progress is not None:
                # Also credits a part file an earlier run completed without
                # streaming anything this time.
                progress(expected_size or part.stat().st_size)
            if not expected_size and sha256 is None:
                # No sha, no size, and the server sent no usable
                # content-length: nothing can prove the download complete
                # (urllib3 still errors on most short bodies, but not on a
                # cleanly closed chunked stream). Promote with a debug
                # note rather than fail or warn: some servers (e.g. the
                # Espressif constraints host) never send a length, the user
                # can do nothing about it, and every current caller
                # extracts or parses the file afterwards, where corruption
                # fails loudly.
                _LOGGER.debug(
                    "Downloaded %s without any way to verify completeness",
                    dest.name,
                )
            # Retry on Windows sharing violations: an antivirus handle on the
            # freshly-written file must not get the verified download deleted
            # as corrupt by the except clause below. If even the backoff
            # retries fail, keep the verified part so the next attempt (or
            # run) only has to redo the rename, not the download.
            try:
                _rename_with_retry(part, dest, overwrite=True)
            except PermissionError as e:
                _LOGGER.debug("Could not move %s into place: %s", part, e)
                last_error = e
                continue
            meta.unlink(missing_ok=True)
            return
        except requests.RequestException as e:
            # Network failures — including connect errors, since a single
            # URL has no mirror-list fallback — keep the part file for the
            # next attempt (or the next esphome run) to resume from. Checked
            # before OSError: RequestException subclasses IOError.
            if not retry_connect_errors and not streamed:
                # The caller falls back to another URL on pre-body failures.
                raise
            _LOGGER.debug("Download of %s interrupted: %s", url, e)
            last_error = e
        except (OSError, EsphomeError) as e:
            # A completed-but-corrupt file (or local disk error) can't be
            # trusted for resume; start over.
            _LOGGER.debug("Discarding %s: %s", part, e)
            part.unlink(missing_ok=True)
            meta.unlink(missing_ok=True)
            last_error = e

    raise EsphomeError(
        f"Failed to download {url} after {attempts} attempts: "
        f"{failure_reason(last_error)}"
    ) from last_error


def failure_reason(e: BaseException) -> str:
    """Format a download exception for the aggregated error message.

    ``requests`` appends " for url: <url>" to HTTP errors; the URL is already
    printed on the line above, so strip the suffix to keep lines short. Falls
    back to the repr for exceptions with no message (e.g. ``TimeoutError()``)
    so the line always names the failure.
    """
    return str(e).split(" for url: ", maxsplit=1)[0] or repr(e)


def _try_mirrors_once(
    urls: list[str],
    path_target: Path,
    timeout: int,
    failures: list[tuple[str, Exception]],
    progress: Callable[[int], None] | None = None,
) -> str | None:
    """Single pass over the resolved mirror ``urls``, one try per URL.

    Returns the source URL on success, or None with each URL's exception
    appended to ``failures``.
    """
    # Imported lazily: requests is a heavy import (~85ms) and is only
    # needed when actually downloading, never during config validation.
    import requests

    from esphome.core import EsphomeError

    for url in urls:
        _LOGGER.debug("Trying to download from %s", url)

        # Delegate to download_with_resume so a partial download persists
        # (and resumes) across esphome runs.
        try:
            download_with_resume(
                url,
                path_target,
                attempts=_MIRROR_ATTEMPTS,
                timeout=timeout,
                # Pre-body failures (connect/HTTP errors) fall to the
                # next mirror immediately; only mid-stream drops
                # retry-with-resume on the same URL.
                retry_connect_errors=False,
                progress=progress,
            )
            return url
        except (requests.RequestException, OSError, EsphomeError) as e:
            # Everything download_with_resume classifies as a download
            # failure; programming errors propagate.
            _LOGGER.debug("Failed to download %s: %s", url, str(e))
            failures.append((url, e))

    return None


def download_and_extract(
    mirrors: list[str],
    substitutions: dict[str, str],
    archive_path: PathType,
    extract_dir: PathType,
    timeout: int = 30,
    progress_header: str | None = None,
    progress: Callable[[int], None] | None = None,
) -> str:
    """Download an archive from ``mirrors`` to ``archive_path``, extract it
    into ``extract_dir``, and delete the archive.

    The archive should live next to its destination (not in a temp dir) so
    an interrupted download's ``.part`` file resumes on the next run. The
    archive is deleted whether extraction succeeds or fails: a
    complete-but-corrupt file (e.g. torn by an unclean shutdown) must not
    poison the next run, and without a checksum only a failed extraction
    can expose it.

    Returns the source URL the download came from.
    """
    archive_path = Path(archive_path)
    url = download_from_mirrors(
        mirrors, substitutions, archive_path, timeout=timeout, progress=progress
    )
    try:
        archive_extract_all(archive_path, extract_dir, progress_header=progress_header)
    finally:
        # Best-effort: an AV handle on the just-written archive (Windows)
        # must not replace the real extraction error or fail a successful
        # extraction. A surviving archive is harmless; download_with_resume
        # re-verifies or re-downloads it next run.
        try:
            archive_path.unlink(missing_ok=True)
        except OSError as err:
            _LOGGER.debug("Could not remove archive %s: %s", archive_path, err)
    return url


def download_from_mirrors(
    mirrors: list[str],
    substitutions: dict[str, str],
    target: PathType,
    timeout: int = 30,
    progress: Callable[[int], None] | None = None,
) -> str:
    """
    Download file from multiple mirrors with substitution support.

    Args:
        mirrors: list of mirror URLs
        substitutions: Dictionary of substitutions to apply to URLs
        target: Target file path
        timeout: Download timeout in seconds
        progress: Passed through to the download (see ``download_with_resume``);
            replaces the built-in per-file bar

    Returns:
        The source URL.

    Mirror URL templates that reference a substitution not present in
    ``substitutions`` are skipped, so callers can offer templates that only
    apply to some downloads.

    The target downloads through ``download_with_resume``, so an
    interrupted download resumes on the next esphome run.

    When every mirror fails and at least one failure is transient (dropped
    connection, timeout, HTTP 429/5xx), the whole list is retried with a
    short backoff; permanent failures (e.g. 404) raise immediately.

    Raises:
        ValueError: If mirrors list is empty.
        EsphomeError: If all download attempts fail; the message lists every
            attempted URL with its individual failure reason. Also raised if
            no template matched the provided substitutions.
    """
    from esphome.core import EsphomeError

    if not isinstance(target, (str, os.PathLike)):
        raise TypeError(f"target must be a str or Path: {type(target)}")
    path_target = Path(target)

    # 1. Resolve the mirror templates (invariant across retry sweeps)
    urls: list[str] = []
    skipped: list[tuple[str, str]] = []
    for mirror in mirrors:
        try:
            urls.append(mirror.format(**substitutions))
        except KeyError as e:
            # The template references a substitution not provided for
            # this download (e.g. SHORT_VERSION only exists for x.y.0
            # versions) - expected, the template just doesn't apply.
            _LOGGER.debug("Skipping mirror %s: %s not available", mirror, e)
            skipped.append((mirror, f"not applicable ({e.args[0]} not available)"))
        except (IndexError, ValueError) as e:
            # A malformed template (unbalanced braces, bad format spec)
            # is an authoring error, not an expected fallthrough - warn
            # even if a later mirror succeeds.
            _LOGGER.warning("Skipping malformed mirror URL template %s: %r", mirror, e)
            skipped.append((mirror, f"skipped ({e!r})"))

    # 2. Sweep the mirror list, retrying transient failures with backoff:
    # a single pass keeps mirror failover fast, re-sweeping keeps one
    # network blip from failing the build when only one mirror applies.
    failures: list[tuple[str, Exception]] = []
    for sweep in range(1, _MIRROR_SWEEP_ATTEMPTS + 1):
        sweep_failures: list[tuple[str, Exception]] = []
        if (
            url := _try_mirrors_once(
                urls, path_target, timeout, sweep_failures, progress
            )
        ) is not None:
            return url
        failures.extend(sweep_failures)
        # Permanent failures (404, verification mismatch) won't heal;
        # only retry when a transient error is in the mix (as git.py does).
        transient = next(
            ((u, e) for u, e in sweep_failures if is_transient_download_error(e)),
            None,
        )
        if transient is None:
            break
        if sweep < _MIRROR_SWEEP_ATTEMPTS:
            delay = 2**sweep
            _LOGGER.warning(
                "Download of %s failed (%s); retrying in %d seconds (attempt %d/%d)",
                transient[0],
                failure_reason(transient[1]),
                delay,
                sweep + 1,
                _MIRROR_SWEEP_ATTEMPTS,
            )
            # Tick with the bytes already on disk so a combined bar holds
            # steady during the backoff instead of rewinding to zero
            done = 0
            if progress is not None:
                part = _part_path(path_target)
                done = part.stat().st_size if part.is_file() else 0
            _cancellable_sleep(delay, progress, done)

    # 3. Report every attempted URL if all mirrors failed. failures spans
    # all sweeps (deduplicated by URL and reason), so neither an early
    # mirror's failure nor an earlier sweep's failure mode is hidden.
    if failures:
        seen: set[tuple[str, str]] = set()
        attempts = ""
        for url, e in failures:
            reason = failure_reason(e)
            if (url, reason) not in seen:
                seen.add((url, reason))
                attempts += f"\n  {url}\n    {reason}"
        attempts += "".join(f"\n  {mirror}\n    {reason}" for mirror, reason in skipped)
        raise EsphomeError(
            f"Failed to download from all mirrors:{attempts}"
        ) from failures[0][1]
    if skipped:
        details = "".join(f"\n  {mirror}\n    {reason}" for mirror, reason in skipped)
        raise EsphomeError(
            f"No mirror URL template matched the provided substitutions:{details}"
        )
    raise ValueError("download_from_mirrors called with an empty mirrors list")
