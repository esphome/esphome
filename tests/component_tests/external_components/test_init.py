"""Tests for the external_components skip-update behavior driven by CORE.skip_external_update."""

from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

from esphome.components.external_components import (
    CONF_DOC_URL,
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
from esphome.loader import _EXTERNAL_DOC_URLS_KEY


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


def _make_doc_url_config(
    tmp_path: Path,
    component_names: list[str],
    *,
    components: Any = "all",
    doc_url: str | None = "https://example.com/docs",
) -> dict[str, Any]:
    components_dir = tmp_path / "components"
    components_dir.mkdir()
    for name in component_names:
        component_dir = components_dir / name
        component_dir.mkdir()
        (component_dir / "__init__.py").write_text("# Test component")

    source: dict[str, Any] = {
        CONF_SOURCE: {
            "type": TYPE_GIT,
            CONF_URL: "https://github.com/test/components",
        },
        CONF_REFRESH: "1d",
        "components": components,
    }
    if doc_url is not None:
        source[CONF_DOC_URL] = doc_url

    return {CONF_EXTERNAL_COMPONENTS: [source]}


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


def test_external_components_doc_url_populates_core_data(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
) -> None:
    """`doc_url` with `components: all` registers a doc URL for every discovered component."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_doc_url_config(tmp_path, ["foo", "bar"])

    do_external_components_pass(config)

    data = CORE.data[_EXTERNAL_DOC_URLS_KEY]
    assert data.urls == {
        "foo": "https://example.com/docs",
        "bar": "https://example.com/docs",
    }


def test_external_components_doc_url_absent_by_default(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
) -> None:
    """Without `doc_url`, nothing is registered and the feature is fully opt-in."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_doc_url_config(tmp_path, ["foo"], doc_url=None)

    do_external_components_pass(config)

    assert _EXTERNAL_DOC_URLS_KEY not in CORE.data


def test_external_components_doc_url_explicit_components_list(
    tmp_path: Path,
    mock_clone_or_update: MagicMock,
    mock_install_meta_finder: MagicMock,
) -> None:
    """With an explicit `components` list, only those names are registered."""
    mock_clone_or_update.return_value = (tmp_path, None)
    config = _make_doc_url_config(tmp_path, ["foo", "bar"], components=["foo"])

    do_external_components_pass(config)

    data = CORE.data[_EXTERNAL_DOC_URLS_KEY]
    assert data.urls == {"foo": "https://example.com/docs"}
