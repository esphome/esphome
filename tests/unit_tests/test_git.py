"""Tests for git.py module."""

from collections.abc import Callable
import logging
import os
from pathlib import Path
import subprocess
import time
from typing import Any
from unittest.mock import Mock, patch

import pytest

from esphome import git
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.git import GitCommandError


def _compute_repo_dir(url: str, ref: str | None, domain: str) -> Path:
    """Helper to compute the expected repo directory path using git module's logic."""
    key = f"{url}@{ref}"
    return git._compute_destination_path(key, domain)


# The tests must probe the exact location the implementation uses
_marker_path = git._clone_complete_marker_path


def _mark_clone_complete(repo_dir: Path) -> None:
    """Write the completion marker so a hand-made repo dir is treated as valid."""
    _marker_path(repo_dir).write_text("test")


def _setup_old_repo(repo_dir: Path, days_old: int = 2) -> None:
    """Helper to set up a git repo directory structure with an old timestamp.

    Args:
        repo_dir: The repository directory path to create.
        days_old: Number of days old to make the FETCH_HEAD file (default: 2).
    """
    # Create repo directory
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with old timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = time.time() - days_old * 86400
    fetch_head.touch()
    os.utime(fetch_head, (old_time, old_time))


def _get_git_command_type(cmd: list[str]) -> str | None:
    """Helper to determine the type of git command from a command list.

    Args:
        cmd: The git command list (e.g., ["git", "rev-parse", "HEAD"]).

    Returns:
        The command type ("rev-parse", "stash", "fetch", "reset", "clone") or None.
    """
    # Git commands are always in format ["git", "command", ...], so check index 1
    if len(cmd) > 1:
        return cmd[1]
    return None


def _simulate_cloned_repo(repo_dir: Path) -> None:
    """Create the directory structure a successful git clone would leave."""
    repo_dir.mkdir(parents=True, exist_ok=True)
    (repo_dir / ".git").mkdir(exist_ok=True)


def _make_clone_side_effect(
    repo_dir: Path, gitmodules: bool = False
) -> Callable[..., str]:
    """Return a run_git_command side effect whose clone creates the repo dir.

    With ``gitmodules`` the cloned repo also declares submodules.
    """

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            _simulate_cloned_repo(repo_dir)
            if gitmodules:
                (repo_dir / ".gitmodules").write_text("test")
        return ""

    return git_command_side_effect


def _submodule_calls(mock: Mock) -> list[Any]:
    """Return the mock's `git submodule` calls."""
    return [
        c for c in mock.call_args_list if _get_git_command_type(c[0][0]) == "submodule"
    ]


def _assert_submodule_runs_without_isolation(call: Any, repo_dir: Path) -> None:
    """Assert a git submodule call ran with plain cwd, not GIT_DIR/GIT_WORK_TREE
    isolation, which breaks the submodule porcelain on some installations."""
    assert call.kwargs.get("git_dir") is None
    assert call.kwargs.get("cwd") == repo_dir


def test_run_git_command_success(tmp_path: Path) -> None:
    """Test that run_git_command returns output on success."""
    # Create a simple git repo to test with
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()

    # Initialize a git repo
    result = git.run_git_command(["git", "init"], str(repo_dir))
    assert "Initialized empty Git repository" in result or result == ""

    # Verify we can run a command and get output
    result = git.run_git_command(["git", "status", "--porcelain"], str(repo_dir))
    # Empty repo should have empty status
    assert isinstance(result, str)


