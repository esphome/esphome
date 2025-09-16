"""Tests for git.py module."""

from datetime import datetime, timedelta
from pathlib import Path
from unittest.mock import Mock, patch

from esphome import git
from esphome.core import TimePeriodSeconds


def test_clone_or_update_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that NEVER_REFRESH skips updates for existing repos."""
    # Create a fake git repo directory
    repo_dir = tmp_path / ".esphome" / "external_components" / "test" / "test_repo"
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Create FETCH_HEAD file with current timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")

    # Mock _compute_destination_path to return our test directory
    with patch.object(git, "_compute_destination_path", return_value=repo_dir):
        # Call with NEVER_REFRESH
        result_dir, revert = git.clone_or_update(
            url="https://github.com/test/repo",
            ref=None,
            refresh=git.NEVER_REFRESH,
            domain="test",
        )

        # Should NOT call git commands since NEVER_REFRESH and repo exists
        mock_run_git_command.assert_not_called()
        assert result_dir == repo_dir
        assert revert is None


def test_clone_or_update_with_refresh_updates_old_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh triggers update for old repos."""
    # Create a fake git repo directory
    repo_dir = tmp_path / ".esphome" / "external_components" / "test" / "test_repo"
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Create FETCH_HEAD file with old timestamp (2 days ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = datetime.now() - timedelta(days=2)
    fetch_head.touch()  # Create the file
    # Set modification time to 2 days ago
    import os

    os.utime(fetch_head, (old_time.timestamp(), old_time.timestamp()))

    # Mock _compute_destination_path to return our test directory
    with patch.object(git, "_compute_destination_path", return_value=repo_dir):
        # Mock git command responses
        mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

        # Call with refresh=1d (1 day)
        refresh = TimePeriodSeconds(days=1)
        result_dir, revert = git.clone_or_update(
            url="https://github.com/test/repo",
            ref=None,
            refresh=refresh,
            domain="test",
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
    # Create a fake git repo directory
    repo_dir = tmp_path / ".esphome" / "external_components" / "test" / "test_repo"
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Create FETCH_HEAD file with recent timestamp (1 hour ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = datetime.now() - timedelta(hours=1)
    fetch_head.touch()  # Create the file
    # Set modification time to 1 hour ago
    import os

    os.utime(fetch_head, (recent_time.timestamp(), recent_time.timestamp()))

    # Mock _compute_destination_path to return our test directory
    with patch.object(git, "_compute_destination_path", return_value=repo_dir):
        # Call with refresh=1d (1 day)
        refresh = TimePeriodSeconds(days=1)
        result_dir, revert = git.clone_or_update(
            url="https://github.com/test/repo",
            ref=None,
            refresh=refresh,
            domain="test",
        )

        # Should NOT call git fetch since repo is fresh
        mock_run_git_command.assert_not_called()
        assert result_dir == repo_dir
        assert revert is None


def test_clone_or_update_clones_missing_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that missing repos are cloned regardless of refresh setting."""
    # Create base directory but not the repo itself
    base_dir = tmp_path / ".esphome" / "external_components" / "test"
    base_dir.mkdir(parents=True)
    repo_dir = base_dir / "test_repo"

    # Mock _compute_destination_path to return our test directory
    with patch.object(git, "_compute_destination_path", return_value=repo_dir):
        # Test with NEVER_REFRESH - should still clone since repo doesn't exist
        result_dir, revert = git.clone_or_update(
            url="https://github.com/test/repo",
            ref=None,
            refresh=git.NEVER_REFRESH,
            domain="test",
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
    # Create a fake git repo directory
    repo_dir = tmp_path / ".esphome" / "external_components" / "test" / "test_repo"
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Create FETCH_HEAD file with very recent timestamp (1 second ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = datetime.now() - timedelta(seconds=1)
    fetch_head.touch()  # Create the file
    # Set modification time to 1 second ago
    import os

    os.utime(fetch_head, (recent_time.timestamp(), recent_time.timestamp()))

    # Mock _compute_destination_path to return our test directory
    with patch.object(git, "_compute_destination_path", return_value=repo_dir):
        # Mock git command responses
        mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

        # Call with refresh=None (default behavior)
        result_dir, revert = git.clone_or_update(
            url="https://github.com/test/repo",
            ref=None,
            refresh=None,
            domain="test",
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
