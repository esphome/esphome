"""Tests for esphome.util module."""

from pathlib import Path

import pytest

from esphome import util


def test_list_yaml_files_with_files_and_directories(tmp_path: Path) -> None:
    """Test that list_yaml_files handles both files and directories."""
    # Create directory structure
    dir1 = tmp_path / "configs"
    dir1.mkdir()
    dir2 = tmp_path / "more_configs"
    dir2.mkdir()

    # Create YAML files in directories
    (dir1 / "config1.yaml").write_text("test: 1")
    (dir1 / "config2.yml").write_text("test: 2")
    (dir1 / "not_yaml.txt").write_text("not yaml")

    (dir2 / "config3.yaml").write_text("test: 3")

    # Create standalone YAML files
    standalone1 = tmp_path / "standalone.yaml"
    standalone1.write_text("test: 4")
    standalone2 = tmp_path / "another.yml"
    standalone2.write_text("test: 5")

    # Test with mixed input (directories and files)
    configs = [
        dir1,
        standalone1,
        dir2,
        standalone2,
    ]

    result = util.list_yaml_files(configs)

    # Should include all YAML files but not the .txt file
    assert set(result) == {
        dir1 / "config1.yaml",
        dir1 / "config2.yml",
        dir2 / "config3.yaml",
        standalone1,
        standalone2,
    }
    # Check that results are sorted
    assert result == sorted(result)


def test_list_yaml_files_only_directories(tmp_path: Path) -> None:
    """Test list_yaml_files with only directories."""
    dir1 = tmp_path / "dir1"
    dir1.mkdir()
    dir2 = tmp_path / "dir2"
    dir2.mkdir()

    (dir1 / "a.yaml").write_text("test: a")
    (dir1 / "b.yml").write_text("test: b")
    (dir2 / "c.yaml").write_text("test: c")

    result = util.list_yaml_files([dir1, dir2])

    assert set(result) == {
        dir1 / "a.yaml",
        dir1 / "b.yml",
        dir2 / "c.yaml",
    }
    assert result == sorted(result)


def test_list_yaml_files_only_files(tmp_path: Path) -> None:
    """Test list_yaml_files with only files."""
    file1 = tmp_path / "file1.yaml"
    file2 = tmp_path / "file2.yml"
    file3 = tmp_path / "file3.yaml"
    non_yaml = tmp_path / "not_yaml.json"

    file1.write_text("test: 1")
    file2.write_text("test: 2")
    file3.write_text("test: 3")
    non_yaml.write_text("{}")

    # Include a non-YAML file to test filtering
    result = util.list_yaml_files(
        [
            file1,
            file2,
            file3,
            non_yaml,
        ]
    )

    assert set(result) == {
        file1,
        file2,
        file3,
    }
    assert result == sorted(result)


def test_list_yaml_files_empty_directory(tmp_path: Path) -> None:
    """Test list_yaml_files with an empty directory."""
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()

    result = util.list_yaml_files([empty_dir])

    assert result == []


def test_list_yaml_files_nonexistent_path(tmp_path: Path) -> None:
    """Test list_yaml_files with a nonexistent path raises an error."""
    nonexistent = tmp_path / "nonexistent"
    existing = tmp_path / "existing.yaml"
    existing.write_text("test: 1")

    # Should raise an error for non-existent directory
    with pytest.raises(FileNotFoundError):
        util.list_yaml_files([nonexistent, existing])


def test_list_yaml_files_mixed_extensions(tmp_path: Path) -> None:
    """Test that both .yaml and .yml extensions are recognized."""
    dir1 = tmp_path / "configs"
    dir1.mkdir()

    yaml_file = dir1 / "config.yaml"
    yml_file = dir1 / "config.yml"
    other_file = dir1 / "config.txt"

    yaml_file.write_text("test: yaml")
    yml_file.write_text("test: yml")
    other_file.write_text("test: txt")

    result = util.list_yaml_files([dir1])

    assert set(result) == {
        yaml_file,
        yml_file,
    }