def test_run_git_command_debug_log_redacts_credentials(
    tmp_path: Path, mock_subprocess_run: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """Embedded URL credentials never reach the debug log; -v output is
    routinely pasted into public issues. subprocess is mocked so no real
    git ever sees the URL (the path is not creatable on Windows)."""
    mock_subprocess_run.return_value = Mock(returncode=0, stdout=b"", stderr=b"")
    with caplog.at_level(logging.DEBUG, logger="esphome.git"):
        git.run_git_command(
            ["git", "clone", "https://user:hunter2@github.com/test/repo"],
            cwd=tmp_path,
        )
    assert "hunter2" not in caplog.text
    assert "://***@github.com/test/repo" in caplog.text


def test_run_git_command_with_git_dir_isolation(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that git_dir parameter properly isolates git operations."""
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Configure mock to return success
    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"test output",
        stderr=b"",
    )

    # Ambient repo-scoping vars simulate a git hook invoking ESPHome; an
    # ambient GIT_INDEX_FILE surviving into a git_dir invocation fails
    # silently (git operates on the caller's index and exits 0).
    with patch.dict(
        os.environ,
        {"GIT_INDEX_FILE": "/caller/index", "GIT_OBJECT_DIRECTORY": "/caller/objects"},
    ):
        result = git.run_git_command(
            ["git", "rev-parse", "HEAD"],
            git_dir=repo_dir,
        )

    # Verify subprocess.run was called
    assert mock_subprocess_run.called
    call_args = mock_subprocess_run.call_args

    # Verify environment was set
    env = call_args[1]["env"]
    assert "GIT_DIR" in env
    assert "GIT_WORK_TREE" in env
    assert env["GIT_DIR"] == str(repo_dir / ".git")
    assert env["GIT_WORK_TREE"] == str(repo_dir)
    # The ambient scoping vars must be stripped, not passed through.
    assert "GIT_INDEX_FILE" not in env
    assert "GIT_OBJECT_DIRECTORY" not in env

    assert result == "test output"


def test_run_git_command_raises_git_not_installed_error(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that FileNotFoundError is converted to GitNotInstalledError."""
    from esphome.git import GitNotInstalledError

    repo_dir = tmp_path / "test_repo"

    # Configure mock to raise FileNotFoundError
    mock_subprocess_run.side_effect = FileNotFoundError("git not found")

    with pytest.raises(GitNotInstalledError, match="git is not installed"):
        git.run_git_command(["git", "status"], git_dir=repo_dir)


def test_run_git_command_raises_git_command_error_on_failure(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that failed git commands raise GitCommandError."""
    repo_dir = tmp_path / "test_repo"

    # Configure mock to return non-zero exit code
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"fatal: not a git repository",
    )

    with pytest.raises(GitCommandError, match="not a git repository"):
        git.run_git_command(["git", "status"], git_dir=repo_dir)


def test_run_git_command_strips_fatal_prefix(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that 'fatal: ' prefix is stripped from error messages."""
    repo_dir = tmp_path / "test_repo"

    # Configure mock to return error with "fatal: " prefix
    mock_subprocess_run.return_value = Mock(
        returncode=128,
        stdout=b"",
        stderr=b"fatal: repository not found\n",
    )

    with pytest.raises(GitCommandError) as exc_info:
        git.run_git_command(["git", "clone", "invalid-url"], git_dir=repo_dir)

    # Error message should NOT include "fatal: " prefix
    assert "fatal:" not in str(exc_info.value)
    assert "repository not found" in str(exc_info.value)


def test_run_git_command_without_git_dir(mock_subprocess_run: Mock) -> None:
    """Test that run_git_command works without git_dir (clone case)."""
    # Configure mock to return success
    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"Cloning into 'test_repo'...",
        stderr=b"",
    )

    result = git.run_git_command(["git", "clone", "https://github.com/test/repo"])

    # Verify subprocess.run was called
    assert mock_subprocess_run.called
    call_args = mock_subprocess_run.call_args

    # Verify environment does NOT have GIT_DIR or GIT_WORK_TREE set
    # (it should use the default environment or None)
    env = call_args[1].get("env")
    if env is not None:
        assert "GIT_DIR" not in env
        assert "GIT_WORK_TREE" not in env

    # Verify cwd is None (default)
    assert call_args[1].get("cwd") is None

    assert result == "Cloning into 'test_repo'..."


@pytest.mark.parametrize("relative", [False, True], ids=["absolute", "relative"])
def test_run_git_command_with_cwd_runs_in_dir_without_isolation(
    tmp_path: Path,
    mock_subprocess_run: Mock,
    monkeypatch: pytest.MonkeyPatch,
    relative: bool,
) -> None:
    """The cwd parameter sets the working directory without GIT_DIR/GIT_WORK_TREE.

    Ambient GIT_DIR/GIT_WORK_TREE (e.g. from a git hook or CI wrapper) must be
    stripped too, and GIT_CEILING_DIRECTORIES must stop git from walking up to
    an enclosing repository if the target repo's .git is missing or corrupt.
    Git silently ignores a relative ceiling entry, so the variable must come
    out absolute even when the given cwd is relative.
    """
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()
    if relative:
        monkeypatch.chdir(tmp_path)
        cwd_arg = Path("test_repo")
    else:
        cwd_arg = repo_dir

    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"test output",
        stderr=b"",
    )

    with patch.dict(
        os.environ,
        {
            "GIT_DIR": "/ambient/.git",
            "GIT_WORK_TREE": "/ambient",
            "GIT_INDEX_FILE": "/ambient/.git/index",
        },
    ):
        result = git.run_git_command(["git", "submodule", "update"], cwd=cwd_arg)

    call_args = mock_subprocess_run.call_args
    env = call_args[1]["env"]
    assert "GIT_DIR" not in env
    assert "GIT_WORK_TREE" not in env
    assert "GIT_INDEX_FILE" not in env
    ceiling = Path(env["GIT_CEILING_DIRECTORIES"])
    assert ceiling.is_absolute()
    assert ceiling.samefile(tmp_path)
    assert call_args[1]["cwd"] == cwd_arg
    assert result == "test output"


def test_run_git_command_raises_on_nonfatal_stderr(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Nonzero exit with stderr lacking a fatal: prefix raises with full stderr."""
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"error: pathspec 'nope' did not match any file(s)\n",
    )

    with pytest.raises(GitCommandError, match="did not match"):
        git.run_git_command(["git", "checkout", "nope"], git_dir=tmp_path)


def test_run_git_command_raises_on_nonzero_exit_without_stderr(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """A nonzero exit must raise even when git printed nothing to stderr.

    Silent nonzero exits were previously treated as success, which is how
    broken checkouts could be cached as complete.
    """
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"",
    )

    with pytest.raises(GitCommandError, match="exited with code 1"):
        git.run_git_command(["git", "submodule", "update"], cwd=tmp_path)


def test_run_git_command_without_git_dir_raises_error(
    mock_subprocess_run: Mock,
) -> None:
    """Test that run_git_command without git_dir can still raise errors."""
    # Configure mock to return error
    mock_subprocess_run.return_value = Mock(
        returncode=128,
        stdout=b"",
        stderr=b"fatal: repository not found\n",
    )

    with pytest.raises(GitCommandError, match="repository not found"):
        git.run_git_command(["git", "clone", "https://invalid.url/repo.git"])


def test_clone_or_update_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that NEVER_REFRESH skips updates for existing repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with current timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")

    # Call with NEVER_REFRESH
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=git.NEVER_REFRESH,
        domain=domain,
    )

    # Should NOT call git commands since NEVER_REFRESH and repo exists
    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None


def test_clone_or_update_skips_when_core_skip_external_update(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """CORE.skip_external_update short-circuits the refresh for existing repos."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)
    (git_dir / "FETCH_HEAD").write_text("test")

    CORE.skip_external_update = True
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
    )

    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None


def test_clone_or_update_with_refresh_updates_old_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh triggers update for old repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with old timestamp (2 days ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = time.time() - 2 * 86400
    fetch_head.touch()  # Create the file
    # Set modification time to 2 days ago
    os.utime(fetch_head, (old_time, old_time))

    # Mock git command responses
    mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

    # Call with refresh=1d (1 day)
    refresh = TimePeriodSeconds(days=1)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
    )

    # Should call git fetch and update commands since repo is older than refresh
    assert mock_run_git_command.called
    # Check for fetch command
    fetch_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "fetch" in call[0][0]
    ]
    assert len(fetch_calls) > 0


