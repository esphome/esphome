"""Tests for the packages component skip_update functionality."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

from esphome.components.packages import do_packages_pass
from esphome.const import CONF_FILES, CONF_PACKAGES, CONF_REFRESH, CONF_URL
from esphome.util import OrderedDict


def test_packages_skip_update_true(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_load_yaml: MagicMock
) -> None:
    """Test that packages don't update when skip_update=True."""
    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

    # Create the test yaml file
    test_file = tmp_path / "test.yaml"
    test_file.write_text("sensor: []")

    # Set mock_load_yaml to return some valid config
    mock_load_yaml.return_value = OrderedDict({"sensor": []})

    config: dict[str, Any] = {
        CONF_PACKAGES: {
            "test_package": {
                CONF_URL: "https://github.com/test/repo",
                CONF_FILES: ["test.yaml"],
                CONF_REFRESH: "1d",
            }
        }
    }

    # Call with skip_update=True
    do_packages_pass(config, skip_update=True)

    # Verify clone_or_update was called with NEVER_REFRESH
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    from esphome import git

    assert call_args.kwargs["refresh"] == git.NEVER_REFRESH


def test_packages_skip_update_false(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_load_yaml: MagicMock
) -> None:
    """Test that packages update when skip_update=False."""
    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

    # Create the test yaml file
    test_file = tmp_path / "test.yaml"
    test_file.write_text("sensor: []")

    # Set mock_load_yaml to return some valid config
    mock_load_yaml.return_value = OrderedDict({"sensor": []})

    config: dict[str, Any] = {
        CONF_PACKAGES: {
            "test_package": {
                CONF_URL: "https://github.com/test/repo",
                CONF_FILES: ["test.yaml"],
                CONF_REFRESH: "1d",
            }
        }
    }

    # Call with skip_update=False (default)
    do_packages_pass(config, skip_update=False)

    # Verify clone_or_update was called with actual refresh value
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    from esphome.core import TimePeriodSeconds

    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)


def test_packages_default_no_skip(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_load_yaml: MagicMock
) -> None:
    """Test that packages update by default when skip_update not specified."""
    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

    # Create the test yaml file
    test_file = tmp_path / "test.yaml"
    test_file.write_text("sensor: []")

    # Set mock_load_yaml to return some valid config
    mock_load_yaml.return_value = OrderedDict({"sensor": []})

    config: dict[str, Any] = {
        CONF_PACKAGES: {
            "test_package": {
                CONF_URL: "https://github.com/test/repo",
                CONF_FILES: ["test.yaml"],
                CONF_REFRESH: "1d",
            }
        }
    }

    # Call without skip_update parameter
    do_packages_pass(config)

    # Verify clone_or_update was called with actual refresh value
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    from esphome.core import TimePeriodSeconds

    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)
