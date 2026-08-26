"""Tests for the external_components skip-update behavior driven by CORE.skip_external_update."""

from pathlib import Path
import time
from typing import Any
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome.components.external_components import (
    _check_for_merged_prs,
    do_external_components_pass,
)
from esphome.const import (
    CONF_EXTERNAL_COMPONENTS,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_URL,
    TYPE_GIT,
)
from esphome.core import CORE, TimePeriodSeconds


def _make_config(tmp_path: Path) -> dict[str, Any]:
    components_dir = tmp_path / "components"
    components_dir.mkdir()
    test_component_dir = components_dir / "test_component"
    test_component_dir.mkdir()
    (test_component_dir / "__init__.py").write_text("# Test component")

    return {
        CONF_EXTERNAL_COMPONENTS: [
            {
                CONF_SOURCE: {
                    "type": TYPE_GIT,
                    CONF_URL: "https://github.com/test/components",
                },
                CONF_REFRESH: "1d",
                "components": "all",
            }
        ]
    }


def test_external_components_skip_update_via_core_flag(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
) -> None:
    """When CORE.skip_external_update is True, refresh is still passed through;
    git.clone_or_update itself short-circuits the actual fetch."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)

    CORE.skip_external_update = True
    do_external_components_pass(config)

    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    # Refresh is passed through verbatim — the global flag is enforced inside git.clone_or_update.
    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)


def test_external_components_normal_refresh(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
) -> None:
    """When CORE.skip_external_update is False, the configured refresh value is used."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)

    do_external_components_pass(config)

    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)


# Tests for _check_for_merged_prs


@pytest.fixture
def mock_cache_file(tmp_path: Path) -> Path:
    """Create a temporary cache file path for testing."""
    cache_file = tmp_path / ".merged_prs_cache"
    with patch.object(CORE, "relative_internal_path", return_value=cache_file):
        yield cache_file


@pytest.fixture
def mock_stale_stat(mock_cache_file: Path):
    """Fixture to mock os.stat to make cache file appear stale (>1 hour old)."""
    import os

    old_time = time.time() - 3700  # >1 hour ago
    original_stat = os.stat

    def mock_stat_func(path, *args, **kwargs):
        result = original_stat(path, *args, **kwargs)
        if str(path) == str(mock_cache_file):
            # Create a new stat result with modified mtime
            return os.stat_result(
                (
                    result.st_mode,
                    result.st_ino,
                    result.st_dev,
                    result.st_nlink,
                    result.st_uid,
                    result.st_gid,
                    result.st_size,
                    result.st_atime,
                    old_time,  # Modified mtime
                    result.st_ctime,
                )
            )
        return result

    with patch("os.stat", side_effect=mock_stat_func):
        yield