def test_clone_or_update_with_refresh_skips_fresh_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh doesn't update fresh repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with recent timestamp (1 hour ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = time.time() - 3600
    fetch_head.touch()  # Create the file
    # Set modification time to 1 hour ago
    os.utime(fetch_head, (recent_time, recent_time))

    # Call with refresh=1d (1 day)
    refresh = TimePeriodSeconds(days=1)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
    )

    # Should NOT call git fetch since repo is fresh
    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None


def test_clone_or_update_clones_missing_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that missing repos are cloned regardless of refresh setting."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create base directory but NOT the repo itself
    base_dir = tmp_path / ".esphome" / domain
    base_dir.mkdir(parents=True)
    # repo_dir should NOT exist
    assert not repo_dir.exists()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    # Test with NEVER_REFRESH - should still clone since repo doesn't exist
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=git.NEVER_REFRESH,
        domain=domain,
    )

    # Should call git clone
    assert mock_run_git_command.called
    clone_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "clone" in call[0][0]
    ]
    assert len(clone_calls) > 0


def test_clone_or_update_with_none_refresh_always_updates(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh=None always updates existing repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with very recent timestamp (1 second ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = time.time() - 1
    fetch_head.touch()  # Create the file
    # Set modification time to 1 second ago
    os.utime(fetch_head, (recent_time, recent_time))

    # Mock git command responses
    mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

    # Call with refresh=None (default behavior)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=None,
        domain=domain,
    )

    # Should call git fetch and update commands since refresh=None means always update
    assert mock_run_git_command.called
    # Check for fetch command
    fetch_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "fetch" in call[0][0]
    ]
    assert len(fetch_calls) > 0


