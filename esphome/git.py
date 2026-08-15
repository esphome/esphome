from collections.abc import Callable, Iterator
from contextlib import contextmanager
from dataclasses import dataclass
from enum import Enum, auto
import errno
import hashlib
import logging
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import TYPE_CHECKING
import urllib.parse

import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.helpers import (
    add_git_ceiling_directory,
    format_duration,
    rmtree,
    write_file,
)

if TYPE_CHECKING:
    from filelock import FileLock

_LOGGER = logging.getLogger(__name__)

# Special value to indicate never refresh
NEVER_REFRESH = TimePeriodSeconds(seconds=-1)

# `refresh: never` validates to a huge interval rather than the NEVER_REFRESH
# sentinel; treat any interval at least that long as refresh disabled instead
# of logging a countdown of hundreds of years
_REFRESH_DISABLED_SECONDS = cv.source_refresh(cv.SOURCE_REFRESH_NEVER).total_seconds

# revert() runs on an already-failing path; bound its wait for the cache
# entry lock so that recovery cannot hang forever behind another process.
_REVERT_LOCK_TIMEOUT_SECONDS = 60

# When a complete cache entry already exists, a caller does not wait forever
# behind another process's stalled clone or update (git sets no network
# timeouts): after this bound it uses the existing clone without refreshing
# it. With no complete entry there is nothing to fall back to, so the wait
# is unbounded.
_COMPLETE_ENTRY_LOCK_TIMEOUT_SECONDS = 60

# Written inside .git only while the entry is a complete, quiescent
# checkout: after every clone step (clone, ref fetch, reset, submodule init)
# has finished, and removed for the duration of a refresh's rewrite
# (stash/fetch/reset). A directory without it is an interrupted clone or
# update (e.g. the process was killed mid-clone) and must be re-cloned;
# without this check such a directory would be trusted forever when the
# caller uses NEVER_REFRESH, and the bounded-wait fallback would hand a
# mid-rewrite tree to a timed-out peer. Lives in .git so
# stash/reset/checkout can never touch it and it does not pollute the
# worktree.
_CLONE_COMPLETE_MARKER = "esphome_clone_complete"

# Environment variables that scope git to a specific repository. Git hooks and
# some CI wrappers export these; if they leak into the git commands run here,
# git binds to the caller's repository instead of the one being managed. The
# effects range from loud (`git clone` producing a bare-style directory with
# no working tree) to silent (an ambient GIT_INDEX_FILE makes
# `git submodule update --init` exit 0 without initializing anything).
_GIT_REPO_SCOPING_ENV = frozenset(
    {
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_COMMON_DIR",
        "GIT_NAMESPACE",
    }
)

# Substrings (matched case-insensitively against git's full stderr) that
# identify transient network failures worth retrying. Auth failures,
# missing repositories, and bad refs must fail immediately. Patterns are
# phrase-anchored so a repository URL quoted back in stderr never matches.
_TRANSIENT_GIT_ERROR_PATTERNS: tuple[str, ...] = (
    "unable to access",
    "could not resolve host",
    "could not connect",
    "failed to connect",
    "timed out",
    "connection reset",
    "connection refused",
    "early eof",
    "rpc failed",
    "certificate verification failed",
    # Anchored to curl's diagnostic prefix so repository URLs containing
    # "ssl_" tokens never classify as transient
    "openssl ssl_",
    "ssl routines",
    "ssl connect error",
    "gnutls recv error",
    "gnutls_handshake",
    "unexpected disconnect",
    "remote end hung up unexpectedly",
)

# git quotes HTTP failures in two forms: curl's "The requested URL returned
# error: <n>" and smart-HTTP's "RPC failed; HTTP <n> curl <m>". 4xx is
# permanent (rejected credentials, missing repository) except 429 rate
# limiting; 408/425 are also treated as permanent, a deliberate trade for a
# simple rule since git hosts rarely emit them.
_PERMANENT_HTTP_ERROR_RE = re.compile(r"(?:http |returned error: )4(?!29)\d\d")

# Network commands get 3 attempts with 2s/4s backoff. Worst case is ~3x
# the command's own duration plus 6s of sleep, held under the cache entry
# lock; peers with a complete entry fall back to it after
# _COMPLETE_ENTRY_LOCK_TIMEOUT_SECONDS.
_NETWORK_MAX_ATTEMPTS = 3


class GitException(cv.Invalid):
    """Base exception for git-related errors."""


class GitNotInstalledError(GitException):
    """Exception raised when git is not installed on the system."""


class GitCommandError(GitException):
    """Exception raised when a git command fails.

    ``stderr`` holds git's full stderr output; the exception message is
    usually only the last ``fatal:`` line, but transient network markers
    (``RPC failed``, ``GnuTLS``, ...) often appear on earlier lines.
    Empty when git produced no stderr, so classification never reads the
    command line (which embeds the user-supplied repository URL).
    """

    def __init__(self, message: str, stderr: str = "") -> None:
        super().__init__(message)
        self.stderr = stderr


