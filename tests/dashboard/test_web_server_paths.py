"""Tests for dashboard web_server Path-related functionality."""

from __future__ import annotations

import gzip
import os
from pathlib import Path
from unittest.mock import MagicMock, patch

from esphome.dashboard import web_server


def test_get_base_frontend_path_production() -> None:
    """Test get_base_frontend_path in production mode."""
    mock_module = MagicMock()
    mock_module.where.return_value = Path("/usr/local/lib/esphome_dashboard")

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.dict("sys.modules", {"esphome_dashboard": mock_module}),
    ):
        result = web_server.get_base_frontend_path()
        assert result == Path("/usr/local/lib/esphome_dashboard")
        mock_module.where.assert_called_once()


def test_get_base_frontend_path_dev_mode() -> None:
    """Test get_base_frontend_path in development mode."""
    test_path = "/home/user/esphome/dashboard"

    with patch.dict(os.environ, {"ESPHOME_DASHBOARD_DEV": test_path}):
        result = web_server.get_base_frontend_path()

        # The function uses Path.resolve() which resolves symlinks
        # The actual function adds "/" to the path, so we simulate that
        test_path_with_slash = test_path if test_path.endswith("/") else test_path + "/"
        expected = (
            Path(os.getcwd()) / test_path_with_slash / "esphome_dashboard"
        ).resolve()
        assert result == expected


def test_get_base_frontend_path_dev_mode_with_trailing_slash() -> None:
    """Test get_base_frontend_path in dev mode with trailing slash."""
    test_path = "/home/user/esphome/dashboard/"

    with patch.dict(os.environ, {"ESPHOME_DASHBOARD_DEV": test_path}):
        result = web_server.get_base_frontend_path()

        # The function uses Path.resolve() which resolves symlinks
        expected = (Path.cwd() / test_path / "esphome_dashboard").resolve()
        assert result == expected


def test_get_base_frontend_path_dev_mode_relative_path() -> None:
    """Test get_base_frontend_path with relative dev path."""
    test_path = "./dashboard"

    with patch.dict(os.environ, {"ESPHOME_DASHBOARD_DEV": test_path}):
        result = web_server.get_base_frontend_path()

        # The function uses Path.resolve() which resolves symlinks
        # The actual function adds "/" to the path, so we simulate that
        test_path_with_slash = test_path if test_path.endswith("/") else test_path + "/"
        expected = (
            Path(os.getcwd()) / test_path_with_slash / "esphome_dashboard"
        ).resolve()
        assert result == expected
        assert result.is_absolute()


def test_get_static_path_single_component() -> None:
    """Test get_static_path with single path component."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        result = web_server.get_static_path("file.js")

        assert result == Path("/base/frontend") / "static" / "file.js"


def test_get_static_path_multiple_components() -> None:
    """Test get_static_path with multiple path components."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        result = web_server.get_static_path("js", "esphome", "index.js")

        assert (
            result == Path("/base/frontend") / "static" / "js" / "esphome" / "index.js"
        )


def test_get_static_path_empty_args() -> None:
    """Test get_static_path with no arguments."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        result = web_server.get_static_path()

        assert result == Path("/base/frontend") / "static"


def test_get_static_path_with_pathlib_path() -> None:
    """Test get_static_path with Path objects."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        path_obj = Path("js") / "app.js"
        result = web_server.get_static_path(str(path_obj))

        assert result == Path("/base/frontend") / "static" / "js" / "app.js"


def test_get_static_file_url_production() -> None:
    """Test get_static_file_url in production mode."""
    web_server.get_static_file_url.cache_clear()
    mock_module = MagicMock()
    mock_path = MagicMock(spec=Path)
    mock_path.read_bytes.return_value = b"test content"

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.dict("sys.modules", {"esphome_dashboard": mock_module}),
        patch("esphome.dashboard.web_server.get_static_path") as mock_get_path,
    ):
        mock_get_path.return_value = mock_path
        result = web_server.get_static_file_url("js/app.js")
        assert result.startswith("./static/js/app.js?hash=")


def test_get_static_file_url_dev_mode() -> None:
    """Test get_static_file_url in development mode."""
    with patch.dict(os.environ, {"ESPHOME_DASHBOARD_DEV": "/dev/path"}):
        web_server.get_static_file_url.cache_clear()
        result = web_server.get_static_file_url("js/app.js")

        assert result == "./static/js/app.js"


def test_get_static_file_url_index_js_special_case() -> None:
    """Test get_static_file_url replaces index.js with entrypoint."""
    web_server.get_static_file_url.cache_clear()
    mock_module = MagicMock()
    mock_module.entrypoint.return_value = "main.js"

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.dict("sys.modules", {"esphome_dashboard": mock_module}),
    ):
        result = web_server.get_static_file_url("js/esphome/index.js")
        assert result == "./static/js/esphome/main.js"


def test_load_file_path(tmp_path: Path) -> None:
    """Test loading a file."""
    test_file = tmp_path / "test.txt"
    test_file.write_bytes(b"test content")

    with open(test_file, "rb") as f:
        content = f.read()
    assert content == b"test content"


def test_load_file_compressed_path(tmp_path: Path) -> None:
    """Test loading a compressed file."""
    test_file = tmp_path / "test.txt.gz"

    with gzip.open(test_file, "wb") as gz:
        gz.write(b"compressed content")

    with gzip.open(test_file, "rb") as gz:
        content = gz.read()
    assert content == b"compressed content"


def test_path_normalization_in_static_path() -> None:
    """Test that paths are normalized correctly."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        # Test with separate components
        result1 = web_server.get_static_path("js", "app.js")
        result2 = web_server.get_static_path("js", "app.js")

        assert result1 == result2
        assert result1 == Path("/base/frontend") / "static" / "js" / "app.js"


def test_windows_path_handling() -> None:
    """Test handling of Windows-style paths."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path(r"C:\Program Files\esphome\frontend")

        result = web_server.get_static_path("js", "app.js")

        # Path should handle this correctly on the platform
        expected = (
            Path(r"C:\Program Files\esphome\frontend") / "static" / "js" / "app.js"
        )
        assert result == expected


def test_path_with_special_characters() -> None:
    """Test paths with special characters."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/frontend")

        result = web_server.get_static_path("js-modules", "app_v1.0.js")

        assert (
            result == Path("/base/frontend") / "static" / "js-modules" / "app_v1.0.js"
        )


def test_path_with_spaces() -> None:
    """Test paths with spaces."""
    with patch("esphome.dashboard.web_server.get_base_frontend_path") as mock_base:
        mock_base.return_value = Path("/base/my frontend")

        result = web_server.get_static_path("my js", "my app.js")

        assert result == Path("/base/my frontend") / "static" / "my js" / "my app.js"
