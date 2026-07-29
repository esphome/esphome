from collections.abc import Callable
from dataclasses import dataclass
import hashlib
import logging
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import urllib.parse

import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.helpers import add_git_ceiling_directory, rmtree, write_file

_LOGGER = logging.getLogger(__name__)

# Special value to indicate never refresh
NEVER_REFRESH = TimePeriodSeconds(seconds=-1)

# Written inside .git only after every clone step (clone, ref fetch, reset,
# submodule init) has completed. A directory without it is an interrupted
# clone (e.g. the process was killed mid-clone) and must be re-cloned; without
# this check such a directory would be trusted forever when the caller uses
# NEVER_REFRESH. Lives in .git so stash/reset/checkout can never touch it and
# it does not pollute the worktree.
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


class GitException(cv.Invalid):
    """Base exception for git-related errors."""


class GitNotInstalledError(GitException):
    """Exception raised when git is not installed on the system."""


class GitCommandError(GitException):
    """Exception raised when a git command fails."""


class GitRepositoryError(GitException):
    """Exception raised when a git repository is in an invalid state."""


def _redact_url_credentials(text: str) -> str:
    """Mask userinfo in any URLs embedded in ``text``.

    Users can put credentials directly in a git URL, and log output is
    routinely pasted into public issues.
    """
    return re.sub(r"://[^/@\s]+@", "://***@", text)


def run_git_command(
    cmd: list[str], git_dir: Path | None = None, *, cwd: Path | None = None
) -> str:
    """Run a git command and return its stdout.

    The repository-scoping environment variables in ``_GIT_REPO_SCOPING_ENV``
    are always stripped. ``git_dir`` additionally pins GIT_DIR/GIT_WORK_TREE
    to that repository and runs the command there; ``cwd`` alone runs the
    command in that directory with GIT_CEILING_DIRECTORIES capping repository
    discovery at its parent.
    """
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
            err_str = ret.stderr.decode("utf-8")
            lines = [x.strip() for x in err_str.splitlines()]
            if lines[-1].startswith("fatal:"):
                raise GitCommandError(lines[-1][len("fatal: ") :])
            raise GitCommandError(err_str)
        raise GitCommandError(
            f"git exited with code {ret.returncode}: "
            f"{_redact_url_credentials(' '.join(cmd))}"
        )

    return ret.stdout.decode("utf-8").strip()


def _compute_destination_path(key: str, domain: str) -> Path:
    base_dir = Path(CORE.data_dir) / domain
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def _clone_complete_marker_path(repo_dir: Path) -> Path:
    return repo_dir / ".git" / _CLONE_COMPLETE_MARKER


def _remove_repo_dir(repo_dir: Path) -> None:
    """Remove a repo directory, deleting the completion marker first.

    Marker-first ordering guarantees an interrupted removal can never leave a
    marker behind next to a partially deleted worktree. The unlink is best
    effort: if it fails (e.g. a file lock on Windows), rmtree below still
    gets the chance to remove the directory, marker included.
    """
    try:
        _clone_complete_marker_path(repo_dir).unlink(missing_ok=True)
    except OSError as err:
        _LOGGER.debug("Could not delete clone completion marker first: %s", err)
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
    ref: str = None,
    refresh: TimePeriodSeconds | None,
    domain: str,
    username: str = None,
    password: str = None,
    init_submodules: bool = False,
    subpath: Path | None = None,
    _recover_broken: bool = True,
) -> tuple[Path, Callable[[], None] | None]:
    key = f"{url}@{ref}"
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

    repo_dir = _compute_destination_path(key, domain)
    hash_dir_name = repo_dir.name
    if subpath:
        repo_dir = repo_dir / subpath

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
            run_git_command(cmd)

            if ref is not None:
                # We need to fetch the PR branch first, otherwise git will complain
                # about missing objects
                _LOGGER.info("Fetching %s", ref)
                run_git_command(
                    ["git", "fetch", "--depth=1", "--", "origin", ref],
                    git_dir=repo_dir,
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

        # Every git step succeeded; the key and hash dir name are recorded
        # purely to make cache debugging easier. The marker is only a
        # validity signal, so a failed write must not fail an otherwise
        # complete clone: the only cost is a re-clone on the next run.
        try:
            write_file(
                _clone_complete_marker_path(repo_dir),
                f"key={key}\nhash={hash_dir_name}\n",
            )
        except EsphomeError as err:
            _LOGGER.warning(
                "Could not write clone completion marker for %s: %s", safe_key, err
            )

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
                run_git_command(cmd, git_dir=repo_dir)

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
                    err,
                )
                _LOGGER.info("Removing broken repository at %s", repo_dir)
                _remove_repo_dir(repo_dir)
                _LOGGER.info("Successfully removed broken repository, re-cloning...")

                # Recursively call clone_or_update to re-clone
                # Set _recover_broken=False to prevent infinite recursion
                result = clone_or_update(
                    url=original_url,
                    ref=ref,
                    refresh=refresh,
                    domain=domain,
                    username=username,
                    password=password,
                    init_submodules=init_submodules,
                    subpath=subpath,
                    _recover_broken=False,
                )
                _LOGGER.info("Repository %s successfully recovered", safe_key)
                return result

            def revert():
                _LOGGER.info("Reverting changes to %s -> %s", safe_key, old_sha)
                run_git_command(["git", "reset", "--hard", old_sha], git_dir=repo_dir)

            return repo_dir, revert

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
