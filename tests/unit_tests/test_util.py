"""Tests for esphome.util module."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock, patch

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
    assert root / "config1.yaml" in result
    assert root / "config2.yml" in result
    assert root / "device.yaml" in result

    # Ensure nested files are NOT found
    for r in result:
        r_str = str(r)
        assert "subdir" not in r_str
        assert "deeper" not in r_str
        assert "nested1.yaml" not in r_str
        assert "nested2.yml" not in r_str
        assert "very_nested.yaml" not in r_str


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
    assert root / "config.yaml" in result
    assert root / "device.yaml" in result
    assert root / "secrets.yaml" not in result
    assert root / "secrets.yml" not in result


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
    assert root / "config.yaml" in result
    assert root / "device.yaml" in result
    assert root / ".hidden.yaml" not in result
    assert root / ".backup.yml" not in result


def test_filter_yaml_files_basic() -> None:
    """Test filter_yaml_files function."""
    files = [
        Path("/path/to/config.yaml"),
        Path("/path/to/device.yml"),
        Path("/path/to/readme.txt"),
        Path("/path/to/script.py"),
        Path("/path/to/data.json"),
        Path("/path/to/another.yaml"),
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 3
    assert Path("/path/to/config.yaml") in result
    assert Path("/path/to/device.yml") in result
    assert Path("/path/to/another.yaml") in result
    assert Path("/path/to/readme.txt") not in result
    assert Path("/path/to/script.py") not in result
    assert Path("/path/to/data.json") not in result


def test_filter_yaml_files_excludes_secrets() -> None:
    """Test that filter_yaml_files excludes secrets files."""
    files = [
        Path("/path/to/config.yaml"),
        Path("/path/to/secrets.yaml"),
        Path("/path/to/secrets.yml"),
        Path("/path/to/device.yaml"),
        Path("/some/dir/secrets.yaml"),
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 2
    assert Path("/path/to/config.yaml") in result
    assert Path("/path/to/device.yaml") in result
    assert Path("/path/to/secrets.yaml") not in result
    assert Path("/path/to/secrets.yml") not in result
    assert Path("/some/dir/secrets.yaml") not in result


def test_filter_yaml_files_excludes_hidden() -> None:
    """Test that filter_yaml_files excludes hidden files."""
    files = [
        Path("/path/to/config.yaml"),
        Path("/path/to/.hidden.yaml"),
        Path("/path/to/.backup.yml"),
        Path("/path/to/device.yaml"),
        Path("/some/dir/.config.yaml"),
    ]

    result = util.filter_yaml_files(files)

    assert len(result) == 2
    assert Path("/path/to/config.yaml") in result
    assert Path("/path/to/device.yaml") in result
    assert Path("/path/to/.hidden.yaml") not in result
    assert Path("/path/to/.backup.yml") not in result
    assert Path("/some/dir/.config.yaml") not in result


def test_filter_yaml_files_case_sensitive() -> None:
    """Test that filter_yaml_files is case-sensitive for extensions."""
    files = [
        Path("/path/to/config.yaml"),
        Path("/path/to/config.YAML"),
        Path("/path/to/config.YML"),
        Path("/path/to/config.Yaml"),
        Path("/path/to/config.yml"),
    ]

    result = util.filter_yaml_files(files)

    # Should only match lowercase .yaml and .yml
    assert len(result) == 2

    # Check the actual suffixes to ensure case-sensitive filtering
    result_suffixes = [p.suffix for p in result]
    assert ".yaml" in result_suffixes
    assert ".yml" in result_suffixes

    # Verify the filtered files have the expected names
    result_names = [p.name for p in result]
    assert "config.yaml" in result_names
    assert "config.yml" in result_names
    # Ensure uppercase extensions are NOT included
    assert "config.YAML" not in result_names
    assert "config.YML" not in result_names
    assert "config.Yaml" not in result_names


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


def test_get_rp2040_mass_storage_volumes_macos(tmp_path: Path) -> None:
    """Test RP2040 mass storage detection on macOS."""
    volumes_dir = tmp_path / "Volumes"
    volumes_dir.mkdir()
    rpi_vol = volumes_dir / "RPI-RP2"
    rpi_vol.mkdir()

    with (
        patch("esphome.util.sys") as mock_sys,
        patch("esphome.util.Path") as mock_path_cls,
    ):
        mock_sys.platform = "darwin"
        # Make Path("/Volumes") return our tmp_path version
        mock_path_cls.side_effect = lambda p: (
            volumes_dir if p == "/Volumes" else Path(p)
        )

        result = util.get_rp2040_mass_storage_volumes()

    assert len(result) == 1
    assert result[0].description == "RP2040 BOOTSEL"


def test_get_rp2040_mass_storage_volumes_none_found(tmp_path: Path) -> None:
    """Test RP2040 mass storage detection when no volumes found."""
    # Point at an empty directory so no RPI-RP2* matches
    empty_dir = tmp_path / "Volumes"
    empty_dir.mkdir()

    with (
        patch("esphome.util.sys.platform", "darwin"),
        patch(
            "esphome.util.Path",
            side_effect=lambda p: empty_dir if p == "/Volumes" else Path(p),
        ),
    ):
        result = util.get_rp2040_mass_storage_volumes()

    assert result == []


def test_get_rp2040_mass_storage_volumes_linux(tmp_path: Path) -> None:
    """Test RP2040 mass storage detection on Linux."""
    # Create /media/<user>/RPI-RP2 structure
    media_dir = tmp_path / "media"
    media_dir.mkdir()
    user_dir = media_dir / "testuser"
    user_dir.mkdir()
    rp2_dir = user_dir / "RPI-RP2"
    rp2_dir.mkdir()

    # Create /run/media and /mnt as empty dirs
    run_media_dir = tmp_path / "run_media"
    run_media_dir.mkdir()
    mnt_dir = tmp_path / "mnt"
    mnt_dir.mkdir()

    def mock_path_side_effect(p: str) -> Path:
        if p == "/media":
            return media_dir
        if p == "/run/media":
            return run_media_dir
        if p == "/mnt":
            return mnt_dir
        return Path(p)

    with (
        patch("esphome.util.sys.platform", "linux"),
        patch("esphome.util.Path", side_effect=mock_path_side_effect),
    ):
        result = util.get_rp2040_mass_storage_volumes()

    assert len(result) == 1
    assert result[0].description == "RP2040 BOOTSEL"


def test_get_rp2040_mass_storage_volumes_linux_oserror(tmp_path: Path) -> None:
    """Test RP2040 mass storage detection on Linux handles OSError."""
    media_dir = tmp_path / "media"
    media_dir.mkdir()

    def mock_path_side_effect(p: str) -> Path:
        if p == "/media":
            return media_dir
        if p in ("/run/media", "/mnt"):
            # Return a path that will raise OSError when globbed
            return tmp_path / "nonexistent"
        return Path(p)

    with (
        patch("esphome.util.sys.platform", "linux"),
        patch("esphome.util.Path", side_effect=mock_path_side_effect),
    ):
        result = util.get_rp2040_mass_storage_volumes()

    assert result == []


def test_get_rp2040_mass_storage_volumes_windows() -> None:
    """Test RP2040 mass storage detection on Windows."""
    mock_ctypes = MagicMock()
    mock_volume_name = MagicMock()
    mock_volume_name.value = "RPI-RP2"
    mock_ctypes.create_unicode_buffer.return_value = mock_volume_name

    def path_side_effect(p: str) -> MagicMock:
        inst = MagicMock()
        inst.exists.return_value = p == "D:\\"
        return inst

    with (
        patch("esphome.util.sys.platform", "win32"),
        patch.dict("sys.modules", {"ctypes": mock_ctypes}),
        patch("esphome.util.Path", side_effect=path_side_effect),
    ):
        result = util.get_rp2040_mass_storage_volumes()

    assert len(result) >= 1
    assert result[0].description == "RP2040 BOOTSEL"


def test_get_rp2040_mass_storage_volumes_unsupported_platform() -> None:
    """Test RP2040 mass storage detection on unsupported platform returns empty."""
    with patch("esphome.util.sys.platform", "freebsd"):
        result = util.get_rp2040_mass_storage_volumes()

    assert result == []


def test_mass_storage_volume_attributes() -> None:
    """Test MassStorageVolume class attributes."""
    vol = util.MassStorageVolume(Path("/Volumes/RPI-RP2"), "RP2040 BOOTSEL")
    assert vol.path == Path("/Volumes/RPI-RP2")
    assert vol.description == "RP2040 BOOTSEL"