@pytest.mark.parametrize(
    ("fail_command", "error_message"),
    [
        (
            "rev-parse",
            "ambiguous argument 'HEAD': unknown revision or path not in the working tree.",
        ),
        ("stash", "fatal: unable to write new index file"),
        (
            "fetch",
            "fatal: unable to access 'https://github.com/test/repo/': Could not resolve host",
        ),
        ("reset", "fatal: Could not reset index file to revision 'FETCH_HEAD'"),
    ],
)
def test_clone_or_update_recovers_from_git_failures(
    tmp_path: Path, mock_run_git_command: Mock, fail_command: str, error_message: str
) -> None:
    """Test that repos are re-cloned when various git commands fail."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Track command call counts to make first call fail, subsequent calls succeed
    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        # Determine which command this is
        cmd_type = _get_git_command_type(cmd)

        # Track call count for this command type
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # Fail on first call to the specified command, succeed on subsequent calls
        if cmd_type == fail_command and call_counts[cmd_type] == 1:
            raise GitCommandError(error_message)

        # Default successful responses
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "clone":
            # Simulate the recovery re-clone creating the repo directory
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
    )

    # Verify recovery happened
    call_list = mock_run_git_command.call_args_list

    # Should have attempted the failing command
    assert any(fail_command in str(c) for c in call_list)

    # Should have called clone for recovery
    assert any("clone" in str(c) for c in call_list)

    # Verify the repo directory path is returned
    assert result_dir == repo_dir


def test_clone_or_update_fails_when_recovery_also_fails(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that we don't infinitely recurse when recovery also fails."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Mock git command to fail on clone (simulating network failure during recovery)
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            # First time fails (broken repo)
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if cmd_type == "clone":
            # Clone also fails (recovery fails)
            raise GitCommandError("fatal: unable to access repository")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise after one recovery attempt fails
    with pytest.raises(GitCommandError, match="fatal: unable to access repository"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify we only tried to clone once (no infinite recursion)
    call_list = mock_run_git_command.call_args_list
    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    # Should have exactly one clone call (the recovery attempt that failed)
    assert len(clone_calls) == 1
    # Should have tried rev-parse once (which failed and triggered recovery)
    rev_parse_calls = [c for c in call_list if "rev-parse" in c[0][0]]
    assert len(rev_parse_calls) == 1


def test_clone_or_update_recover_broken_flag_prevents_second_recovery(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that _recover_broken=False prevents a second recovery attempt (tests the raise path)."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Track fetch calls to differentiate between first (in clone) and second (in recovery update)
    call_counts: dict[str, int] = {}

    # Mock git command to fail on fetch during recovery's ref checkout
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)

        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # First attempt: rev-parse fails (broken repo)
        if cmd_type == "rev-parse" and call_counts[cmd_type] == 1:
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )

        # Recovery: clone succeeds
        if cmd_type == "clone":
            return ""

        # Recovery: fetch for ref checkout fails
        # This happens in the clone path when ref is not None (line 80 in git.py)
        if cmd_type == "fetch" and call_counts[cmd_type] == 1:
            raise GitCommandError("fatal: couldn't find remote ref main")

        # Default success
        return "abc123" if cmd_type == "rev-parse" else ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise on the fetch during recovery (when _recover_broken=False)
    # This tests the critical "if not _recover_broken: raise" path
    with pytest.raises(GitCommandError, match="fatal: couldn't find remote ref main"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify the sequence of events
    call_list = mock_run_git_command.call_args_list

    # Should have: rev-parse (fail, triggers recovery), clone (success),
    # fetch (fail during ref checkout, raises because _recover_broken=False)
    rev_parse_calls = [c for c in call_list if "rev-parse" in c[0][0]]
    # Should have exactly one rev-parse call that failed
    assert len(rev_parse_calls) == 1

    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    # Should have exactly one clone call (the recovery attempt)
    assert len(clone_calls) == 1

    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    # Should have exactly one fetch call that failed (during ref checkout in recovery)
    assert len(fetch_calls) == 1


def test_clone_or_update_recover_broken_flag_prevents_infinite_loop(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that _recover_broken=False prevents infinite recursion when repo persists."""
    # This tests the critical "if not _recover_broken: raise" path at line 124-125
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Mock shutil.rmtree to NOT actually delete the directory
    # This simulates a scenario where deletion fails (permissions, etc.)
    import unittest.mock

    def mock_rmtree(path, *args, **kwargs):
        # Don't actually delete - this causes the recursive call to still see the repo
        pass

    # Mock git commands to always fail on stash
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "stash":
            # Always fails
            raise GitCommandError("fatal: unable to write new index file")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Mock shutil.rmtree and test
    # Should raise on the second attempt when _recover_broken=False
    # This hits the "if not _recover_broken: raise" path
    with (
        unittest.mock.patch("esphome.git.rmtree", side_effect=mock_rmtree),
        pytest.raises(GitCommandError, match="fatal: unable to write new index file"),
    ):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify the sequence: stash fails twice (once triggering recovery, once raising)
    call_list = mock_run_git_command.call_args_list
    stash_calls = [c for c in call_list if "stash" in c[0][0]]
    # Should have exactly two stash calls
    assert len(stash_calls) == 2


