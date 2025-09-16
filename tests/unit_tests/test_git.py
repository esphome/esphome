"""Tests for git.py module."""

from datetime import datetime, timedelta
from unittest.mock import MagicMock, Mock, patch

from esphome import git
from esphome.core import TimePeriodSeconds


@patch("esphome.git.run_git_command")
@patch("pathlib.Path.is_dir")
def test_clone_or_update_with_none_refresh_no_update(
    mock_is_dir: MagicMock, mock_run_git: MagicMock
) -> None:
    """Test that refresh=None skips updates for existing repos."""
    # Setup - repo already exists
    mock_is_dir.return_value = True

    # Mock file timestamps
    with patch("pathlib.Path.exists") as mock_exists:
        mock_exists.return_value = True
        with patch("pathlib.Path.stat") as mock_stat:
            mock_stat_result = Mock()
            mock_stat_result.st_mtime = datetime.now().timestamp()
            mock_stat.return_value = mock_stat_result

            # Call with refresh=None
            repo_dir, revert = git.clone_or_update(
                url="https://github.com/test/repo",
                ref=None,
                refresh=None,
                domain="test",
            )

            # Should NOT call git fetch or any update commands
            mock_run_git.assert_not_called()
            assert revert is None


@patch("esphome.git.run_git_command")
@patch("pathlib.Path.is_dir")
def test_clone_or_update_with_refresh_updates_old_repo(
    mock_is_dir: MagicMock, mock_run_git: MagicMock
) -> None:
    """Test that refresh triggers update for old repos."""
    # Setup - repo already exists
    mock_is_dir.return_value = True
    mock_run_git.return_value = "abc123"  # mock SHA

    # Mock file timestamps - 2 days old
    with patch("pathlib.Path.exists") as mock_exists:
        mock_exists.return_value = True
        with patch("pathlib.Path.stat") as mock_stat:
            mock_stat_result = Mock()
            old_time = datetime.now() - timedelta(days=2)
            mock_stat_result.st_mtime = old_time.timestamp()
            mock_stat.return_value = mock_stat_result

            # Call with refresh=1d (1 day)
            refresh = TimePeriodSeconds(days=1)
            repo_dir, revert = git.clone_or_update(
                url="https://github.com/test/repo",
                ref=None,
                refresh=refresh,
                domain="test",
            )

            # Should call git fetch and update commands
            assert mock_run_git.called
            # Check for fetch command
            fetch_calls = [
                call for call in mock_run_git.call_args_list if "fetch" in str(call)
            ]
            assert len(fetch_calls) > 0


@patch("esphome.git.run_git_command")
@patch("pathlib.Path.is_dir")
def test_clone_or_update_with_refresh_skips_fresh_repo(
    mock_is_dir: MagicMock, mock_run_git: MagicMock
) -> None:
    """Test that refresh doesn't update fresh repos."""
    # Setup - repo already exists
    mock_is_dir.return_value = True

    # Mock file timestamps - 1 hour old
    with patch("pathlib.Path.exists") as mock_exists:
        mock_exists.return_value = True
        with patch("pathlib.Path.stat") as mock_stat:
            mock_stat_result = Mock()
            recent_time = datetime.now() - timedelta(hours=1)
            mock_stat_result.st_mtime = recent_time.timestamp()
            mock_stat.return_value = mock_stat_result

            # Call with refresh=1d (1 day)
            refresh = TimePeriodSeconds(days=1)
            repo_dir, revert = git.clone_or_update(
                url="https://github.com/test/repo",
                ref=None,
                refresh=refresh,
                domain="test",
            )

            # Should NOT call git fetch since repo is fresh
            mock_run_git.assert_not_called()
            assert revert is None


@patch("esphome.git.run_git_command")
@patch("pathlib.Path.is_dir")
def test_clone_or_update_clones_missing_repo(
    mock_is_dir: MagicMock, mock_run_git: MagicMock
) -> None:
    """Test that missing repos are cloned regardless of refresh setting."""
    # Setup - repo doesn't exist
    mock_is_dir.return_value = False

    # Test with refresh=None
    repo_dir, revert = git.clone_or_update(
        url="https://github.com/test/repo",
        ref=None,
        refresh=None,
        domain="test",
    )

    # Should call git clone
    assert mock_run_git.called
    clone_calls = [call for call in mock_run_git.call_args_list if "clone" in str(call)]
    assert len(clone_calls) > 0

    # Reset mock
    mock_run_git.reset_mock()

    # Test with refresh=1d
    mock_is_dir.return_value = False
    refresh = TimePeriodSeconds(days=1)
    repo_dir, revert = git.clone_or_update(
        url="https://github.com/test/repo2",
        ref=None,
        refresh=refresh,
        domain="test",
    )

    # Should still call git clone
    assert mock_run_git.called
    clone_calls = [call for call in mock_run_git.call_args_list if "clone" in str(call)]
    assert len(clone_calls) > 0
