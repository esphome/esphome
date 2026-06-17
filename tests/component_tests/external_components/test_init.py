"""Tests for the external_components skip-update behavior driven by CORE.skip_external_update."""

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
