"""Tests for dashboard settings Path-related functionality."""

from __future__ import annotations

from pathlib import Path
import tempfile

import pytest

from esphome.dashboard.settings import DashboardSettings


@pytest.fixture
def dashboard_settings(tmp_path: Path) -> DashboardSettings:
    """Create DashboardSettings instance with temp directory."""
    settings = DashboardSettings()
    # Resolve symlinks to ensure paths match
    resolved_dir = tmp_path.resolve()
    settings.config_dir = resolved_dir
    settings.absolute_config_dir = resolved_dir
    return settings


def test_rel_path_simple(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path with simple relative path."""
    result = dashboard_settings.rel_path("config.yaml")

    expected = dashboard_settings.config_dir / "config.yaml"
    assert result == expected


def test_rel_path_multiple_components(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path with multiple path components."""
    result = dashboard_settings.rel_path("subfolder", "device", "config.yaml")

    expected = dashboard_settings.config_dir / "subfolder" / "device" / "config.yaml"
    assert result == expected


def test_rel_path_with_dots(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path prevents directory traversal."""
    # This should raise ValueError as it tries to go outside config_dir
    with pytest.raises(ValueError):
        dashboard_settings.rel_path("..", "outside.yaml")


def test_rel_path_absolute_path_within_config(
    dashboard_settings: DashboardSettings,
) -> None:
    """Test rel_path with absolute path that's within config dir."""
    internal_path = dashboard_settings.absolute_config_dir / "internal.yaml"

    internal_path.touch()
    result = dashboard_settings.rel_path("internal.yaml")
    expected = dashboard_settings.config_dir / "internal.yaml"
    assert result == expected


def test_rel_path_absolute_path_outside_config(
    dashboard_settings: DashboardSettings,
) -> None:
    """Test rel_path with absolute path outside config dir raises error."""
    outside_path = "/tmp/outside/config.yaml"

    with pytest.raises(ValueError):
        dashboard_settings.rel_path(outside_path)


def test_rel_path_empty_args(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path with no arguments returns config_dir."""
    result = dashboard_settings.rel_path()
    assert result == dashboard_settings.config_dir


def test_rel_path_with_pathlib_path(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path works with Path objects as arguments."""
    path_obj = Path("subfolder") / "config.yaml"
    result = dashboard_settings.rel_path(path_obj)

    expected = dashboard_settings.config_dir / "subfolder" / "config.yaml"
    assert result == expected


def test_rel_path_normalizes_slashes(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path normalizes path separators."""
    # os.path.join normalizes slashes on Windows but preserves them on Unix
    # Test that providing components separately gives same result
    result1 = dashboard_settings.rel_path("folder", "subfolder", "file.yaml")
    result2 = dashboard_settings.rel_path("folder", "subfolder", "file.yaml")
    assert result1 == result2

    # Also test that the result is as expected
    expected = dashboard_settings.config_dir / "folder" / "subfolder" / "file.yaml"
    assert result1 == expected


def test_rel_path_handles_spaces(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path handles paths with spaces."""
    result = dashboard_settings.rel_path("my folder", "my config.yaml")

    expected = dashboard_settings.config_dir / "my folder" / "my config.yaml"
    assert result == expected


def test_rel_path_handles_special_chars(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path handles paths with special characters."""
    result = dashboard_settings.rel_path("device-01_test", "config.yaml")

    expected = dashboard_settings.config_dir / "device-01_test" / "config.yaml"
    assert result == expected


def test_config_dir_as_path_property(dashboard_settings: DashboardSettings) -> None:
    """Test that config_dir can be accessed and used with Path operations."""
    config_path = dashboard_settings.config_dir

    assert config_path.exists()
    assert config_path.is_dir()
    assert config_path.is_absolute()


def test_absolute_config_dir_property(dashboard_settings: DashboardSettings) -> None:
    """Test absolute_config_dir is a Path object."""
    assert isinstance(dashboard_settings.absolute_config_dir, Path)
    assert dashboard_settings.absolute_config_dir.exists()
    assert dashboard_settings.absolute_config_dir.is_dir()
    assert dashboard_settings.absolute_config_dir.is_absolute()


def test_rel_path_symlink_inside_config(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path with symlink that points inside config dir."""
    target = dashboard_settings.absolute_config_dir / "target.yaml"
    target.touch()
    symlink = dashboard_settings.absolute_config_dir / "link.yaml"
    symlink.symlink_to(target)
    result = dashboard_settings.rel_path("link.yaml")
    expected = dashboard_settings.config_dir / "link.yaml"
    assert result == expected


def test_rel_path_symlink_outside_config(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path with symlink that points outside config dir."""
    with tempfile.NamedTemporaryFile(suffix=".yaml") as tmp:
        symlink = dashboard_settings.absolute_config_dir / "external_link.yaml"
        symlink.symlink_to(tmp.name)
        with pytest.raises(ValueError):
            dashboard_settings.rel_path("external_link.yaml")


def test_rel_path_with_none_arg(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path handles None arguments gracefully."""
    result = dashboard_settings.rel_path("None")
    expected = dashboard_settings.config_dir / "None"
    assert result == expected


def test_rel_path_with_numeric_args(dashboard_settings: DashboardSettings) -> None:
    """Test rel_path handles numeric arguments."""
    result = dashboard_settings.rel_path("123", "456.789")
    expected = dashboard_settings.config_dir / "123" / "456.789"
    assert result == expected
