"""Tests for esphome.util module."""

from __future__ import annotations

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
        str(dir1),
        str(standalone1),
        str(dir2),
        str(standalone2),
    ]

    result = util.list_yaml_files(configs)

    # Should include all YAML files but not the .txt file
    assert set(result) == {
        str(dir1 / "config1.yaml"),
        str(dir1 / "config2.yml"),
        str(dir2 / "config3.yaml"),
        str(standalone1),
        str(standalone2),
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

    result = util.list_yaml_files([str(dir1), str(dir2)])

    assert set(result) == {
        str(dir1 / "a.yaml"),
        str(dir1 / "b.yml"),
        str(dir2 / "c.yaml"),
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
            str(file1),
            str(file2),
            str(file3),
            str(non_yaml),
        ]
    )

    assert set(result) == {
        str(file1),
        str(file2),
        str(file3),
    }
    assert result == sorted(result)


def test_list_yaml_files_empty_directory(tmp_path: Path) -> None:
    """Test list_yaml_files with an empty directory."""
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()

    result = util.list_yaml_files([str(empty_dir)])

    assert result == []


def test_list_yaml_files_nonexistent_path(tmp_path: Path) -> None:
    """Test list_yaml_files with a nonexistent path raises an error."""
    nonexistent = tmp_path / "nonexistent"
    existing = tmp_path / "existing.yaml"
    existing.write_text("test: 1")

    # Should raise an error for non-existent directory
    with pytest.raises(FileNotFoundError):
        util.list_yaml_files([str(nonexistent), str(existing)])


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

    result = util.list_yaml_files([str(dir1)])

    assert set(result) == {
        str(yaml_file),
        str(yml_file),
    }


def test_list_yaml_files_does_not_recurse_into_subdirectories(tmp_path: Path) -> None:
    """Test that list_yaml_files only finds files in specified directory, not subdirectories."""
    # Create directory structure with YAML files at different depths
    root = tmp_path / "configs"
    root.mkdir()

    # Create YAML files in the root directory
    (root / "config1.yaml").write_text("test: 1")
    (root / "config2.yml").write_text("test: 2")
    (root / "device.yaml").write_text("test: device")

    # Create subdirectory with YAML files (should NOT be found)
    subdir = root / "subdir"
    subdir.mkdir()
    (subdir / "nested1.yaml").write_text("test: nested1")
    (subdir / "nested2.yml").write_text("test: nested2")

    # Create deeper subdirectory (should NOT be found)
    deep_subdir = subdir / "deeper"
    deep_subdir.mkdir()
    (deep_subdir / "very_nested.yaml").write_text("test: very_nested")

    # Test listing files from the root directory
    result = util.list_yaml_files([str(root)])

    # Should only find the 3 files in root, not the 3 in subdirectories
    assert len(result) == 3

    # Check that only root-level files are found
    assert str(root / "config1.yaml") in result
    assert str(root / "config2.yml") in result
    assert str(root / "device.yaml") in result

    # Ensure nested files are NOT found
    for r in result:
        assert "subdir" not in r
        assert "deeper" not in r
        assert "nested1.yaml" not in r
        assert "nested2.yml" not in r
        assert "very_nested.yaml" not in r


def test_list_yaml_files_excludes_secrets(tmp_path: Path) -> None:
    """Test that secrets.yaml and secrets.yml are excluded."""
    root = tmp_path / "configs"
    root.mkdir()

    # Create various YAML files including secrets
    (root / "config.yaml").write_text("test: config")
    (root / "secrets.yaml").write_text("wifi_password: secret123")
    (root / "secrets.yml").write_text("api_key: secret456")
    (root / "device.yaml").write_text("test: device")

    result = util.list_yaml_files([str(root)])

    # Should find 2 files (config.yaml and device.yaml), not secrets
    assert len(result) == 2
    assert str(root / "config.yaml") in result
    assert str(root / "device.yaml") in result
    assert str(root / "secrets.yaml") not in result
    assert str(root / "secrets.yml") not in result


def test_list_yaml_files_excludes_hidden_files(tmp_path: Path) -> None:
    """Test that hidden files (starting with .) are excluded."""
    root = tmp_path / "configs"
    root.mkdir()

    # Create regular and hidden YAML files
    (root / "config.yaml").write_text("test: config")
    (root / ".hidden.yaml").write_text("test: hidden")
    (root / ".backup.yml").write_text("test: backup")
    (root / "device.yaml").write_text("test: device")

    result = util.list_yaml_files([str(root)])

    # Should find only non-hidden files
    assert len(result) == 2
    assert str(root / "config.yaml") in result
    assert str(root / "device.yaml") in result
    assert str(root / ".hidden.yaml") not in result
    assert str(root / ".backup.yml") not in result