class GitRepositoryError(GitException):
    """Exception raised when a git repository is in an invalid state."""


def _redact_url_credentials(text: str) -> str:
    """Mask userinfo in any URLs embedded in ``text``.

    Users can put credentials directly in a git URL, and log output is
    routinely pasted into public issues.
    """
    return re.sub(r"://[^/@\s]+@", "://***@", text)


def _is_transient_git_error(stderr: str) -> bool:
    """Return True when git's stderr looks like a transient network failure."""
    lowered = stderr.lower()
    if _PERMANENT_HTTP_ERROR_RE.search(lowered):
        return False
    if "authentication failed" in lowered:
        return False
    return any(pattern in lowered for pattern in _TRANSIENT_GIT_ERROR_PATTERNS)


def run_git_command(
    cmd: list[str],
    git_dir: Path | None = None,
    *,
    cwd: Path | None = None,
    network: bool = False,
    retry_cleanup: Path | None = None,
) -> str:
    """Run a git command and return its stdout.

    The repository-scoping environment variables in ``_GIT_REPO_SCOPING_ENV``
    are always stripped. ``git_dir`` additionally pins GIT_DIR/GIT_WORK_TREE
    to that repository and runs the command there; ``cwd`` alone runs the
    command in that directory with GIT_CEILING_DIRECTORIES capping repository
    discovery at its parent.

    ``network=True`` marks a command that talks to a remote (clone, fetch,
    submodule update): transient network failures (DNS, TLS, dropped
    connections) are retried with a short backoff so a momentary blip does
    not fail the whole build. Local-only commands must not set it.
    ``retry_cleanup`` names a directory to remove before each retry, for
    commands like clone that can leave a partial destination behind.
    """
    attempts = _NETWORK_MAX_ATTEMPTS if network else 1
    attempt = 0
    while True:
        try:
            return _run_git_command_once(cmd, git_dir, cwd=cwd)
        except GitCommandError as err:
            attempt += 1
            if attempt >= attempts or not _is_transient_git_error(err.stderr):
                raise
            if retry_cleanup is not None and retry_cleanup.is_dir():
                try:
                    rmtree(retry_cleanup)
                except OSError as cleanup_err:
                    # A retry would fail on the leftover directory anyway;
                    # give up and keep the git error as the reported cause.
                    _LOGGER.warning(
                        "Could not remove %s before retry (%s); not retrying",
                        retry_cleanup,
                        cleanup_err,
                    )
                    raise err from None
            delay = 2**attempt
            _LOGGER.warning(
                "Git command failed: %s. Retrying in %d seconds... (attempt %d/%d)",
                _redact_url_credentials(str(err)),
                delay,
                attempt,
                attempts,
            )
            time.sleep(delay)


def _run_git_command_once(
    cmd: list[str], git_dir: Path | None = None, *, cwd: Path | None = None
) -> str:
    """Single attempt of ``run_git_command``; see its docstring."""
    # Every invocation starts from an environment with the repository-scoping
    # variables stripped (see _GIT_REPO_SCOPING_ENV) so a git hook or CI
    # wrapper invoking ESPHome can never redirect these commands to its own
    # repository or index.
    #
    # ``git_dir`` then re-adds GIT_DIR and GIT_WORK_TREE pointing at the
    # managed repository. This prevents git from walking up the directory
    # tree to find parent repositories when the target repo's .git directory
    # is corrupt. Without this, commands like 'git stash' could accidentally
    # operate on parent repositories (e.g., the main ESPHome repo) instead of
    # failing, causing data loss.
    #
    # ``cwd`` (without ``git_dir``) runs the command in that directory
    # without GIT_DIR/GIT_WORK_TREE. The ``git submodule`` porcelain needs
    # this: on some installations (e.g. Windows setups where a shim hands
    # git untranslated paths) it refuses to run when GIT_DIR/GIT_WORK_TREE
    # are set, failing with "cannot be used without a working tree".
    # GIT_CEILING_DIRECTORIES (which git only honors as an absolute path)
    # keeps the parent-repo-walk protection instead: if the repo's .git is
    # missing or corrupt, git fails rather than discovering an enclosing
    # repository.
    env = {k: v for k, v in os.environ.items() if k not in _GIT_REPO_SCOPING_ENV}
    if git_dir is not None:
        env["GIT_DIR"] = str(Path(git_dir) / ".git")
        env["GIT_WORK_TREE"] = str(git_dir)
        cwd = git_dir
    elif cwd is not None:
        add_git_ceiling_directory(env, Path(cwd).absolute().parent)

    _LOGGER.debug(
        "Running git command: %s (cwd=%s, isolated=%s)",
        _redact_url_credentials(" ".join(cmd)),
        cwd,
        git_dir is not None,
    )

    try:
        ret = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            check=False,
            close_fds=False,
            env=env,
        )
    except FileNotFoundError as err:
        raise GitNotInstalledError(
            "git is not installed. See "
            "https://git-scm.com/book/en/v2/Getting-Started-Installing-Git "
            "for installation instructions."
        ) from err

    if ret.returncode != 0:
        if ret.stderr:
            # errors="replace": git can emit locale-encoded (non-UTF-8) bytes
            # in stderr; the error path must never raise UnicodeDecodeError.
            err_str = ret.stderr.decode("utf-8", errors="replace")
            lines = [x.strip() for x in err_str.splitlines()]
            if lines[-1].startswith("fatal:"):
                raise GitCommandError(lines[-1][len("fatal: ") :], stderr=err_str)
            raise GitCommandError(err_str, stderr=err_str)
        # No stderr (e.g. git killed by a signal): nothing to classify,
        # never retried.
        raise GitCommandError(
            f"git exited with code {ret.returncode}: "
            f"{_redact_url_credentials(' '.join(cmd))}"
        )

    return ret.stdout.decode("utf-8").strip()