def test_check_for_merged_prs_empty_list(
    mock_cache_file: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Test _check_for_merged_prs with empty PR list."""
    _check_for_merged_prs([])

    # Should create empty cache file but not log warnings
    assert mock_cache_file.exists()
    assert mock_cache_file.read_text() == ""
    assert "merged and released" not in caplog.text


def test_check_for_merged_prs_fresh_cache_no_check(
    mock_cache_file: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that fresh cache prevents API calls."""
    # Create a fresh cache file with one PR
    mock_cache_file.write_text("12345\n")

    pr_srcs = [("12345", ["component1"]), ("67890", ["component2"])]

    with patch("esphome.components.external_components.requests.post") as mock_post:
        _check_for_merged_prs(pr_srcs)

        # Fresh cache should not trigger API calls
        mock_post.assert_not_called()

    # Should warn about the cached PR
    assert "github://PR#12345" in caplog.text
    assert "merged and released" in caplog.text
    assert "component1" in caplog.text


def test_check_for_merged_prs_stale_cache_checks_new_prs(
    mock_cache_file: Path, mock_stale_stat, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that stale cache triggers checks for new PRs."""
    # Create a stale cache file (>1 hour old)
    mock_cache_file.write_text("12345\n")

    pr_srcs = [("12345", ["component1"]), ("67890", ["component2"])]

    # Mock the batch API response
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"67890": {"status": "merged"}}

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

        # Should call API with only PR 67890 (not already in cache)
        mock_post.assert_called_once()
        call_args = mock_post.call_args
        assert call_args.kwargs["json"]["pr_numbers"] == ["67890"]

    # Cache should now contain both PRs
    cache_contents = mock_cache_file.read_text()
    assert "12345" in cache_contents
    assert "67890" in cache_contents

    # Should warn about both PRs
    assert "github://PR#12345" in caplog.text
    assert "github://PR#67890" in caplog.text


def test_check_for_merged_prs_stale_cache_pr_not_merged(
    mock_cache_file: Path, mock_stale_stat, caplog: pytest.LogCaptureFixture
) -> None:
    """Test stale cache with PR that is not merged."""
    # Create a stale cache file
    mock_cache_file.write_text("12345\n")

    pr_srcs = [("67890", ["component2"])]

    # Mock the batch API response - PR is not merged
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"67890": {"status": "open"}}

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

        mock_post.assert_called_once()

    # Cache should only contain original PR (67890 not added)
    cache_contents = mock_cache_file.read_text()
    assert "12345" in cache_contents
    assert "67890" not in cache_contents

    # Should not warn about unmerged PR
    assert "github://PR#67890" not in caplog.text
    # Cache file should have been touched to update timestamp
    assert mock_cache_file.exists()


def test_check_for_merged_prs_no_cache_file(
    mock_cache_file: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Test behavior when cache file doesn't exist."""
    pr_srcs = [("12345", ["component1"]), ("67890", ["component2", "component3"])]

    # Mock the batch API response - only PR 12345 is merged
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {
        "12345": {"status": "merged"},
        "67890": {"status": "open"},
    }

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

        # Should check both PRs in one API call
        mock_post.assert_called_once()
        call_args = mock_post.call_args
        assert set(call_args.kwargs["json"]["pr_numbers"]) == {"12345", "67890"}

    # Cache file should be created with merged PR
    assert mock_cache_file.exists()
    cache_contents = mock_cache_file.read_text()
    assert "12345" in cache_contents
    assert "67890" not in cache_contents

    # Should warn about merged PR with multiple components
    assert "github://PR#12345" in caplog.text
    assert "component1" in caplog.text
    assert "github://PR#67890" not in caplog.text


def test_check_for_merged_prs_multiple_merged(
    mock_cache_file: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Test multiple merged PRs are all added to cache and logged."""
    pr_srcs = [
        ("11111", ["comp1"]),
        ("22222", ["comp2"]),
        ("33333", ["comp3"]),
    ]

    # Mock the batch API response - all PRs are merged
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {
        "11111": {"status": "merged"},
        "22222": {"status": "merged"},
        "33333": {"status": "merged"},
    }

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

        # Should check all PRs in one API call
        mock_post.assert_called_once()

    # All merged PRs should be in cache
    cache_contents = mock_cache_file.read_text()
    assert "11111" in cache_contents
    assert "22222" in cache_contents
    assert "33333" in cache_contents

    # Should warn about all merged PRs
    assert "github://PR#11111" in caplog.text
    assert "github://PR#22222" in caplog.text
    assert "github://PR#33333" in caplog.text


def test_check_for_merged_prs_cache_preserves_existing_entries(
    mock_cache_file: Path, mock_stale_stat, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that existing cache entries are preserved when adding new ones."""
    # Create cache with existing entries
    mock_cache_file.write_text("11111\n22222\n")

    pr_srcs = [("33333", ["new_comp"])]

    # Mock the batch API response
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"33333": {"status": "merged"}}

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

    # Cache should contain all PRs (using set, so order doesn't matter)
    cache_contents = mock_cache_file.read_text()
    lines = cache_contents.strip().split("\n")
    assert "11111" in lines
    assert "22222" in lines
    assert "33333" in lines


def test_check_for_merged_prs_touch_cache_when_no_new_merged(
    mock_cache_file: Path,
    mock_stale_stat,
) -> None:
    """Test that cache file is touched even when no new PRs are merged."""
    # Create stale cache
    mock_cache_file.write_text("11111\n")

    pr_srcs = [("22222", ["comp"])]

    # Mock the batch API response - PR is not merged
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"22222": {"status": "open"}}

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

    # File should exist and been touched
    assert mock_cache_file.exists()
    # Content should be unchanged
    assert mock_cache_file.read_text() == "11111\n"


def test_check_for_merged_prs_api_error(
    mock_cache_file: Path, mock_stale_stat, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that API errors are handled gracefully."""
    mock_cache_file.write_text("12345\n")

    pr_srcs = [("67890", ["component2"])]

    # Mock API error response
    mock_response = Mock()
    mock_response.status_code = 500

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        # Should not raise exception
        _check_for_merged_prs(pr_srcs)

    # Cache should remain unchanged
    cache_contents = mock_cache_file.read_text()
    assert "12345" in cache_contents
    assert "67890" not in cache_contents


def test_check_for_merged_prs_empty_check_numbers(
    mock_cache_file: Path, mock_stale_stat, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that API is not called when all PRs are already in cache."""
    # Cache already contains all PRs
    mock_cache_file.write_text("12345\n67890\n")

    pr_srcs = [("12345", ["component1"]), ("67890", ["component2"])]

    with patch("esphome.components.external_components.requests.post") as mock_post:
        _check_for_merged_prs(pr_srcs)

        # Should not call API when all PRs are in cache
        mock_post.assert_not_called()

    # Should warn about both cached PRs
    assert "github://PR#12345" in caplog.text
    assert "github://PR#67890" in caplog.text


def test_check_for_merged_prs_deduplicates_cache(
    mock_cache_file: Path, mock_stale_stat
) -> None:
    """Test that cache entries are deduplicated when writing."""
    # Create cache with duplicate entries
    mock_cache_file.write_text("11111\n11111\n22222\n")

    pr_srcs = [("33333", ["comp"])]

    # Mock the batch API response
    mock_response = Mock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"33333": {"status": "merged"}}

    with patch("esphome.components.external_components.requests.post") as mock_post:
        mock_post.return_value = mock_response
        _check_for_merged_prs(pr_srcs)

    # Cache should have deduplicated entries (using set())
    cache_contents = mock_cache_file.read_text()
    lines = cache_contents.strip().split("\n")
    # Should have 3 unique entries
    assert len(set(lines)) == 3
    assert "11111" in lines
    assert "22222" in lines
    assert "33333" in lines