def test_clone_or_update_cleans_up_on_failed_ref_fetch(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that a failed ref fetch removes the incomplete clone directory.

    When cloning with a specific ref, if `git clone` succeeds but the
    subsequent `git fetch <ref>` fails, the clone directory should be
    removed so the next attempt starts fresh instead of finding a stale
    clone on the default branch.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "clone":
            # Simulate successful clone by creating the directory
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
            return ""
        if cmd_type == "fetch":
            raise GitCommandError("fatal: couldn't find remote ref pull/123/head")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    with pytest.raises(GitCommandError, match="couldn't find remote ref"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # The incomplete clone directory should have been removed
    assert not repo_dir.exists()

    # Verify clone was attempted then fetch failed
    call_list = mock_run_git_command.call_args_list
    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1


def test_clone_or_update_stale_clone_is_retried_after_cleanup(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that after cleanup, a subsequent call does a fresh clone.

    This is the full scenario: first call fails at fetch (directory cleaned up),
    second call sees no directory and clones fresh.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    call_count = {"clone": 0, "fetch": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "clone":
            call_count["clone"] += 1
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
            return ""
        if cmd_type == "fetch":
            call_count["fetch"] += 1
            if call_count["fetch"] == 1:
                # First fetch fails
                raise GitCommandError("fatal: couldn't find remote ref pull/123/head")
            # Second fetch succeeds
            return ""
        if cmd_type == "reset":
            return ""
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # First call: clone succeeds, fetch fails, directory cleaned up
    with pytest.raises(GitCommandError, match="couldn't find remote ref"):
        git.clone_or_update(url=url, ref=ref, refresh=refresh, domain=domain)

    assert not repo_dir.exists()

    # Second call: fresh clone + fetch succeeds
    result_dir, _ = git.clone_or_update(
        url=url, ref=ref, refresh=refresh, domain=domain
    )

    assert result_dir == repo_dir
    assert repo_dir.exists()
    assert call_count["clone"] == 2
    assert call_count["fetch"] == 2


def test_clone_or_update_recloned_when_marker_missing_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A repo dir without the completion marker is an interrupted clone.

    It must be removed and re-cloned even with NEVER_REFRESH, which would
    otherwise trust the broken directory forever.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "1.8.4"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Simulate an interrupted clone: directory exists, no marker
    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    result_dir, _ = git.clone_or_update(
        url=url, ref=ref, refresh=git.NEVER_REFRESH, domain=domain
    )

    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert result_dir == repo_dir
    # The fresh clone completed, so the marker must now be present
    assert _marker_path(repo_dir).is_file()


def test_clone_or_update_recloned_when_marker_missing_with_skip_external_update(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """skip_external_update must not preserve an interrupted clone."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    CORE.skip_external_update = True
    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert result_dir == repo_dir
    assert _marker_path(repo_dir).is_file()


def test_fresh_clone_writes_completion_marker_with_debug_info(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The marker is written after a fresh clone and records key and hash dir."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    git.clone_or_update(url=url, ref=ref, refresh=git.NEVER_REFRESH, domain=domain)

    marker = _marker_path(repo_dir)
    assert marker.is_file()
    content = marker.read_text()
    assert f"{url}@{ref}" in content
    assert repo_dir.name in content


def test_marker_is_deleted_before_rmtree(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The marker must be gone even if rmtree fails partway.

    Simulated by an rmtree that does nothing: the directory survives but the
    marker must already have been deleted, so the next run still re-clones
    instead of trusting a partially deleted worktree.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)
    assert _marker_path(repo_dir).is_file()

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "stash":
            raise GitCommandError("fatal: unable to write new index file")
        return "abc123"

    mock_run_git_command.side_effect = git_command_side_effect

    with (
        patch("esphome.git.rmtree"),
        pytest.raises(GitCommandError),
    ):
        git.clone_or_update(
            url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
        )

    # rmtree never deleted anything, yet the marker is gone
    assert repo_dir.is_dir()
    assert not _marker_path(repo_dir).is_file()


def test_failed_marker_write_does_not_fail_the_clone(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A marker write failure must not fail an otherwise complete clone.

    The clone is valid; the missing marker only costs a re-clone on the next
    run, so the error is logged as a warning instead of propagating.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    with patch(
        "esphome.git.write_file", side_effect=EsphomeError("Could not write file")
    ):
        result_dir, _ = git.clone_or_update(
            url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
        )

    assert result_dir == repo_dir
    assert not _marker_path(repo_dir).is_file()
    assert "Could not write clone completion marker" in caplog.text


def test_corrupt_git_dir_without_head_recovers(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A .git with neither FETCH_HEAD nor HEAD must recover, not crash.

    The age check stats FETCH_HEAD falling back to HEAD; if both are gone
    (partially deleted clone) the stat raised an unhandled FileNotFoundError
    before the broken-repository recovery could run.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    # Marker present but .git gutted: no FETCH_HEAD, no HEAD
    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()
    _mark_clone_complete(repo_dir)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            raise GitCommandError("ambiguous argument 'HEAD': unknown revision")
        if cmd_type == "clone":
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    assert result_dir == repo_dir
    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert _marker_path(repo_dir).is_file()


def test_remove_repo_dir_tolerates_marker_unlink_failure(tmp_path: Path) -> None:
    """A locked marker file must not abort the directory removal."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / ".git").mkdir()
    _mark_clone_complete(repo_dir)

    with patch.object(Path, "unlink", side_effect=PermissionError("locked")):
        git._remove_repo_dir(repo_dir)

    # rmtree still removed the directory, marker included
    assert not repo_dir.exists()


def test_clone_or_update_recovery_preserves_subpath(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Recovery must re-clone into the same subpath-ed directory.

    Without passing subpath through, the recursive recovery call would
    recompute the destination without the subpath and clone (and write the
    completion marker) at the wrong location.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    subpath = Path("mylib")
    repo_dir = _compute_repo_dir(url, ref, domain) / subpath

    _setup_old_repo(repo_dir)

    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1
        # First rev-parse fails (broken repo) to trigger recovery
        if cmd_type == "rev-parse" and call_counts[cmd_type] == 1:
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if cmd_type == "clone":
            # Create whatever directory the clone was asked to target
            target = Path(cmd[-1])
            target.mkdir(parents=True, exist_ok=True)
            (target / ".git").mkdir(exist_ok=True)
        if cmd_type == "rev-parse":
            return "abc123"
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    result_dir, _ = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
        subpath=subpath,
    )

    # The recovery re-clone must target the subpath-ed directory and the
    # completion marker must land there too
    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert clone_calls[0][0][0][-1] == str(repo_dir)
    assert result_dir == repo_dir
    assert _marker_path(repo_dir).is_file()


def test_clone_with_ref_uses_shallow_fetch(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Clone with a ref should use --depth=1 on both clone and fetch."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    git.clone_or_update(url=url, ref=ref, refresh=None, domain=domain)

    call_list = mock_run_git_command.call_args_list

    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert "--depth=1" in clone_calls[0][0][0]

    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1
    assert "--depth=1" in fetch_calls[0][0][0]
    # Ref must still be passed so the requested commit/branch is fetched.
    assert ref in fetch_calls[0][0][0]


def test_refresh_fetch_is_shallow(tmp_path: Path, mock_run_git_command: Mock) -> None:
    """The refresh-path fetch should use --depth=1."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)
    mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    fetch_calls = [c for c in mock_run_git_command.call_args_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1
    cmd = fetch_calls[0][0][0]
    assert "--depth=1" in cmd
    # Ref must still be in the refresh fetch so the right tip is updated.
    assert cmd[-1] == ref


@pytest.mark.parametrize(
    "refresh", [None, TimePeriodSeconds(days=1)], ids=["clone", "refresh"]
)
def test_all_submodules_skipped_without_gitmodules(
    tmp_path: Path, mock_run_git_command: Mock, refresh: TimePeriodSeconds | None
) -> None:
    """init_submodules is a no-op for repos with no .gitmodules.

    This is the esp-idf toolchain library scenario from issue #17860: the
    PlatformIO library converter requests "all submodules" for every git
    library, and most libraries declare none. The git submodule porcelain
    must not run at all in that case — it fails outright on some git
    installations.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    if refresh is None:
        mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)
    else:
        _setup_old_repo(repo_dir)
        mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url,
        ref=None,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    assert not _submodule_calls(mock_run_git_command)


@pytest.mark.parametrize(
    "refresh", [None, TimePeriodSeconds(days=1)], ids=["clone", "refresh"]
)
def test_all_submodules_updated_with_gitmodules(
    tmp_path: Path, mock_run_git_command: Mock, refresh: TimePeriodSeconds | None
) -> None:
    """init_submodules initializes all submodules when .gitmodules exists."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    if refresh is None:
        mock_run_git_command.side_effect = _make_clone_side_effect(
            repo_dir, gitmodules=True
        )
    else:
        _setup_old_repo(repo_dir)
        (repo_dir / ".gitmodules").write_text("test")
        mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url,
        ref=None,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    submodule_calls = _submodule_calls(mock_run_git_command)
    # Which submodules get populated is git's own policy, so no status
    # verification follows the update.
    assert len(submodule_calls) == 1
    cmd = submodule_calls[0][0][0]
    assert cmd[2] == "update"
    assert "--depth=1" in cmd
    # Recursive, mirroring PlatformIO's recursive library clones.
    assert "--recursive" in cmd
    _assert_submodule_runs_without_isolation(submodule_calls[0], repo_dir)


def test_recovery_reclone_keeps_credentials_and_cache_key(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The recovery re-clone must not re-apply credentials to the already
    rewritten URL (no doubled userinfo) and must land in the same cache
    directory, or a credentialed private repo re-clones on every run."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    _setup_old_repo(repo_dir)
    (repo_dir / ".gitmodules").write_text("test")

    calls = {"submodule": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            _simulate_cloned_repo(repo_dir)
        if _get_git_command_type(cmd) == "submodule":
            calls["submodule"] += 1
            if calls["submodule"] == 1:
                raise git.GitCommandError("git submodule update exited with code 1")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    recovered_dir, _ = git.clone_or_update(
        url=url,
        ref=None,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
        username="user",
        password="hunter2",
        init_submodules=True,
    )

    assert recovered_dir == repo_dir
    clone_cmds = [
        c[0][0]
        for c in mock_run_git_command.call_args_list
        if _get_git_command_type(c[0][0]) == "clone"
    ]
    assert clone_cmds
    clone_url = clone_cmds[0][-2]
    assert clone_url == "https://user:hunter2@github.com/test/repo"
    assert clone_url.count("@") == 1


def test_refresh_submodule_failure_recovers_then_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A refresh-path submodule failure routes through the recovery re-clone.

    The broken repo is removed and re-cloned; when the submodule update fails
    again on the fresh clone the cache entry is removed and the error
    propagates, instead of leaving behind a repo the refresh window would
    silently accept on the next run.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    _setup_old_repo(repo_dir)
    (repo_dir / ".gitmodules").write_text("test")

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            _simulate_cloned_repo(repo_dir)
            (repo_dir / ".gitmodules").write_text("test")
        if _get_git_command_type(cmd) == "submodule":
            raise git.GitCommandError("git submodule update exited with code 1")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    with pytest.raises(git.GitCommandError, match="exited with code 1"):
        git.clone_or_update(
            url=url,
            ref=None,
            refresh=TimePeriodSeconds(days=1),
            domain=domain,
            init_submodules=True,
        )

    assert not repo_dir.is_dir()
    # Recovery removed the repo and re-cloned before failing again.
    assert any(
        _get_git_command_type(c[0][0]) == "clone"
        for c in mock_run_git_command.call_args_list
    )


def _real_git(*args: str, cwd: Path) -> None:
    """Run real git to build a test fixture repository."""
    subprocess.run(
        [
            "git",
            "-c",
            "user.email=test@test.invalid",
            "-c",
            "user.name=test",
            "-c",
            "commit.gpgsign=false",
            "-c",
            "protocol.file.allow=always",
            *args,
        ],
        cwd=cwd,
        check=True,
        capture_output=True,
    )


# Git blocks file-protocol submodules by default (CVE-2022-39253); the e2e
# tests allow them via GIT_CONFIG_* environment variables, which reach the
# child git processes through run_git_command's filtered environment.
_ALLOW_FILE_PROTOCOL_ENV = {
    "GIT_CONFIG_COUNT": "1",
    "GIT_CONFIG_KEY_0": "protocol.file.allow",
    "GIT_CONFIG_VALUE_0": "always",
}


def _make_real_repo(path: Path, filename: str) -> None:
    """Create a real git repository containing one committed file."""
    path.mkdir()
    _real_git("init", "-q", cwd=path)
    (path / filename).write_text("content")
    _real_git("add", filename, cwd=path)
    _real_git("commit", "-q", "-m", "init", cwd=path)


def _add_submodule(
    repo: Path, url: Path, path: str, *, update_none: bool = False
) -> None:
    """Add ``url`` as a submodule of ``repo`` at ``path`` and commit it."""
    _real_git("submodule", "add", str(url), path, cwd=repo)
    if update_none:
        _real_git(
            "config", "-f", ".gitmodules", f"submodule.{path}.update", "none", cwd=repo
        )
        _real_git("add", ".gitmodules", cwd=repo)
    _real_git("commit", "-q", "-m", f"add submodule {path}", cwd=repo)


def test_clone_or_update_real_git_without_submodules(tmp_path: Path) -> None:
    """End-to-end with real git: a repo with no .gitmodules clones cleanly.

    This is the issue #17860 scenario: requesting "all submodules" on a
    submodule-less repository must not invoke the git submodule porcelain
    and must produce a usable checkout.
    """
    CORE.config_path = tmp_path / "test.yaml"

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")

    repo_dir, _ = git.clone_or_update(
        url=str(upstream),
        ref=None,
        refresh=None,
        domain="test_e2e",
        init_submodules=True,
    )

    assert (repo_dir / "README.md").is_file()


def test_clone_or_update_real_git_initializes_submodules(tmp_path: Path) -> None:
    """End-to-end with real git: submodules are actually checked out.

    Exercises the real `git submodule update` invocation, including the
    env handling in run_git_command that the mocked tests cannot cover.
    """
    CORE.config_path = tmp_path / "test.yaml"

    sub_repo = tmp_path / "sub"
    _make_real_repo(sub_repo, "sub_file.txt")

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")
    _add_submodule(upstream, sub_repo, "vendor/sub")

    with patch.dict(os.environ, _ALLOW_FILE_PROTOCOL_ENV):
        repo_dir, _ = git.clone_or_update(
            url=str(upstream),
            ref=None,
            refresh=None,
            domain="test_e2e",
            init_submodules=True,
        )

    assert (repo_dir / "vendor" / "sub" / "sub_file.txt").is_file()


def test_clone_or_update_real_git_honors_update_none_submodule(
    tmp_path: Path,
) -> None:
    """End-to-end with real git: submodules declared `update = none` stay skipped.

    Shows git itself skipping the declared paths at both nesting levels
    (and exiting 0) while the regular submodules check out.
    """
    CORE.config_path = tmp_path / "test.yaml"

    sub_repo = tmp_path / "sub"
    _make_real_repo(sub_repo, "sub_file.txt")

    # Intermediate submodule that itself declares a skipped nested submodule.
    mid_repo = tmp_path / "mid"
    _make_real_repo(mid_repo, "mid_file.txt")
    _add_submodule(mid_repo, sub_repo, "vendor/leaf", update_none=True)

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")
    _add_submodule(upstream, sub_repo, "vendor/sub")
    _add_submodule(upstream, sub_repo, "vendor/skipped", update_none=True)
    _add_submodule(upstream, mid_repo, "vendor/mid")

    with patch.dict(os.environ, _ALLOW_FILE_PROTOCOL_ENV):
        repo_dir, _ = git.clone_or_update(
            url=str(upstream),
            ref=None,
            refresh=None,
            domain="test_e2e",
            init_submodules=True,
        )

    assert (repo_dir / "vendor" / "sub" / "sub_file.txt").is_file()
    assert not (repo_dir / "vendor" / "skipped" / "sub_file.txt").exists()
    assert (repo_dir / "vendor" / "mid" / "mid_file.txt").is_file()
    assert not (
        repo_dir / "vendor" / "mid" / "vendor" / "leaf" / "sub_file.txt"
    ).exists()


def test_refresh_picks_up_new_remote_commits(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Shallow fetch must still pull new commits when the remote tip moves.

    Simulates a stale local repo at SHA "old" while the remote has advanced
    to SHA "new". The refresh path must run fetch (with --depth=1) followed
    by reset --hard FETCH_HEAD so the working tree advances to the new tip.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)

    # rev-parse is called once before fetch to record the pre-update SHA.
    rev_parse_calls = {"count": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            rev_parse_calls["count"] += 1
            return "old_sha"
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    _, revert = git.clone_or_update(
        url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    # Verify the refresh sequence: rev-parse -> stash -> fetch (depth=1) -> reset
    call_list = mock_run_git_command.call_args_list
    cmd_sequence = [_get_git_command_type(c[0][0]) for c in call_list]
    assert cmd_sequence == ["rev-parse", "stash", "fetch", "reset"]

    fetch_cmd = call_list[2][0][0]
    assert "--depth=1" in fetch_cmd
    assert fetch_cmd[-1] == ref

    reset_cmd = call_list[3][0][0]
    assert reset_cmd[-1] == "FETCH_HEAD"

    # revert callback should reset back to the recorded pre-update SHA.
    assert revert is not None
    revert()
    assert mock_run_git_command.call_args_list[-1][0][0] == [
        "git",
        "reset",
        "--hard",
        "old_sha",
    ]


def test_resolve_symlink_stub_returns_none_on_non_windows(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """On non-Windows, resolve_symlink_stub returns None without calling git."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    stub = repo_dir / "file.yaml"
    stub.write_text("static/file.yaml")

    with patch("esphome.git.sys.platform", "linux"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_target_for_mode_120000(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A mode-120000 file is recognised as a stub; its target Path is returned."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "static").mkdir()

    target = repo_dir / "static" / "real.yaml"
    target.write_text("esphome:\n  name: real\n")

    stub = repo_dir / "real.yaml"
    stub.write_text("static/real.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\treal.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()
    # Stub file itself was not modified — only inspected.
    assert stub.read_text() == "static/real.yaml"


def test_resolve_symlink_stub_resolves_relative_parent_paths(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Symlink targets with ``..`` segments resolve correctly within the repo."""
    repo_dir = tmp_path / "repo"
    (repo_dir / "subdir").mkdir(parents=True)
    (repo_dir / "static").mkdir()

    target = repo_dir / "static" / "shared.yaml"
    target.write_text("shared content")

    stub = repo_dir / "subdir" / "shared.yaml"
    stub.write_text("../static/shared.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tsubdir/shared.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()


def test_resolve_symlink_stub_refuses_escape_outside_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A symlink pointing outside the repository is not followed."""
    outside = tmp_path / "outside.yaml"
    outside.write_text("sensitive")

    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "escape.yaml"
    stub.write_text("../outside.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tescape.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_for_real_symlink(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A real symlink already opens transparently, so the helper short-circuits.

    Skipped on Windows where symlink creation requires
    SeCreateSymbolicLinkPrivilege.
    """
    if os.name == "nt":
        pytest.skip("Requires symlink-creation privilege on Windows")

    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    target = repo_dir / "real.yaml"
    target.write_text("real content")

    real_link = repo_dir / "link.yaml"
    real_link.symlink_to("real.yaml")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, real_link)

    assert result is None
    # No git call needed for real symlinks.
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_for_regular_file(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A regular file (mode 100644) whose content looks path-shaped is not
    followed."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    regular = repo_dir / "looks_like_path.txt"
    regular.write_text("static/something.yaml")

    mock_run_git_command.return_value = "100644 abc123 0\tlooks_like_path.txt"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, regular)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_git_fails(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """If ``git ls-files`` fails (e.g. not a repo), the helper returns None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "real.yaml"
    stub.write_text("static/real.yaml")

    mock_run_git_command.side_effect = GitCommandError("ls-files exploded")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_for_non_utf8_content(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file whose bytes are not valid UTF-8 must not raise — return None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "binary.bin"
    stub.write_bytes(b"\xff\xfe\x00\xff")

    mock_run_git_command.return_value = "120000 abc123 0\tbinary.bin"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_preserves_whitespace_in_target(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Only trailing CR/LF is stripped — internal whitespace is preserved."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    target_dir = repo_dir / "dir with spaces"
    target_dir.mkdir()
    target = target_dir / "real.yaml"
    target.write_text("hello")

    stub = repo_dir / "link.yaml"
    # Trailing newline (as git's checkout may append) is stripped, but
    # whitespace inside the target path itself must survive.
    stub.write_bytes(b"dir with spaces/real.yaml\n")

    mock_run_git_command.return_value = "120000 abc123 0\tlink.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()


def test_resolve_symlink_stub_returns_none_for_directory_target(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A symlink pointing at a directory has no file content to load."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "dir_target").mkdir()

    stub = repo_dir / "link_to_dir"
    stub.write_text("dir_target")

    mock_run_git_command.return_value = "120000 abc123 0\tlink_to_dir"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_resolve_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Path.resolve() raising (e.g. on a malformed target) must not propagate."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "broken.yaml"
    stub.write_text("ignored")

    mock_run_git_command.return_value = "120000 abc123 0\tbroken.yaml"

    with (
        patch("esphome.git.sys.platform", "win32"),
        patch.object(Path, "resolve", side_effect=OSError("bad path")),
    ):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_file_missing(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file path that doesn't exist is rejected before git is consulted."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    missing = repo_dir / "ghost.yaml"  # not created

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, missing)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_when_path_outside_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file path that isn't under repo_dir is rejected (ValueError from relative_to)."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    outside = tmp_path / "stray.yaml"
    outside.write_text("something")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, outside)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_when_untracked(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Empty `git ls-files` output (untracked file) makes the helper return None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "untracked.yaml"
    stub.write_text("static/foo.yaml")

    mock_run_git_command.return_value = ""

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_read_bytes_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """An OSError from read_bytes() (e.g. file vanished mid-call) must not propagate."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "racy.yaml"
    stub.write_text("static/racy.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tracy.yaml"

    with (
        patch("esphome.git.sys.platform", "win32"),
        patch.object(Path, "read_bytes", side_effect=OSError("vanished")),
    ):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None