def _cache_key(url: str, ref: str | None) -> str:
    """Cache key identifying one repository checkout.

    The lock path and the entry directory both hash this, keeping them in
    agreement. (micro_wake_word still rebuilds the format by hand to locate
    manifests; fold it in here if the format ever changes.)
    """
    return f"{url}@{ref}"


def _compute_destination_path(key: str, domain: str) -> Path:
    base_dir = Path(CORE.data_dir) / domain
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def _repo_entry_dir(key: str, domain: str, subpath: Path | None) -> Path:
    """Worktree directory of one cache entry: the hash dir plus optional subpath."""
    repo_dir = _compute_destination_path(key, domain)
    if subpath:
        repo_dir = repo_dir / subpath
    return repo_dir


def _repo_lock_path(key: str, domain: str) -> Path:
    """Path of the lock file serializing all work on one cache entry.

    Lives next to the hash directory, never inside it, so the removal of a
    broken or incomplete clone can never delete a lock another process holds.
    """
    repo_dir = _compute_destination_path(key, domain)
    return repo_dir.parent / f"{repo_dir.name}.lock"


class _LockStatus(Enum):
    ACQUIRED = auto()
    # A bounded wait expired while another process held the lock.
    TIMEOUT = auto()
    # The lock could not be taken at all; callers proceed unlocked,
    # matching the behavior before the lock existed.
    UNAVAILABLE = auto()


# Errnos that mean the filesystem genuinely cannot take file locks (NFS
# without a lock daemon, some FUSE mounts). Any other OSError (permissions,
# read-only volume, full disk) is a cache directory problem, which the git
# commands themselves report clearly when it actually matters. EPERM is
# deliberately absent: it usually means a permissions problem, so it takes
# the generic message that names no cause. On Linux ENOTSUP and EOPNOTSUPP
# are the same value; the set folds them.
_NO_LOCK_SUPPORT_ERRNOS = frozenset(
    {errno.ENOLCK, errno.ENOSYS, errno.EOPNOTSUPP, errno.ENOTSUP}
)


def _acquire_repo_lock(
    lock: "FileLock",
    safe_key: str,
    timeout: float,
    wait_message: str = "Waiting for another process to finish updating %s",
) -> _LockStatus:
    """Acquire ``lock``, logging ``wait_message`` when a wait actually begins.

    ``timeout`` of -1 waits forever; a positive value bounds the wait and
    can yield ``TIMEOUT``.
    """
    from filelock import Timeout

    try:
        try:
            lock.acquire(blocking=False)
        except Timeout:
            # Waiting on another process's clone or update can take
            # minutes; say so instead of appearing hung.
            _LOGGER.info(wait_message, safe_key)
            lock.acquire(timeout=timeout)
    except Timeout:
        return _LockStatus.TIMEOUT
    except OSError as err:
        if err.errno in _NO_LOCK_SUPPORT_ERRNOS:
            _LOGGER.warning(
                "The filesystem does not support locking the cache entry for "
                "%s (%s), continuing without a lock",
                safe_key,
                err,
            )
        else:
            # Not a locking problem (permissions, read-only volume, full
            # disk). Still continue unlocked: a pre-seeded read-only cache
            # with refresh disabled only reads and must keep working, and
            # in every other case the git commands fail with the real error.
            _LOGGER.warning(
                "Could not take the cache entry lock for %s (%s), "
                "continuing without a lock",
                safe_key,
                err,
            )
        return _LockStatus.UNAVAILABLE
    return _LockStatus.ACQUIRED


