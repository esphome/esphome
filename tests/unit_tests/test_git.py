"""Tests for git.py module."""

from datetime import datetime, timedelta
import hashlib
import os
from pathlib import Path
from unittest.mock import Mock

import pytest

from esphome import git
import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds


def test_clone_or_update_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that NEVER_REFRESH skips updates for existing repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    # Compute the expected repo directory path
    url = "https://github.com/test/repo"
    ref = None
    key = f"{url}@{ref}"
    domain = "test"

    # Compute hash-based directory name (matching _compute_destination_path logic)
    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

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

    # Compute the expected repo directory path
    url = "https://github.com/test/repo"
    ref = None
    key = f"{url}@{ref}"
    domain = "test"

    # Compute hash-based directory name (matching _compute_destination_path logic)
    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

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

    # Compute the expected repo directory path
    url = "https://github.com/test/repo"
    ref = None
    key = f"{url}@{ref}"
    domain = "test"

    # Compute hash-based directory name (matching _compute_destination_path logic)
    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

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

    # Compute the expected repo directory path
    url = "https://github.com/test/repo"
    ref = None
    key = f"{url}@{ref}"
    domain = "test"

    # Compute hash-based directory name (matching _compute_destination_path logic)
    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

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

    # Compute the expected repo directory path
    url = "https://github.com/test/repo"
    ref = None
    key = f"{url}@{ref}"
    domain = "test"

    # Compute hash-based directory name (matching _compute_destination_path logic)
    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

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
    key = f"{url}@{ref}"
    domain = "test"

    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

    # Create repo directory
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = datetime.now() - timedelta(days=2)
    fetch_head.touch()
    os.utime(fetch_head, (old_time.timestamp(), old_time.timestamp()))

    # Track command call counts to make first call fail, subsequent calls succeed
    call_counts: dict[str, int] = {}

    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        # Determine which command this is
        cmd_type = None
        if "rev-parse" in cmd:
            cmd_type = "rev-parse"
        elif "stash" in cmd:
            cmd_type = "stash"
        elif "fetch" in cmd:
            cmd_type = "fetch"
        elif "reset" in cmd:
            cmd_type = "reset"
        elif "clone" in cmd:
            cmd_type = "clone"

        # Track call count for this command type
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # Fail on first call to the specified command, succeed on subsequent calls
        if cmd_type == fail_command and call_counts[cmd_type] == 1:
            raise cv.Invalid(error_message)

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
    key = f"{url}@{ref}"
    domain = "test"

    h = hashlib.new("sha256")
    h.update(key.encode())
    repo_dir = tmp_path / ".esphome" / domain / h.hexdigest()[:8]

    # Create repo directory
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = datetime.now() - timedelta(days=2)
    fetch_head.touch()
    os.utime(fetch_head, (old_time.timestamp(), old_time.timestamp()))

    # Mock git command to fail on clone (simulating network failure during recovery)
    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        if "rev-parse" in cmd:
            # First time fails (broken repo)
            raise cv.Invalid(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if "clone" in cmd:
            # Clone also fails (recovery fails)
            raise cv.Invalid("fatal: unable to access repository")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise after one recovery attempt fails
    with pytest.raises(cv.Invalid, match="fatal: unable to access repository"):
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
