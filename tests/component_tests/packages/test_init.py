"""Tests for the packages skip-update behavior driven by CORE.skip_external_update."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

from esphome.components.packages import do_packages_pass
from esphome.const import CONF_FILES, CONF_PACKAGES, CONF_REFRESH, CONF_URL
from esphome.core import CORE, TimePeriodSeconds
from esphome.util import OrderedDict


def _make_config() -> dict[str, Any]:
    return {
        CONF_PACKAGES: {
            "test_package": {
                CONF_URL: "https://github.com/test/repo",
                CONF_FILES: ["test.yaml"],
                CONF_REFRESH: "1d",
            }
        }
    }


def test_packages_skip_update_via_core_flag(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_load_yaml: MagicMock,
) -> None:
    """When CORE.skip_external_update is True, refresh is still passed through;
    git.clone_or_update itself short-circuits the actual fetch."""
    mock_clone_or_update.return_value = (tmp_path, None)

    test_file = tmp_path / "test.yaml"
    test_file.write_text("sensor: []")
    mock_load_yaml.return_value = OrderedDict({"sensor": []})

    config = _make_config()

    CORE.skip_external_update = True
    do_packages_pass(config, command_line_substitutions={})

    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    # Refresh is passed through verbatim — the global flag is enforced inside git.clone_or_update.
    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)


def test_packages_normal_refresh(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_load_yaml: MagicMock,
) -> None:
    """When CORE.skip_external_update is False, the configured refresh value is used."""
    mock_clone_or_update.return_value = (tmp_path, None)

    test_file = tmp_path / "test.yaml"
    test_file.write_text("sensor: []")
    mock_load_yaml.return_value = OrderedDict({"sensor": []})

    config = _make_config()

    do_packages_pass(config, command_line_substitutions={})

    mock_clone_or_update.assert_called_once()
    call_args = mock_clone_or_update.call_args
    assert call_args.kwargs["refresh"] == TimePeriodSeconds(days=1)