@contextmanager
def _repo_cache_lock(
    key: str, domain: str, repo_dir: Path
) -> Iterator[tuple[bool, "FileLock | None"]]:
    """Hold the cache entry lock for ``key`` over the with block.

    Yields ``(use_existing, lock)``. ``use_existing`` is True when the lock
    could not be acquired within the bounded wait but ``repo_dir`` is a
    complete cache entry; the caller should use it as-is and do nothing
    else. Otherwise ``lock`` is the held lock, released when the block
    exits, or ``None`` when the lock could not be taken at all and the
    caller proceeds unlocked.
    """
    # Lazy import: keeps filelock off the CLI startup import path.
    from filelock import FileLock

    safe_key = _redact_url_credentials(key)
    # acquire() creates the lock file's directory itself; git clone later
    # creates the hash directory next to it. fallback_to_soft would silently
    # downgrade ENOSYS to a SoftFileLock, whose stale existence marker from
    # another host on a shared cache could hang the unbounded wait forever;
    # routing it through the OSError handler runs unlocked instead.
    lock: FileLock | None = FileLock(
        str(_repo_lock_path(key, domain)), fallback_to_soft=False
    )
    status = _acquire_repo_lock(lock, safe_key, _COMPLETE_ENTRY_LOCK_TIMEOUT_SECONDS)
    if status is _LockStatus.TIMEOUT:
        if _clone_complete_marker_path(repo_dir).is_file():
            # Mutual exclusion matters most while no complete entry exists
            # (initial clone, recovery re-clone); with one on disk, reading
            # it beats hanging behind a stalled holder.
            _LOGGER.warning(
                "Timed out waiting for another process updating %s, proceeding "
                "with the existing clone, which that process may still be "
                "changing",
                safe_key,
            )
            yield True, None
            return
        # Nothing to fall back to; the holder is producing the clone this
        # caller needs.
        status = _acquire_repo_lock(
            lock,
            safe_key,
            timeout=-1,
            wait_message="Still waiting for the clone of %s, "
            "there is no existing clone to fall back on",
        )
    if status is not _LockStatus.ACQUIRED:
        lock = None
    try:
        yield False, lock
    finally:
        if lock is not None:
            lock.release()


def _clone_complete_marker_path(repo_dir: Path) -> Path:
    return repo_dir / ".git" / _CLONE_COMPLETE_MARKER


def _clear_clone_complete_marker(repo_dir: Path) -> None:
    """Best-effort removal of the completion marker.

    If the unlink fails (e.g. a file lock on Windows), the marker stays and
    the entry keeps its previous trust level; every consumer of the marker
    tolerates that.
    """
    try:
        _clone_complete_marker_path(repo_dir).unlink(missing_ok=True)
    except OSError as err:
        _LOGGER.debug("Could not delete clone completion marker: %s", err)


def _write_clone_complete_marker(
    repo_dir: Path, key: str, hash_dir_name: str, safe_key: str
) -> None:
    """Mark the entry as a complete, quiescent checkout.

    The key and hash dir name are recorded purely to make cache debugging
    easier. The marker is only a validity signal, so a failed write must not
    fail an otherwise complete clone or update: the only cost is a re-clone
    on the next run.
    """
    try:
        write_file(
            _clone_complete_marker_path(repo_dir),
            f"key={key}\nhash={hash_dir_name}\n",
        )
    except EsphomeError as err:
        _LOGGER.warning(
            "Could not write clone completion marker for %s: %s", safe_key, err
        )


def _remove_repo_dir(repo_dir: Path) -> None:
    """Remove a repo directory, deleting the completion marker first.

    Marker-first ordering guarantees an interrupted removal can never leave a
    marker behind next to a partially deleted worktree. The unlink is best
    effort: if it fails, rmtree below still gets the chance to remove the
    directory, marker included.
    """
    _clear_clone_complete_marker(repo_dir)
    if repo_dir.is_dir():
        rmtree(repo_dir)


def update_submodules(repo_dir: Path, key: str) -> None:
    """Initialize/update every submodule the repository declares, recursively,
    matching how PlatformIO clones libraries.

    Most repositories declare no submodules, so this does nothing when there
    is no ``.gitmodules`` file. Which submodules get populated is git's own
    policy (``update = none``, ``submodule.active``, sparse checkouts);
    git's exit code is the error signal.

    Runs with plain ``cwd`` rather than ``git_dir`` isolation, which the
    ``git submodule`` porcelain does not tolerate (see ``run_git_command``).
    """
    if not (repo_dir / ".gitmodules").is_file():
        return
    _LOGGER.info("Updating submodules for %s", _redact_url_credentials(key))
    run_git_command(
        ["git", "submodule", "update", "--init", "--recursive", "--depth=1"],
        cwd=repo_dir,
        network=True,
    )


