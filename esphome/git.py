from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime
import hashlib
import logging
from pathlib import Path
import re
import shutil
import subprocess
import urllib.parse

import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)

# Special value to indicate never refresh
NEVER_REFRESH = TimePeriodSeconds(seconds=-1)


class GitException(cv.Invalid):
    """Base exception for git-related errors."""


class GitNotInstalledError(GitException):
    """Exception raised when git is not installed on the system."""


class GitCommandError(GitException):
    """Exception raised when a git command fails."""


class GitRepositoryError(GitException):
    """Exception raised when a git repository is in an invalid state."""


def run_git_command(cmd: list[str], git_dir: Path | None = None) -> str:
    if git_dir is not None:
        _LOGGER.debug(
            "Running git command with repository isolation: %s (git_dir=%s)",
            " ".join(cmd),
            git_dir,
        )
    else:
        _LOGGER.debug("Running git command: %s", " ".join(cmd))

    # Set up environment for repository isolation if git_dir is provided
    # Force git to only operate on this specific repository by setting
    # GIT_DIR and GIT_WORK_TREE. This prevents git from walking up the
    # directory tree to find parent repositories when the target repo's
    # .git directory is corrupt. Without this, commands like 'git stash'
    # could accidentally operate on parent repositories (e.g., the main
    # ESPHome repo) instead of failing, causing data loss.
    env: dict[str, str] | None = None
    cwd: str | None = None
    if git_dir is not None:
        env = {
            **subprocess.os.environ,
            "GIT_DIR": str(Path(git_dir) / ".git"),
            "GIT_WORK_TREE": str(git_dir),
        }
        cwd = str(git_dir)

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
            "git is not installed but required for external_components.\n"
            "Please see https://git-scm.com/book/en/v2/Getting-Started-Installing-Git for installing git"
        ) from err

    if ret.returncode != 0 and ret.stderr:
        err_str = ret.stderr.decode("utf-8")
        lines = [x.strip() for x in err_str.splitlines()]
        if lines[-1].startswith("fatal:"):
            raise GitCommandError(lines[-1][len("fatal: ") :])
        raise GitCommandError(err_str)

    return ret.stdout.decode("utf-8").strip()


def _compute_destination_path(key: str, domain: str) -> Path:
    base_dir = Path(CORE.data_dir) / domain
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def clone_or_update(
    *,
    url: str,
    ref: str = None,
    refresh: TimePeriodSeconds | None,
    domain: str,
    username: str = None,
    password: str = None,
    submodules: list[str] | None = None,
    _recover_broken: bool = True,
) -> tuple[Path, Callable[[], None] | None]:
    key = f"{url}@{ref}"

    if username is not None and password is not None:
        url = url.replace(
            "://", f"://{urllib.parse.quote(username)}:{urllib.parse.quote(password)}@"
        )

    repo_dir = _compute_destination_path(key, domain)
    if not repo_dir.is_dir():
        _LOGGER.info("Cloning %s", key)
        _LOGGER.debug("Location: %s", repo_dir)
        cmd = ["git", "clone", "--depth=1"]
        cmd += ["--", url, str(repo_dir)]
        run_git_command(cmd)

        if ref is not None:
            # We need to fetch the PR branch first, otherwise git will complain
            # about missing objects
            _LOGGER.info("Fetching %s", ref)
            run_git_command(["git", "fetch", "--", "origin", ref], git_dir=repo_dir)
            run_git_command(["git", "reset", "--hard", "FETCH_HEAD"], git_dir=repo_dir)

        if submodules is not None:
            _LOGGER.info(
                "Initializing submodules (%s) for %s", ", ".join(submodules), key
            )
            run_git_command(
                ["git", "submodule", "update", "--init"] + submodules, git_dir=repo_dir
            )

    else:
        # Check refresh needed
        # Skip refresh if NEVER_REFRESH is specified
        if refresh == NEVER_REFRESH:
            _LOGGER.debug("Skipping update for %s (refresh disabled)", key)
            return repo_dir, None

        file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
        # On first clone, FETCH_HEAD does not exists
        if not file_timestamp.exists():
            file_timestamp = Path(repo_dir / ".git" / "HEAD")
        age = datetime.now() - datetime.fromtimestamp(file_timestamp.stat().st_mtime)
        if refresh is None or age.total_seconds() > refresh.total_seconds:
            # Try to update the repository, recovering from broken state if needed
            old_sha: str | None = None
            try:
                # First verify the repository is valid by checking HEAD
                # Use git_dir parameter to prevent git from walking up to parent repos
                old_sha = run_git_command(
                    ["git", "rev-parse", "HEAD"], git_dir=repo_dir
                )

                _LOGGER.info("Updating %s", key)
                _LOGGER.debug("Location: %s", repo_dir)

                # Stash local changes (if any)
                # Use git_dir to ensure this only affects the specific repo
                run_git_command(
                    ["git", "stash", "push", "--include-untracked"],
                    git_dir=repo_dir,
                )

                # Fetch remote ref
                cmd = ["git", "fetch", "--", "origin"]
                if ref is not None:
                    cmd.append(ref)
                run_git_command(cmd, git_dir=repo_dir)

                # Hard reset to FETCH_HEAD (short-lived git ref corresponding to most recent fetch)
                run_git_command(
                    ["git", "reset", "--hard", "FETCH_HEAD"],
                    git_dir=repo_dir,
                )
            except GitException as err:
                # Repository is in a broken state or update failed
                # Only attempt recovery once to prevent infinite recursion
                if not _recover_broken:
                    _LOGGER.error(
                        "Repository %s recovery failed, cannot retry (already attempted once)",
                        key,
                    )
                    raise

                _LOGGER.warning(
                    "Repository %s has issues (%s), attempting recovery",
                    key,
                    err,
                )
                _LOGGER.info("Removing broken repository at %s", repo_dir)
                shutil.rmtree(repo_dir)
                _LOGGER.info("Successfully removed broken repository, re-cloning...")

                # Recursively call clone_or_update to re-clone
                # Set _recover_broken=False to prevent infinite recursion
                result = clone_or_update(
                    url=url,
                    ref=ref,
                    refresh=refresh,
                    domain=domain,
                    username=username,
                    password=password,
                    submodules=submodules,
                    _recover_broken=False,
                )
                _LOGGER.info("Repository %s successfully recovered", key)
                return result

            if submodules is not None:
                _LOGGER.info(
                    "Updating submodules (%s) for %s", ", ".join(submodules), key
                )
                run_git_command(
                    ["git", "submodule", "update", "--init"] + submodules,
                    git_dir=repo_dir,
                )

            def revert():
                _LOGGER.info("Reverting changes to %s -> %s", key, old_sha)
                run_git_command(["git", "reset", "--hard", old_sha], git_dir=repo_dir)

            return repo_dir, revert

    return repo_dir, None


GIT_DOMAINS = {
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
