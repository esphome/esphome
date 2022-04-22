from pathlib import Path
import subprocess
import hashlib
import logging
import urllib.parse

from datetime import datetime

from esphome.core import CORE, TimePeriodSeconds
import esphome.config_validation as cv

_LOGGER = logging.getLogger(__name__)


def run_git_command(cmd, cwd=None):
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


def _compute_destination_path(key: str, domain: str) -> Path:
    base_dir = Path(CORE.config_dir) / ".esphome" / domain
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def clone_or_update(
    *,
    url: str,
    ref: str = None,
    refresh: TimePeriodSeconds,
    domain: str,
    username: str = None,
    password: str = None,
) -> Path:
    key = f"{url}@{ref}"

    if username is not None and password is not None:
        url = url.replace(
            "://", f"://{urllib.parse.quote(username)}:{urllib.parse.quote(password)}@"
        )

    repo_dir = _compute_destination_path(key, domain)
    fetch_pr_branch = ref is not None and ref.startswith("pull/")
    if not repo_dir.is_dir():
        _LOGGER.info("Cloning %s", key)
        _LOGGER.debug("Location: %s", repo_dir)
        cmd = ["git", "clone", "--depth=1"]
        if ref is not None and not fetch_pr_branch:
            cmd += ["--branch", ref]
        cmd += ["--", url, str(repo_dir)]
        run_git_command(cmd)

        if fetch_pr_branch:
            # We need to fetch the PR branch first, otherwise git will complain
            # about missing objects
            _LOGGER.info("Fetching %s", ref)
            run_git_command(["git", "fetch", "--", "origin", ref], str(repo_dir))
            run_git_command(["git", "reset", "--hard", "FETCH_HEAD"], str(repo_dir))

    else:
        # Check refresh needed
        file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
        # On first clone, FETCH_HEAD does not exists
        if not file_timestamp.exists():
            file_timestamp = Path(repo_dir / ".git" / "HEAD")
        age = datetime.now() - datetime.fromtimestamp(file_timestamp.stat().st_mtime)
        if age.total_seconds() > refresh.total_seconds:
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

    return repo_dir