def resolve_symlink_stub(repo_dir: Path, file_path: Path) -> Path | None:
    """Return the symlink target if ``file_path`` is a Windows-checked-out symlink stub.

    On Windows, when ``core.symlinks=false`` (the default unless the user has
    SeCreateSymbolicLinkPrivilege — i.e. Developer Mode or running elevated),
    git materializes files with tree mode ``120000`` as plain text files
    whose content is the literal symlink target path. Opening such a file
    yields the target path string instead of the target's content.

    If ``file_path`` is one of those stubs, return the resolved target Path
    inside ``repo_dir``. Otherwise return ``None`` and the caller should use
    ``file_path`` as-is.

    Designed to be called *only* when normal access has already produced an
    unexpected result (e.g. YAML parsed as a top-level scalar), so the
    per-file ``git ls-files`` subprocess cost is paid only on the failure
    path. Returns ``None`` on any error or check failure — it's purely a
    best-effort recovery, never raises.
    """
    # On non-Windows, git creates real symlinks; ordinary file access already
    # transparently follows them.
    if sys.platform != "win32":
        return None
    if file_path.is_symlink():
        return None
    if not file_path.is_file():
        return None

    try:
        rel = file_path.relative_to(repo_dir)
    except ValueError:
        return None

    try:
        # ``git ls-files -s <path>`` prints "<mode> <sha> <stage>\t<path>"
        # for that single entry, or empty if untracked.
        out = run_git_command(
            ["git", "ls-files", "-s", "--", rel.as_posix()],
            git_dir=repo_dir,
        )
    except GitException:
        return None

    parts = out.split()
    if not parts or parts[0] != "120000":
        return None

    # Stubs are short ASCII relative paths. Decode defensively, and only
    # strip the trailing newline git's checkout may append — preserving any
    # whitespace that could be part of a valid target name.
    try:
        raw = file_path.read_bytes()
    except OSError:
        return None
    try:
        target_str = raw.decode("utf-8").rstrip("\r\n")
    except UnicodeDecodeError:
        return None

    # ``Path()`` and ``Path.resolve()`` can raise on malformed inputs (e.g.
    # embedded NUL bytes from a hostile symlink blob, paths too long for the
    # OS, or temporary I/O errors). Catch broadly — this helper is purely a
    # best-effort recovery and must never raise.
    try:
        target_path = (file_path.parent / target_str).resolve()
        repo_root_resolved = repo_dir.resolve()
    except (OSError, ValueError, RuntimeError):
        return None

    # ``Path.resolve()`` follows ``..``; re-verify containment afterwards.
    try:
        target_path.relative_to(repo_root_resolved)
    except ValueError:
        _LOGGER.warning(
            "Refusing to follow symlink %s -> %s (escapes repository)",
            file_path,
            target_str,
        )
        return None

    if not target_path.is_file():
        return None

    return target_path


def clone_or_update(
    *,
    url: str,
    ref: str | None = None,
    refresh: TimePeriodSeconds | None,
    domain: str,
    username: str | None = None,
    password: str | None = None,
    init_submodules: bool = False,
    subpath: Path | None = None,
) -> tuple[Path, Callable[[], bool] | None]:
    """Clone a repository into the cache, or refresh an existing clone.

    All work runs under a per-cache-entry inter-process file lock, so
    concurrent resolutions of the same repository (two esphome processes, or
    a subprocess plus an in-process load) serialize instead of interleaving.
    Without the lock, ``repo_dir.is_dir()`` is true from the instant
    ``git clone`` creates the directory: a second caller could read a half
    populated worktree, or see the missing completion marker and delete the
    clone in progress out from under the first caller.

    The lock guards mutation of the cache entry only; it is released when
    this function returns, so a caller still reading the worktree can
    overlap a later refresh by another process. That residual window is
    narrow (the refresh interval is re-checked under the lock) and predates
    the lock.

    Locking is best effort: on a filesystem that cannot take file locks a
    warning is logged and the work proceeds unlocked, matching the behavior
    before the lock existed. A complete cache entry also caps the wait: if
    the holder is still busy after a bounded time (e.g. stalled on the
    network), the existing clone is used without refreshing it, so a stuck
    process cannot hang every peer that already has a good entry.
    """
    key = _cache_key(url, ref)
    repo_dir = _repo_entry_dir(key, domain, subpath)
    with _repo_cache_lock(key, domain, repo_dir) as (use_existing, lock):
        if use_existing:
            return repo_dir, None
        return _clone_or_update_locked(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
            username=username,
            password=password,
            init_submodules=init_submodules,
            subpath=subpath,
            lock=lock,
        )


