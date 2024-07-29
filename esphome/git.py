from dataclasses import dataclass
from datetime import datetime
import hashlib
import logging
from pathlib import Path
import re
import subprocess
from typing import Callable, Optional
import urllib.parse

import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)


def run_git_command(cmd, cwd=None) -> str:
    _LOGGER.debug("Running git command: %s", " ".join(cmd))
    try:
        ret = subprocess.run(cmd, cwd=cwd, capture_output=True, check=False)
    except FileNotFoundError as err:
        raise cv.Invalid(
            "git is not installed but required for external_components.\n"
            "Please see https://git-scm.com/book/en/v2/Getting-Started-Installing-Git for installing git"
        ) from err

    if ret.returncode != 0 and ret.stderr:
        err_str = ret.stderr.decode("utf-8")
        lines = [x.strip() for x in err_str.splitlines()]
        if lines[-1].startswith("fatal:"):
            raise cv.Invalid(lines[-1][len("fatal: ") :])
        raise cv.Invalid(err_str)

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
    refresh: Optional[TimePeriodSeconds],
    domain: str,
    username: str = None,
    password: str = None,
    submodules: Optional[list[str]] = None,
) -> tuple[Path, Optional[Callable[[], None]]]:
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
            run_git_command(["git", "fetch", "--", "origin", ref], str(repo_dir))
            run_git_command(["git", "reset", "--hard", "FETCH_HEAD"], str(repo_dir))

        if submodules is not None:
            _LOGGER.info(
                "Initialising submodules (%s) for %s", ", ".join(submodules), key
            )
            run_git_command(
                ["git", "submodule", "update", "--init"] + submodules, str(repo_dir)
            )

    else:
        # Check refresh needed
        file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
        # On first clone, FETCH_HEAD does not exists
        if not file_timestamp.exists():
            file_timestamp = Path(repo_dir / ".git" / "HEAD")
        age = datetime.now() - datetime.fromtimestamp(file_timestamp.stat().st_mtime)
        if refresh is None or age.total_seconds() > refresh.total_seconds:
            old_sha = run_git_command(["git", "rev-parse", "HEAD"], str(repo_dir))
            _LOGGER.info("Updating %s", key)
            _LOGGER.debug("Location: %s", repo_dir)
            # Stash local changes (if any)
            run_git_command(
                ["git", "stash", "push", "--include-untracked"], str(repo_dir)
            )
            # Fetch remote ref
            cmd = ["git", "fetch", "--", "origin"]
            if ref is not None:
                cmd.append(ref)
            run_git_command(cmd, str(repo_dir))
            # Hard reset to FETCH_HEAD (short-lived git ref corresponding to most recent fetch)
            run_git_command(["git", "reset", "--hard", "FETCH_HEAD"], str(repo_dir))

            if submodules is not None:
                _LOGGER.info(
                    "Updating submodules (%s) for %s", ", ".join(submodules), key
                )
                run_git_command(
                    ["git", "submodule", "update", "--init"] + submodules, str(repo_dir)
                )

            def revert():
                _LOGGER.info("Reverting changes to %s -> %s", key, old_sha)
                run_git_command(["git", "reset", "--hard", old_sha], str(repo_dir))

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
