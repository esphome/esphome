"""Unit tests for script/clang_tidy_hash.py module."""

import hashlib
from pathlib import Path
import sys
from unittest.mock import patch

import pytest

# Add the script directory to Python path so we can import clang_tidy_hash
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import clang_tidy_hash  # noqa: E402


@pytest.mark.parametrize(
    ("file_content", "expected"),
    [
        (
            "clang-tidy==18.1.5 # via -r requirements_dev.in\n",
            "clang-tidy==18.1.5 # via -r requirements_dev.in",
        ),
        (
            "other-package==1.0\nclang-tidy==17.0.0\nmore-packages==2.0\n",
            "clang-tidy==17.0.0",
        ),
        (
            "# comment\nclang-tidy==16.0.0  # some comment\n",
            "clang-tidy==16.0.0  # some comment",
        ),
        ("no-clang-tidy-here==1.0\n", "clang-tidy version not found"),
    ],
)
def test_get_clang_tidy_version_from_requirements(
    file_content: str, expected: str
) -> None:
    """Test extracting clang-tidy version from various file formats."""
    # Mock read_file_lines to return our test content
    with patch("clang_tidy_hash.read_file_lines") as mock_read:
        mock_read.return_value = file_content.splitlines(keepends=True)

        result = clang_tidy_hash.get_clang_tidy_version_from_requirements()

    assert result == expected


def test_calculate_clang_tidy_hash_with_sdkconfig(tmp_path: Path) -> None:
    """Test calculating hash from all configuration sources including sdkconfig.defaults."""
    clang_tidy_content = b"Checks: '-*,readability-*'\n"
    requirements_version = "clang-tidy==18.1.5"
    platformio_content = b"[env:esp32]\nplatform = espressif32\n"
    sdkconfig_content = b""
    requirements_content = "clang-tidy==18.1.5\n"

    # Create temporary files
    (tmp_path / ".clang-tidy").write_bytes(clang_tidy_content)
    (tmp_path / "platformio.ini").write_bytes(platformio_content)
    (tmp_path / "sdkconfig.defaults").write_bytes(sdkconfig_content)
    (tmp_path / "requirements_dev.txt").write_text(requirements_content)

    # Expected hash calculation
    expected_hasher = hashlib.sha256()
    expected_hasher.update(clang_tidy_content)
    expected_hasher.update(requirements_version.encode())
    expected_hasher.update(platformio_content)
    expected_hasher.update(b"sdkconfig.defaults")
    expected_hasher.update(sdkconfig_content)
    expected_hash = expected_hasher.hexdigest()

    result = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    assert result == expected_hash


def test_calculate_clang_tidy_hash_includes_per_target_sdkconfig(
    tmp_path: Path,
) -> None:
    """Per-target sdkconfig.defaults.<target> files must be part of the hash."""
    (tmp_path / ".clang-tidy").write_bytes(b"Checks: '-*'\n")
    (tmp_path / "platformio.ini").write_bytes(b"[env:esp32]\n")
    (tmp_path / "requirements_dev.txt").write_text("clang-tidy==18.1.5\n")
    (tmp_path / "sdkconfig.defaults").write_bytes(b"CONFIG_BASE=y\n")

    before = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    # Adding a per-target file must change the hash.
    per_target = tmp_path / "sdkconfig.defaults.esp32c6"
    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=y\n")
    after_add = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_add != before

    # Editing the per-target file must change the hash again.
    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=n\n")
    after_edit = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_edit != after_add


def test_calculate_clang_tidy_hash_without_sdkconfig(tmp_path: Path) -> None:
    """Test calculating hash without sdkconfig.defaults file."""
    clang_tidy_content = b"Checks: '-*,readability-*'\n"
    requirements_version = "clang-tidy==18.1.5"
    platformio_content = b"[env:esp32]\nplatform = espressif32\n"
    requirements_content = "clang-tidy==18.1.5\n"

    # Create temporary files (without sdkconfig.defaults)
    (tmp_path / ".clang-tidy").write_bytes(clang_tidy_content)
    (tmp_path / "platformio.ini").write_bytes(platformio_content)
    (tmp_path / "requirements_dev.txt").write_text(requirements_content)

    # Expected hash calculation (no sdkconfig)
    expected_hasher = hashlib.sha256()
    expected_hasher.update(clang_tidy_content)
    expected_hasher.update(requirements_version.encode())
    expected_hasher.update(platformio_content)
    expected_hash = expected_hasher.hexdigest()

    result = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    assert result == expected_hash


def test_read_file_lines(tmp_path: Path) -> None:
    """Test read_file_lines helper function."""
    test_file = tmp_path / "test.txt"
    test_content = "line1\nline2\nline3\n"
    test_file.write_text(test_content)

    result = clang_tidy_hash.read_file_lines(test_file)

    assert result == ["line1\n", "line2\n", "line3\n"]


def test_read_file_bytes(tmp_path: Path) -> None:
    """Test read_file_bytes helper function."""
    test_file = tmp_path / "test.bin"
    test_content = b"binary content\x00\xff"
    test_file.write_bytes(test_content)

    result = clang_tidy_hash.read_file_bytes(test_file)

    assert result == test_content


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        ("clang-tidy==18.1.5", ("clang-tidy", "clang-tidy==18.1.5")),
        (
            "clang-tidy==18.1.5  # comment",
            ("clang-tidy", "clang-tidy==18.1.5  # comment"),
        ),
        ("some-package>=1.0,<2.0", ("some-package", "some-package>=1.0,<2.0")),
        ("pkg_with-dashes==1.0", ("pkg_with-dashes", "pkg_with-dashes==1.0")),
        ("# just a comment", None),
        ("", None),
        ("   ", None),
        ("invalid line without version", None),
    ],
)
def test_parse_requirement_line(line: str, expected: tuple[str, str] | None) -> None:
    """Test parsing individual requirement lines."""
    result = clang_tidy_hash.parse_requirement_line(line)
    assert result == expected
