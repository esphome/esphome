"""Tests for the external_components skip_update functionality."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

from esphome.components.external_components import do_external_components_pass
from esphome.const import (
    CONF_EXTERNAL_COMPONENTS,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_URL,
    TYPE_GIT,
)


@patch("esphome.git.clone_or_update")
@patch("esphome.loader.install_meta_finder")
def test_external_components_skip_update_true(
    mock_install_meta: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that external components don't update when skip_update=True."""
    # Setup mocks
    test_path = Path("/tmp/test/components")
    test_path.mkdir(parents=True, exist_ok=True)
    mock_clone_or_update.return_value = (test_path.parent, None)

    config: dict[str, Any] = {
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

    # Call with skip_update=True
    do_external_components_pass(config, skip_update=True)

    # Verify clone_or_update was called with refresh=None
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] is None


@patch("esphome.git.clone_or_update")
@patch("esphome.loader.install_meta_finder")
def test_external_components_skip_update_false(
    mock_install_meta: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that external components update when skip_update=False."""
    # Setup mocks
    test_path = Path("/tmp/test/components")
    test_path.mkdir(parents=True, exist_ok=True)
    mock_clone_or_update.return_value = (test_path.parent, None)

    config: dict[str, Any] = {
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

    # Call with skip_update=False
    do_external_components_pass(config, skip_update=False)

    # Verify clone_or_update was called with actual refresh value
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] == "1d"


@patch("esphome.git.clone_or_update")
@patch("esphome.loader.install_meta_finder")
def test_external_components_default_no_skip(
    mock_install_meta: MagicMock, mock_clone_or_update: MagicMock
) -> None:
    """Test that external components update by default when skip_update not specified."""
    # Setup mocks
    test_path = Path("/tmp/test/components")
    test_path.mkdir(parents=True, exist_ok=True)
    mock_clone_or_update.return_value = (test_path.parent, None)

    config: dict[str, Any] = {
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

    # Call without skip_update parameter
    do_external_components_pass(config)

    # Verify clone_or_update was called with actual refresh value
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] == "1d"
