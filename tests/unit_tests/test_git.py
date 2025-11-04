"""Tests for git.py module."""

from datetime import datetime, timedelta
import os
from pathlib import Path
from typing import Any
from unittest.mock import Mock

import pytest

from esphome import git
from esphome.core import CORE, TimePeriodSeconds
from esphome.git import GitCommandError


def _compute_repo_dir(url: str, ref: str | None, domain: str) -> Path:
    """Helper to compute the expected repo directory path using git module's logic."""
    key = f"{url}@{ref}"
    return git._compute_destination_path(key, domain)


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

    # Create FETCH_HEAD file with old timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = datetime.now() - timedelta(days=days_old)
    fetch_head.touch()
    os.utime(fetch_head, (old_time.timestamp(), old_time.timestamp()))


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

    # Create FETCH_HEAD file with old timestamp (2 days ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = datetime.now() - timedelta(days=2)
    fetch_head.touch()  # Create the file
    # Set modification time to 2 days ago
    os.utime(fetch_head, (old_time.timestamp(), old_time.timestamp()))

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

    # Create FETCH_HEAD file with recent timestamp (1 hour ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = datetime.now() - timedelta(hours=1)
    fetch_head.touch()  # Create the file
    # Set modification time to 1 hour ago
    os.utime(fetch_head, (recent_time.timestamp(), recent_time.timestamp()))

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

    # Create FETCH_HEAD file with very recent timestamp (1 second ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = datetime.now() - timedelta(seconds=1)
    fetch_head.touch()  # Create the file
    # Set modification time to 1 second ago
    os.utime(fetch_head, (recent_time.timestamp(), recent_time.timestamp()))

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
        unittest.mock.patch("esphome.git.shutil.rmtree", side_effect=mock_rmtree),
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