def _clone_or_update_locked(
    *,
    url: str,
    ref: str | None,
    refresh: TimePeriodSeconds | None,
    domain: str,
    username: str | None,
    password: str | None,
    init_submodules: bool,
    subpath: Path | None,
    lock: "FileLock | None",
    _recover_broken: bool = True,
) -> tuple[Path, Callable[[], bool] | None]:
    """Body of ``clone_or_update``; the caller holds ``lock``.

    Split out because the broken-repository recovery below re-enters this
    function: re-acquiring the already-held lock would deadlock, since OS
    file locks taken on separate file descriptors conflict even within one
    process. ``lock`` is only re-acquired by the returned ``revert``
    callback, which runs after the wrapper's ``finally`` has released it.
    ``lock`` is ``None`` when the filesystem cannot take file locks and the
    wrapper fell back to running unlocked.
    """
    key = _cache_key(url, ref)
    # The user may have embedded credentials in the URL itself; log this
    # instead of key.
    safe_key = _redact_url_credentials(key)

    # Keep the caller's URL for the recovery re-clone below: rewriting the
    # rewritten URL would double the userinfo, and the recursive call must
    # compute the same cache key as this one.
    original_url = url
    if username is not None and password is not None:
        url = url.replace(
            "://", f"://{urllib.parse.quote(username)}:{urllib.parse.quote(password)}@"
        )

    hash_dir_name = _compute_destination_path(key, domain).name
    repo_dir = _repo_entry_dir(key, domain, subpath)

    if repo_dir.is_dir() and not _clone_complete_marker_path(repo_dir).is_file():
        # The last clone never finished (killed process, container stop) or
        # predates the marker; either way it cannot be trusted, especially
        # with NEVER_REFRESH where it would otherwise be reused forever.
        _LOGGER.warning(
            "Removing incomplete clone of %s at %s, will re-clone", safe_key, repo_dir
        )
        _remove_repo_dir(repo_dir)

    if not repo_dir.is_dir():
        _LOGGER.info("Cloning %s", safe_key)
        _LOGGER.debug("Location: %s", repo_dir)
        try:
            cmd = ["git", "clone", "--depth=1"]
            cmd += ["--", url, str(repo_dir)]
            run_git_command(cmd, network=True, retry_cleanup=repo_dir)

            if ref is not None:
                # We need to fetch the PR branch first, otherwise git will complain
                # about missing objects
                _LOGGER.info("Fetching %s", ref)
                run_git_command(
                    ["git", "fetch", "--depth=1", "--", "origin", ref],
                    git_dir=repo_dir,
                    network=True,
                )
                run_git_command(
                    ["git", "reset", "--hard", "FETCH_HEAD"], git_dir=repo_dir
                )

            if init_submodules:
                update_submodules(repo_dir, key)

        except GitException:
            # Remove incomplete clone to prevent stale state. Without this,
            # a failed ref fetch leaves a clone on the default branch, and
            # subsequent calls skip the update due to the refresh window.
            _remove_repo_dir(repo_dir)
            raise

        # Every git step succeeded.
        _write_clone_complete_marker(repo_dir, key, hash_dir_name, safe_key)

    else:
        if refresh == NEVER_REFRESH or CORE.skip_external_update:
            _LOGGER.debug("Skipping update for %s (refresh disabled)", safe_key)
            return repo_dir, None

        file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
        # On first clone, FETCH_HEAD does not exist
        if not file_timestamp.exists():
            file_timestamp = Path(repo_dir / ".git" / "HEAD")
        try:
            age_seconds = time.time() - file_timestamp.stat().st_mtime
        except OSError:
            # A .git with neither FETCH_HEAD nor HEAD is corrupt (e.g. a
            # partially deleted clone). Force the update path so the
            # broken-repository recovery below removes and re-clones it.
            age_seconds = float("inf")
        if refresh is None or age_seconds > refresh.total_seconds:
            # Try to update the repository, recovering from broken state if needed
            old_sha: str | None = None
            try:
                # First verify the repository is valid by checking HEAD
                # Use git_dir parameter to prevent git from walking up to parent repos
                old_sha = run_git_command(
                    ["git", "rev-parse", "HEAD"], git_dir=repo_dir
                )

                _LOGGER.info("Updating %s", safe_key)
                _LOGGER.debug("Location: %s", repo_dir)

                # The entry is about to be rewritten; drop the marker so a
                # timed-out peer's fallback and the incomplete-entry check
                # can tell a quiescent complete entry from one mid-rewrite,
                # and so an update interrupted by a crash re-clones instead
                # of being trusted.
                _clear_clone_complete_marker(repo_dir)

                # Stash local changes (if any)
                # Use git_dir to ensure this only affects the specific repo
                run_git_command(
                    ["git", "stash", "push", "--include-untracked"],
                    git_dir=repo_dir,
                )

                # Fetch from the remote. --depth=1 keeps the clone shallow
                # while still picking up new commits when the remote tip
                # moves: a shallow fetch retrieves the current tip being
                # fetched, whether that's an explicit ref or the remote's
                # default branch, then reset --hard FETCH_HEAD updates the
                # working tree to it.
                cmd = ["git", "fetch", "--depth=1", "--", "origin"]
                if ref is not None:
                    cmd.append(ref)
                fetch_head = Path(repo_dir) / ".git" / "FETCH_HEAD"
                try:
                    fetch_head_stat = fetch_head.stat()
                except OSError:
                    # Missing (or unreadable): no pre-fetch FETCH_HEAD
                    fetch_head_stat = None
                try:
                    run_git_command(cmd, git_dir=repo_dir, network=True)
                except GitCommandError as err:
                    if not _is_transient_git_error(err.stderr):
                        raise
                    # Verified clone, untouched worktree, network-only
                    # failure: keep the clone instead of destroying it via
                    # recovery, which would re-clone on the same dead
                    # network. The marker must be restored or the next run
                    # removes the entry as an incomplete clone.
                    #
                    # A failed fetch still freshens FETCH_HEAD's mtime,
                    # which would suppress refresh attempts for the whole
                    # refresh window; restore it so the next run retries.
                    try:
                        if fetch_head_stat is not None:
                            os.utime(
                                fetch_head,
                                (fetch_head_stat.st_atime, fetch_head_stat.st_mtime),
                            )
                        else:
                            fetch_head.unlink(missing_ok=True)
                    except OSError as stamp_err:
                        # Cannot keep the fallback honest; let the git error
                        # route through the recovery below instead.
                        _LOGGER.warning(
                            "Could not restore the refresh timestamp for %s (%s)",
                            safe_key,
                            stamp_err,
                        )
                        raise err from None
                    _LOGGER.warning(
                        "Could not refresh %s (%s); using the existing clone "
                        "at %s (last updated %s ago)",
                        safe_key,
                        _redact_url_credentials(str(err)),
                        old_sha,
                        # age_seconds is inf when neither FETCH_HEAD nor HEAD
                        # could be stat'ed; format_duration would overflow
                        format_duration(age_seconds)
                        if math.isfinite(age_seconds)
                        else "unknown time",
                    )
                    _write_clone_complete_marker(repo_dir, key, hash_dir_name, safe_key)
                    return repo_dir, None

                # Hard reset to FETCH_HEAD (short-lived git ref corresponding to most recent fetch)
                run_git_command(
                    ["git", "reset", "--hard", "FETCH_HEAD"],
                    git_dir=repo_dir,
                )

                # Inside the try so a submodule failure routes through the
                # recovery re-clone below instead of leaving a repo that the
                # refresh window would silently accept on the next run.
                if init_submodules:
                    update_submodules(repo_dir, key)

                # Recorded so revert() can tell whether the checkout is
                # still the one this update produced.
                new_sha = run_git_command(
                    ["git", "rev-parse", "HEAD"], git_dir=repo_dir
                )

                # The rewrite finished; the entry is trustworthy again.
                _write_clone_complete_marker(repo_dir, key, hash_dir_name, safe_key)
            except GitException as err:
                # Repository is in a broken state or update failed
                # Only attempt recovery once to prevent infinite recursion
                if not _recover_broken:
                    _LOGGER.error(
                        "Repository %s recovery failed, cannot retry (already attempted once)",
                        safe_key,
                    )
                    raise

                _LOGGER.warning(
                    "Repository %s has issues (%s), attempting recovery",
                    safe_key,
                    _redact_url_credentials(str(err)),
                )
                _LOGGER.info("Removing broken repository at %s", repo_dir)
                _remove_repo_dir(repo_dir)
                _LOGGER.info("Successfully removed broken repository, re-cloning...")

                # Re-clone while still holding the lock; going through the
                # public wrapper would try to re-acquire it and deadlock.
                # Set _recover_broken=False to prevent infinite recursion.
                result = _clone_or_update_locked(
                    url=original_url,
                    ref=ref,
                    refresh=refresh,
                    domain=domain,
                    username=username,
                    password=password,
                    init_submodules=init_submodules,
                    subpath=subpath,
                    lock=lock,
                    _recover_broken=False,
                )
                _LOGGER.info("Repository %s successfully recovered", safe_key)
                return result

            def revert() -> bool:
                """Reset the checkout to the pre-update SHA.

                Returns False when the revert did not happen: the cache
                entry lock could not be acquired in time, the checkout
                moved since this update (another process refreshed it), or
                the reset itself failed. A retry cannot reach the
                pre-update content then.
                """
                if lock is None:
                    # The wrapper already warned about the unlockable
                    # filesystem; revert unlocked like everything else.
                    status = _LockStatus.UNAVAILABLE
                else:
                    status = _acquire_repo_lock(
                        lock, safe_key, _REVERT_LOCK_TIMEOUT_SECONDS
                    )
                if status is _LockStatus.TIMEOUT:
                    # revert() only runs on an already-failing path; skip
                    # rather than hang so the original error can surface.
                    _LOGGER.warning(
                        "Could not lock %s to revert to %s, skipping revert; "
                        "the cached checkout keeps the un-reverted content "
                        "until its next refresh",
                        safe_key,
                        old_sha,
                    )
                    return False
                try:
                    # Anything can happen between the wrapper releasing the
                    # lock and revert() re-acquiring it; only undo this
                    # process's own update, never a peer's newer refresh.
                    head = run_git_command(
                        ["git", "rev-parse", "HEAD"], git_dir=repo_dir
                    )
                    if head != new_sha:
                        _LOGGER.warning(
                            "Not reverting %s: the checkout moved since this "
                            "update (another process refreshed it)",
                            safe_key,
                        )
                        return False
                    # Announced only once every skip check has passed, so
                    # the log says exactly one thing per outcome.
                    _LOGGER.info("Reverting changes to %s -> %s", safe_key, old_sha)
                    run_git_command(
                        ["git", "reset", "--hard", old_sha], git_dir=repo_dir
                    )
                except GitException as err:
                    # GitException is a cv.Invalid; letting it escape would
                    # replace the caller's original error with a bare git
                    # message. Report the failed reset like the skip above,
                    # and drop the marker: an entry whose reset fails cannot
                    # be trusted, so the next use re-clones it instead of
                    # the refresh window silently accepting it.
                    _LOGGER.warning(
                        "Could not revert %s to %s (%s), the entry will be "
                        "re-cloned on next use",
                        safe_key,
                        old_sha,
                        err,
                    )
                    _clear_clone_complete_marker(repo_dir)
                    return False
                finally:
                    if status is _LockStatus.ACQUIRED:
                        lock.release()
                return True

            return repo_dir, revert
        if refresh.total_seconds >= _REFRESH_DISABLED_SECONDS:
            # refresh: never
            _LOGGER.debug("Skipping update for %s (refresh disabled)", safe_key)
        else:
            _LOGGER.info(
                "Skipping update for %s, will refresh on the next run after %s "
                "(refresh: %s); use refresh: always to update now",
                safe_key,
                format_duration(refresh.total_seconds - age_seconds),
                format_duration(refresh.total_seconds),
            )

    return repo_dir, None


