"""Tests for the external_components config pass."""

import logging
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

import pytest

from esphome.components.external_components import do_external_components_pass
from esphome.const import (
    CONF_EXTERNAL_COMPONENTS,
    CONF_PATH,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_URL,
    TYPE_GIT,
    TYPE_LOCAL,
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


def test_external_components_logs_built_in_override(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A source that provides a component with the same name as a built-in one logs an info message."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)

    for name in ("gpio", "some_custom_component"):
        component_dir = tmp_path / "components" / name
        component_dir.mkdir()
        (component_dir / "__init__.py").write_text("# Test component")

    with caplog.at_level(logging.INFO):
        do_external_components_pass(config)

    assert (
        "External components are overriding built-in components:\n"
        "  source: https://github.com/test/components\n"
        "  components: gpio" in caplog.text
    )
    assert "some_custom_component" not in caplog.text


def test_external_components_override_log_includes_ref(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A git source with a ref logs the ref appended to the url."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)
    config[CONF_EXTERNAL_COMPONENTS][0][CONF_SOURCE] = "github://test/components@main"

    component_dir = tmp_path / "components" / "gpio"
    component_dir.mkdir()
    (component_dir / "__init__.py").write_text("# Test component")

    with caplog.at_level(logging.INFO):
        do_external_components_pass(config)

    assert "  source: https://github.com/test/components.git@main\n" in caplog.text


def test_external_components_override_log_includes_git_path(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A git source with a subdirectory path logs the path after the url."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)
    config[CONF_EXTERNAL_COMPONENTS][0][CONF_SOURCE][CONF_PATH] = "components"

    component_dir = tmp_path / "components" / "gpio"
    component_dir.mkdir()
    (component_dir / "__init__.py").write_text("# Test component")

    with caplog.at_level(logging.INFO):
        do_external_components_pass(config)

    assert "  source: https://github.com/test/components (components)\n" in caplog.text


def test_external_components_override_log_local_source(
    tmp_path: Path,
    mock_install_meta_finder: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A local source logs its resolved path."""
    components_dir = tmp_path / "my_components"
    gpio_dir = components_dir / "gpio"
    gpio_dir.mkdir(parents=True)
    (gpio_dir / "__init__.py").write_text("# Test component")

    CORE.config_path = tmp_path / "dummy.yaml"
    config = {
        CONF_EXTERNAL_COMPONENTS: [
            {CONF_SOURCE: {"type": TYPE_LOCAL, CONF_PATH: "my_components"}}
        ]
    }

    with caplog.at_level(logging.INFO):
        do_external_components_pass(config)

    assert f"  source: {components_dir}\n" in caplog.text
    assert "  components: gpio" in caplog.text


def test_external_components_no_override_no_log(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A source that only provides components not shipped with ESPHome logs nothing."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_config(tmp_path)

    with caplog.at_level(logging.INFO):
        do_external_components_pass(config)

    assert "are overriding built-in components" not in caplog.text
