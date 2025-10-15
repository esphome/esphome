"""Tests for git.py module."""

from datetime import datetime, timedelta
import os
from pathlib import Path
from unittest.mock import Mock

import pytest

from esphome import git
import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds


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

    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        # Determine which command this is
        cmd_type = _get_git_command_type(cmd)

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
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Mock git command to fail on clone (simulating network failure during recovery)
    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            # First time fails (broken repo)
            raise cv.Invalid(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if cmd_type == "clone":
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
    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        cmd_type = _get_git_command_type(cmd)

        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # First attempt: rev-parse fails (broken repo)
        if cmd_type == "rev-parse" and call_counts[cmd_type] == 1:
            raise cv.Invalid(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )

        # Recovery: clone succeeds
        if cmd_type == "clone":
            return ""

        # Recovery: fetch for ref checkout fails
        # This happens in the clone path when ref is not None (line 80 in git.py)
        if cmd_type == "fetch" and call_counts[cmd_type] == 1:
            raise cv.Invalid("fatal: couldn't find remote ref main")

        # Default success
        return "abc123" if cmd_type == "rev-parse" else ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise on the fetch during recovery (when _recover_broken=False)
    # This tests the critical "if not _recover_broken: raise" path
    with pytest.raises(cv.Invalid, match="fatal: couldn't find remote ref main"):
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
    def git_command_side_effect(cmd: list[str], cwd: str | None = None) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "stash":
            # Always fails
            raise cv.Invalid("fatal: unable to write new index file")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Mock shutil.rmtree and test
    # Should raise on the second attempt when _recover_broken=False
    # This hits the "if not _recover_broken: raise" path
    with (
        unittest.mock.patch("esphome.git.shutil.rmtree", side_effect=mock_rmtree),
        pytest.raises(cv.Invalid, match="fatal: unable to write new index file"),
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