GIT_DOMAINS = {
    "codeberg": "codeberg.org",
    "github": "github.com",
    "gitlab": "gitlab.com",
}


@dataclass(frozen=True)
class GitFile:
    domain: str
    owner: str
    repo: str
    filename: str
    ref: str = None
    query: str = None

    @property
    def git_url(self) -> str:
        return f"https://{self.domain}/{self.owner}/{self.repo}.git"

    @property
    def raw_url(self) -> str:
        if self.ref is None:
            raise ValueError("URL has no ref")
        if self.domain == "codeberg.org":
            return f"https://codeberg.org/{self.owner}/{self.repo}/raw/commit/{self.ref}/{self.filename}"
        if self.domain == "github.com":
            return f"https://raw.githubusercontent.com/{self.owner}/{self.repo}/{self.ref}/{self.filename}"
        if self.domain == "gitlab.com":
            return f"https://gitlab.com/{self.owner}/{self.repo}/-/raw/{self.ref}/{self.filename}"
        raise NotImplementedError(f"Git domain {self.domain} not supported")

    @classmethod
    def from_shorthand(cls, shorthand):
        """Parse a git shorthand URL into its components."""
        if not isinstance(shorthand, str):
            raise ValueError("Git shorthand must be a string")
        m = re.match(
            r"(?P<domain>[a-zA-Z0-9\-]+)://(?P<owner>[a-zA-Z0-9\-]+)/(?P<repo>[a-zA-Z0-9\-\._]+)/(?P<filename>[a-zA-Z0-9\-_.\./]+)(?:@(?P<ref>[a-zA-Z0-9\-_.\./]+))?(?:\?(?P<query>[a-zA-Z0-9\-_.\./]+))?",
            shorthand,
        )
        if m is None:
            raise ValueError(
                "URL is not in expected github://username/name/[sub-folder/]file-path.yml[@branch-or-tag] format!"
            )
        if m.group("domain") not in GIT_DOMAINS:
            raise ValueError(f"Unknown git domain {m.group('domain')}")
        return cls(
            domain=GIT_DOMAINS[m.group("domain")],
            owner=m.group("owner"),
            repo=m.group("repo"),
            filename=m.group("filename"),
            ref=m.group("ref"),
            query=m.group("query"),
        )
