"""Tests for config_validation.py path-related functions."""

from pathlib import Path

import pytest
import voluptuous as vol

from esphome import config_validation as cv


def test_directory_valid_path(setup_core: Path) -> None:
    """Test directory validator with valid directory."""
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    result = cv.directory("test_directory")

    assert result == test_dir


def test_directory_absolute_path(setup_core: Path) -> None:
    """Test directory validator with absolute path."""
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    result = cv.directory(str(test_dir))

    assert result == test_dir


def test_directory_nonexistent_path(setup_core: Path) -> None:
    """Test directory validator raises error for non-existent directory."""
    with pytest.raises(
        vol.Invalid, match="Could not find directory.*nonexistent_directory"
    ):
        cv.directory("nonexistent_directory")


def test_directory_file_instead_of_directory(setup_core: Path) -> None:
    """Test directory validator raises error when path is a file."""
    test_file = setup_core / "test_file.txt"
    test_file.write_text("content")

    with pytest.raises(vol.Invalid, match="is not a directory"):
        cv.directory("test_file.txt")


def test_directory_with_parent_directory(setup_core: Path) -> None:
    """Test directory validator with nested directory structure."""
    nested_dir = setup_core / "parent" / "child" / "grandchild"
    nested_dir.mkdir(parents=True)

    result = cv.directory("parent/child/grandchild")

    assert result == nested_dir


def test_file_valid_path(setup_core: Path) -> None:
    """Test file_ validator with valid file."""
    test_file = setup_core / "test_file.yaml"
    test_file.write_text("test content")

    result = cv.file_("test_file.yaml")

    assert result == test_file


def test_file_absolute_path(setup_core: Path) -> None:
    """Test file_ validator with absolute path."""
    test_file = setup_core / "test_file.yaml"
    test_file.write_text("test content")

    result = cv.file_(str(test_file))

    assert result == test_file


def test_file_nonexistent_path(setup_core: Path) -> None:
    """Test file_ validator raises error for non-existent file."""
    with pytest.raises(vol.Invalid, match="Could not find file.*nonexistent_file.yaml"):
        cv.file_("nonexistent_file.yaml")


def test_file_directory_instead_of_file(setup_core: Path) -> None:
    """Test file_ validator raises error when path is a directory."""
    test_dir = setup_core / "test_directory"
    test_dir.mkdir()

    with pytest.raises(vol.Invalid, match="is not a file"):
        cv.file_("test_directory")


def test_file_with_parent_directory(setup_core: Path) -> None:
    """Test file_ validator with file in nested directory."""
    nested_dir = setup_core / "configs" / "sensors"
    nested_dir.mkdir(parents=True)
    test_file = nested_dir / "temperature.yaml"
    test_file.write_text("sensor config")

    result = cv.file_("configs/sensors/temperature.yaml")

    assert result == test_file


def test_directory_handles_trailing_slash(setup_core: Path) -> None:
    """Test directory validator handles trailing slashes correctly."""
    test_dir = setup_core / "test_dir"
    test_dir.mkdir()

    result = cv.directory("test_dir/")
    assert result == test_dir

    result = cv.directory("test_dir")
    assert result == test_dir


def test_file_handles_various_extensions(setup_core: Path) -> None:
    """Test file_ validator works with different file extensions."""
    yaml_file = setup_core / "config.yaml"
    yaml_file.write_text("yaml content")
    assert cv.file_("config.yaml") == yaml_file

    yml_file = setup_core / "config.yml"
    yml_file.write_text("yml content")
    assert cv.file_("config.yml") == yml_file

    txt_file = setup_core / "readme.txt"
    txt_file.write_text("text content")
    assert cv.file_("readme.txt") == txt_file

    no_ext_file = setup_core / "LICENSE"
    no_ext_file.write_text("license content")
    assert cv.file_("LICENSE") == no_ext_file


def test_directory_with_symlink(setup_core: Path) -> None:
    """Test directory validator follows symlinks."""
    actual_dir = setup_core / "actual_directory"
    actual_dir.mkdir()

    symlink_dir = setup_core / "symlink_directory"
    symlink_dir.symlink_to(actual_dir)

    result = cv.directory("symlink_directory")
    assert result == symlink_dir


def test_file_with_symlink(setup_core: Path) -> None:
    """Test file_ validator follows symlinks."""
    actual_file = setup_core / "actual_file.txt"
    actual_file.write_text("content")

    symlink_file = setup_core / "symlink_file.txt"
    symlink_file.symlink_to(actual_file)

    result = cv.file_("symlink_file.txt")
    assert result == symlink_file


def test_directory_error_shows_full_path(setup_core: Path) -> None:
    """Test directory validator error message includes full path."""
    with pytest.raises(vol.Invalid, match=".*missing_dir.*full path:.*"):
        cv.directory("missing_dir")


def test_file_error_shows_full_path(setup_core: Path) -> None:
    """Test file_ validator error message includes full path."""
    with pytest.raises(vol.Invalid, match=".*missing_file.yaml.*full path:.*"):
        cv.file_("missing_file.yaml")


def test_directory_with_spaces_in_name(setup_core: Path) -> None:
    """Test directory validator handles spaces in directory names."""
    dir_with_spaces = setup_core / "my test directory"
    dir_with_spaces.mkdir()

    result = cv.directory("my test directory")
    assert result == dir_with_spaces


def test_file_with_spaces_in_name(setup_core: Path) -> None:
    """Test file_ validator handles spaces in file names."""
    file_with_spaces = setup_core / "my test file.yaml"
    file_with_spaces.write_text("content")

    result = cv.file_("my test file.yaml")
    assert result == file_with_spaces
