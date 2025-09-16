"""Tests for the external_components skip_update functionality."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

from esphome.components.external_components import do_external_components_pass
from esphome.const import (
    CONF_EXTERNAL_COMPONENTS,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_URL,
    TYPE_GIT,
)


def test_external_components_skip_update_true(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_install_meta_finder: MagicMock
) -> None:
    """Test that external components don't update when skip_update=True."""
    # Create a components directory structure
    components_dir = tmp_path / "components"
    components_dir.mkdir()

    # Create a test component
    test_component_dir = components_dir / "test_component"
    test_component_dir.mkdir()
    (test_component_dir / "__init__.py").write_text("# Test component")

    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

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

    # Verify clone_or_update was called with NEVER_REFRESH
    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    from esphome import git

    assert call_args.kwargs["refresh"] == git.NEVER_REFRESH


def test_external_components_skip_update_false(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_install_meta_finder: MagicMock
) -> None:
    """Test that external components update when skip_update=False."""
    # Create a components directory structure
    components_dir = tmp_path / "components"
    components_dir.mkdir()

    # Create a test component
    test_component_dir = components_dir / "test_component"
    test_component_dir.mkdir()
    (test_component_dir / "__init__.py").write_text("# Test component")

    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

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
    from esphome.core import TimePeriodSeconds

    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)


def test_external_components_default_no_skip(
    tmp_path: Path, mock_clone_or_update: MagicMock, mock_install_meta_finder: MagicMock
) -> None:
    """Test that external components update by default when skip_update not specified."""
    # Create a components directory structure
    components_dir = tmp_path / "components"
    components_dir.mkdir()

    # Create a test component
    test_component_dir = components_dir / "test_component"
    test_component_dir.mkdir()
    (test_component_dir / "__init__.py").write_text("# Test component")

    # Set up mock to return our tmp_path
    mock_clone_or_update.return_value = (tmp_path, None)

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
    from esphome.core import TimePeriodSeconds

    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)