def test_filter_yaml_files_basic() -> None:
    """Test filter_yaml_files function."""
    files = [
        "/path/to/config.yaml",
        "/path/to/device.yml",
        "/path/to/readme.txt",
        "/path/to/script.py",
        "/path/to/data.json",
        "/path/to/another.yaml",
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 3
    assert "/path/to/config.yaml" in result
    assert "/path/to/device.yml" in result
    assert "/path/to/another.yaml" in result
    assert "/path/to/readme.txt" not in result
    assert "/path/to/script.py" not in result
    assert "/path/to/data.json" not in result


def test_filter_yaml_files_excludes_secrets() -> None:
    """Test that filter_yaml_files excludes secrets files."""
    files = [
        "/path/to/config.yaml",
        "/path/to/secrets.yaml",
        "/path/to/secrets.yml",
        "/path/to/device.yaml",
        "/some/dir/secrets.yaml",
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 2
    assert "/path/to/config.yaml" in result
    assert "/path/to/device.yaml" in result
    assert "/path/to/secrets.yaml" not in result
    assert "/path/to/secrets.yml" not in result
    assert "/some/dir/secrets.yaml" not in result


def test_filter_yaml_files_excludes_hidden() -> None:
    """Test that filter_yaml_files excludes hidden files."""
    files = [
        "/path/to/config.yaml",
        "/path/to/.hidden.yaml",
        "/path/to/.backup.yml",
        "/path/to/device.yaml",
        "/some/dir/.config.yaml",
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 2
    assert "/path/to/config.yaml" in result
    assert "/path/to/device.yaml" in result
    assert "/path/to/.hidden.yaml" not in result
    assert "/path/to/.backup.yml" not in result
    assert "/some/dir/.config.yaml" not in result


def test_filter_yaml_files_case_sensitive() -> None:
    """Test that filter_yaml_files is case-sensitive for extensions."""
    files = [
        "/path/to/config.yaml",
        "/path/to/config.YAML",
        "/path/to/config.YML",
        "/path/to/config.Yaml",
        "/path/to/config.yml",
    ]

    result = util.filter_yaml_files(files)

    # Should only match lowercase .yaml and .yml
    assert len(result) == 2
    assert "/path/to/config.yaml" in result
    assert "/path/to/config.yml" in result
    assert "/path/to/config.YAML" not in result
    assert "/path/to/config.YML" not in result
    assert "/path/to/config.Yaml" not in result


@pytest.mark.parametrize(
    ("input_str", "expected"),
    [
        # Empty string
        ("", "''"),
        # Simple strings that don't need quoting
        ("hello", "hello"),
        ("test123", "test123"),
        ("file.txt", "file.txt"),
        ("/path/to/file", "/path/to/file"),
        ("user@host", "user@host"),
        ("value:123", "value:123"),
        ("item,list", "item,list"),
        ("path-with-dash", "path-with-dash"),
        # Strings that need quoting
        ("hello world", "'hello world'"),
        ("test\ttab", "'test\ttab'"),
        ("line\nbreak", "'line\nbreak'"),
        ("semicolon;here", "'semicolon;here'"),
        ("pipe|symbol", "'pipe|symbol'"),
        ("redirect>file", "'redirect>file'"),
        ("redirect<file", "'redirect<file'"),
        ("background&", "'background&'"),
        ("dollar$sign", "'dollar$sign'"),
        ("backtick`cmd", "'backtick`cmd'"),
        ('double"quote', "'double\"quote'"),
        ("backslash\\path", "'backslash\\path'"),
        ("question?mark", "'question?mark'"),
        ("asterisk*wild", "'asterisk*wild'"),
        ("bracket[test]", "'bracket[test]'"),
        ("paren(test)", "'paren(test)'"),
        ("curly{brace}", "'curly{brace}'"),
        # Single quotes in string (special escaping)
        ("it's", "'it'\"'\"'s'"),
        ("don't", "'don'\"'\"'t'"),
        ("'quoted'", "''\"'\"'quoted'\"'\"''"),
        # Complex combinations
        ("test 'with' quotes", "'test '\"'\"'with'\"'\"' quotes'"),
        ("path/to/file's.txt", "'path/to/file'\"'\"'s.txt'"),
    ],
)
def test_shlex_quote(input_str: str, expected: str) -> None:
    """Test shlex_quote properly escapes shell arguments."""
    assert util.shlex_quote(input_str) == expected


def test_shlex_quote_safe_characters() -> None:
    """Test that safe characters are not quoted."""
    # These characters are considered safe and shouldn't be quoted
    safe_chars = (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@%+=:,./-_"
    )
    for char in safe_chars:
        assert util.shlex_quote(char) == char
        assert util.shlex_quote(f"test{char}test") == f"test{char}test"


def test_shlex_quote_unsafe_characters() -> None:
    """Test that unsafe characters trigger quoting."""
    # These characters should trigger quoting
    unsafe_chars = ' \t\n;|>&<$`"\\?*[](){}!#~^'
    for char in unsafe_chars:
        result = util.shlex_quote(f"test{char}test")
        assert result.startswith("'")
        assert result.endswith("'")


def test_shlex_quote_edge_cases() -> None:
    """Test edge cases for shlex_quote."""
    # Multiple single quotes
    assert util.shlex_quote("'''") == "''\"'\"''\"'\"''\"'\"''"

    # Mixed quotes
    assert util.shlex_quote('"\'"') == "'\"'\"'\"'\"'"

    # Only whitespace
    assert util.shlex_quote(" ") == "' '"
    assert util.shlex_quote("\t") == "'\t'"
    assert util.shlex_quote("\n") == "'\n'"
    assert util.shlex_quote("   ") == "'   '"
