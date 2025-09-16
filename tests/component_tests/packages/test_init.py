"""Tests for the packages component skip_update functionality."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

from esphome.components.packages import do_packages_pass
from esphome.const import CONF_FILES, CONF_PACKAGES, CONF_REFRESH, CONF_URL
from esphome.util import OrderedDict


@patch("esphome.git.clone_or_update")
@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
def test_packages_skip_update_true(
    mock_is_file: MagicMock, mock_load_yaml: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that packages don't update when skip_update=True."""
    # Setup mocks
    mock_clone_or_update.return_value = (Path("/tmp/test"), None)
    mock_is_file.return_value = True
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

    # Verify clone_or_update was called with refresh=None
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] is None


@patch("esphome.git.clone_or_update")
@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
def test_packages_skip_update_false(
    mock_is_file: MagicMock, mock_load_yaml: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that packages update when skip_update=False."""
    # Setup mocks
    mock_clone_or_update.return_value = (Path("/tmp/test"), None)
    mock_is_file.return_value = True
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
    assert call_args.kwargs["refresh"] == "1d"


@patch("esphome.git.clone_or_update")
@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
def test_packages_default_no_skip(
    mock_is_file: MagicMock, mock_load_yaml: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that packages update by default when skip_update not specified."""
    # Setup mocks
    mock_clone_or_update.return_value = (Path("/tmp/test"), None)
    mock_is_file.return_value = True
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
    assert call_args.kwargs["refresh"] == "1d"
