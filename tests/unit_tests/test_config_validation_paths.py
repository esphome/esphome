"""Tests for config_validation.py path-related functions."""

from pathlib import Path

import pytest
import voluptuous as vol

from esphome import config_validation as cv
from esphome.core import CORE


@pytest.fixture
def setup_core(tmp_path: Path) -> Path:
    """Set up CORE with test paths."""
    CORE.reset()
    CORE.config_path = str(tmp_path / "test.yaml")
    return tmp_path


def test_directory_valid_path(setup_core: Path) -> None:
    """Test directory validator with valid directory."""
    # Create a test directory
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    # Use relative path
    result = cv.directory("test_directory")

    assert result == "test_directory"


def test_directory_absolute_path(setup_core: Path) -> None:
    """Test directory validator with absolute path."""
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    result = cv.directory(str(test_dir))

    assert result == str(test_dir)


def test_directory_nonexistent_path(setup_core: Path) -> None:
    """Test directory validator raises error for non-existent directory."""
    with pytest.raises(vol.Invalid) as exc_info:
        cv.directory("nonexistent_directory")

    assert "Could not find directory" in str(exc_info.value)
    assert "nonexistent_directory" in str(exc_info.value)


def test_directory_file_instead_of_directory(setup_core: Path) -> None:
    """Test directory validator raises error when path is a file."""
    # Create a file instead of directory
    test_file = setup_core / "test_file.txt"
    test_file.write_text("content")

    with pytest.raises(vol.Invalid) as exc_info:
        cv.directory("test_file.txt")

    assert "is not a directory" in str(exc_info.value)


def test_directory_with_parent_directory(setup_core: Path) -> None:
    """Test directory validator with nested directory structure."""
    # Create nested directories
    nested_dir = setup_core / "parent" / "child" / "grandchild"
    nested_dir.mkdir(parents=True)

    result = cv.directory("parent/child/grandchild")

    assert result == "parent/child/grandchild"


def test_file_valid_path(setup_core: Path) -> None:
    """Test file_ validator with valid file."""
    # Create a test file
    test_file = setup_core / "test_file.yaml"
    test_file.write_text("test content")

    result = cv.file_("test_file.yaml")

    assert result == "test_file.yaml"


def test_file_absolute_path(setup_core: Path) -> None:
    """Test file_ validator with absolute path."""
    test_file = setup_core / "test_file.yaml"
    test_file.write_text("test content")

    result = cv.file_(str(test_file))

    assert result == str(test_file)


def test_file_nonexistent_path(setup_core: Path) -> None:
    """Test file_ validator raises error for non-existent file."""
    with pytest.raises(vol.Invalid) as exc_info:
        cv.file_("nonexistent_file.yaml")

    assert "Could not find file" in str(exc_info.value)
    assert "nonexistent_file.yaml" in str(exc_info.value)


def test_file_directory_instead_of_file(setup_core: Path) -> None:
    """Test file_ validator raises error when path is a directory."""
    # Create a directory instead of file
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    with pytest.raises(vol.Invalid) as exc_info:
        cv.file_("test_directory")

    assert "is not a file" in str(exc_info.value)


def test_file_with_parent_directory(setup_core: Path) -> None:
    """Test file_ validator with file in nested directory."""
    # Create nested directories and file
    nested_dir = setup_core / "configs" / "sensors"
    nested_dir.mkdir(parents=True)
    test_file = nested_dir / "temperature.yaml"
    test_file.write_text("sensor config")

    result = cv.file_("configs/sensors/temperature.yaml")

    assert result == "configs/sensors/temperature.yaml"


def test_directory_handles_trailing_slash(setup_core: Path) -> None:
    """Test directory validator handles trailing slashes correctly."""
    test_dir = setup_core / "test_dir"
    test_dir.mkdir()

    # Test with trailing slash
    result = cv.directory("test_dir/")
    assert result == "test_dir/"

    # Test without trailing slash
    result = cv.directory("test_dir")
    assert result == "test_dir"


def test_file_handles_various_extensions(setup_core: Path) -> None:
    """Test file_ validator works with different file extensions."""
    # Test with .yaml
    yaml_file = setup_core / "config.yaml"
    yaml_file.write_text("yaml content")
    assert cv.file_("config.yaml") == "config.yaml"

    # Test with .yml
    yml_file = setup_core / "config.yml"
    yml_file.write_text("yml content")
    assert cv.file_("config.yml") == "config.yml"

    # Test with .txt
    txt_file = setup_core / "readme.txt"
    txt_file.write_text("text content")
    assert cv.file_("readme.txt") == "readme.txt"

    # Test with no extension
    no_ext_file = setup_core / "LICENSE"
    no_ext_file.write_text("license content")
    assert cv.file_("LICENSE") == "LICENSE"


def test_directory_with_symlink(setup_core: Path) -> None:
    """Test directory validator follows symlinks."""
    # Create actual directory
    actual_dir = setup_core / "actual_directory"
    actual_dir.mkdir()

    # Create symlink to directory
    symlink_dir = setup_core / "symlink_directory"
    symlink_dir.symlink_to(actual_dir)

    result = cv.directory("symlink_directory")
    assert result == "symlink_directory"


def test_file_with_symlink(setup_core: Path) -> None:
    """Test file_ validator follows symlinks."""
    # Create actual file
    actual_file = setup_core / "actual_file.txt"
    actual_file.write_text("content")

    # Create symlink to file
    symlink_file = setup_core / "symlink_file.txt"
    symlink_file.symlink_to(actual_file)

    result = cv.file_("symlink_file.txt")
    assert result == "symlink_file.txt"


def test_directory_error_shows_full_path(setup_core: Path) -> None:
    """Test directory validator error message includes full path."""
    with pytest.raises(vol.Invalid) as exc_info:
        cv.directory("missing_dir")

    error_message = str(exc_info.value)
    # Should show both relative and absolute paths
    assert "missing_dir" in error_message
    assert "full path:" in error_message
    assert str(setup_core) in error_message


def test_file_error_shows_full_path(setup_core: Path) -> None:
    """Test file_ validator error message includes full path."""
    with pytest.raises(vol.Invalid) as exc_info:
        cv.file_("missing_file.yaml")

    error_message = str(exc_info.value)
    # Should show both relative and absolute paths
    assert "missing_file.yaml" in error_message
    assert "full path:" in error_message
    assert str(setup_core) in error_message


def test_directory_with_spaces_in_name(setup_core: Path) -> None:
    """Test directory validator handles spaces in directory names."""
    dir_with_spaces = setup_core / "my test directory"
    dir_with_spaces.mkdir()

    result = cv.directory("my test directory")
    assert result == "my test directory"


def test_file_with_spaces_in_name(setup_core: Path) -> None:
    """Test file_ validator handles spaces in file names."""
    file_with_spaces = setup_core / "my test file.yaml"
    file_with_spaces.write_text("content")

    result = cv.file_("my test file.yaml")
    assert result == "my test file.yaml"
