"""Unit tests for script/clang_tidy_hash.py module."""

import hashlib
from pathlib import Path
import sys
from unittest.mock import Mock, patch

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


def test_calculate_clang_tidy_hash() -> None:
    """Test calculating hash from all configuration sources."""
    clang_tidy_content = b"Checks: '-*,readability-*'\n"
    requirements_version = "clang-tidy==18.1.5"
    platformio_content = b"[env:esp32]\nplatform = espressif32\n"

    # Expected hash calculation
    expected_hasher = hashlib.sha256()
    expected_hasher.update(clang_tidy_content)
    expected_hasher.update(requirements_version.encode())
    expected_hasher.update(platformio_content)
    expected_hash = expected_hasher.hexdigest()

    # Mock the dependencies
    with (
        patch("clang_tidy_hash.read_file_bytes") as mock_read_bytes,
        patch(
            "clang_tidy_hash.get_clang_tidy_version_from_requirements",
            return_value=requirements_version,
        ),
    ):
        # Set up mock to return different content based on the file being read
        def read_file_mock(path: Path) -> bytes:
            if ".clang-tidy" in str(path):
                return clang_tidy_content
            if "platformio.ini" in str(path):
                return platformio_content
            return b""

        mock_read_bytes.side_effect = read_file_mock
        result = clang_tidy_hash.calculate_clang_tidy_hash()

    assert result == expected_hash


def test_read_stored_hash_exists(tmp_path: Path) -> None:
    """Test reading hash when file exists."""
    stored_hash = "abc123def456"
    hash_file = tmp_path / ".clang-tidy.hash"
    hash_file.write_text(f"{stored_hash}\n")

    with (
        patch("clang_tidy_hash.Path") as mock_path_class,
        patch("clang_tidy_hash.read_file_lines", return_value=[f"{stored_hash}\n"]),
    ):
        # Mock the path calculation and exists check
        mock_hash_file = Mock()
        mock_hash_file.exists.return_value = True
        mock_path_class.return_value.parent.parent.__truediv__.return_value = (
            mock_hash_file
        )

        result = clang_tidy_hash.read_stored_hash()

    assert result == stored_hash


def test_read_stored_hash_not_exists() -> None:
    """Test reading hash when file doesn't exist."""
    with patch("clang_tidy_hash.Path") as mock_path_class:
        # Mock the path calculation and exists check
        mock_hash_file = Mock()
        mock_hash_file.exists.return_value = False
        mock_path_class.return_value.parent.parent.__truediv__.return_value = (
            mock_hash_file
        )

        result = clang_tidy_hash.read_stored_hash()

    assert result is None


def test_write_hash() -> None:
    """Test writing hash to file."""
    hash_value = "abc123def456"

    with patch("clang_tidy_hash.write_file_content") as mock_write:
        clang_tidy_hash.write_hash(hash_value)

        # Verify write_file_content was called with correct parameters
        mock_write.assert_called_once()
        args = mock_write.call_args[0]
        assert str(args[0]).endswith(".clang-tidy.hash")
        assert args[1] == hash_value.strip() + "\n"


@pytest.mark.parametrize(
    ("args", "current_hash", "stored_hash", "expected_exit"),
    [
        (["--check"], "abc123", "abc123", 1),  # Hashes match, no scan needed
        (["--check"], "abc123", "def456", 0),  # Hashes differ, scan needed
        (["--check"], "abc123", None, 0),  # No stored hash, scan needed
    ],
)
def test_main_check_mode(
    args: list[str], current_hash: str, stored_hash: str | None, expected_exit: int
) -> None:
    """Test main function in check mode."""
    with (
        patch("sys.argv", ["clang_tidy_hash.py"] + args),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
        pytest.raises(SystemExit) as exc_info,
    ):
        clang_tidy_hash.main()

    assert exc_info.value.code == expected_exit


def test_main_update_mode(capsys: pytest.CaptureFixture[str]) -> None:
    """Test main function in update mode."""
    current_hash = "abc123"

    with (
        patch("sys.argv", ["clang_tidy_hash.py", "--update"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.write_hash") as mock_write,
    ):
        clang_tidy_hash.main()

    mock_write.assert_called_once_with(current_hash)
    captured = capsys.readouterr()
    assert f"Hash updated: {current_hash}" in captured.out


@pytest.mark.parametrize(
    ("current_hash", "stored_hash"),
    [
        ("abc123", "def456"),  # Hash changed, should update
        ("abc123", None),  # No stored hash, should update
    ],
)
def test_main_update_if_changed_mode_update(
    current_hash: str, stored_hash: str | None, capsys: pytest.CaptureFixture[str]
) -> None:
    """Test main function in update-if-changed mode when update is needed."""
    with (
        patch("sys.argv", ["clang_tidy_hash.py", "--update-if-changed"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
        patch("clang_tidy_hash.write_hash") as mock_write,
        pytest.raises(SystemExit) as exc_info,
    ):
        clang_tidy_hash.main()

    assert exc_info.value.code == 0
    mock_write.assert_called_once_with(current_hash)
    captured = capsys.readouterr()
    assert "Clang-tidy hash updated" in captured.out


def test_main_update_if_changed_mode_no_update(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Test main function in update-if-changed mode when no update is needed."""
    current_hash = "abc123"
    stored_hash = "abc123"

    with (
        patch("sys.argv", ["clang_tidy_hash.py", "--update-if-changed"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
        patch("clang_tidy_hash.write_hash") as mock_write,
        pytest.raises(SystemExit) as exc_info,
    ):
        clang_tidy_hash.main()

    assert exc_info.value.code == 0
    mock_write.assert_not_called()
    captured = capsys.readouterr()
    assert "Clang-tidy hash unchanged" in captured.out


def test_main_verify_mode_success(capsys: pytest.CaptureFixture[str]) -> None:
    """Test main function in verify mode when verification passes."""
    current_hash = "abc123"
    stored_hash = "abc123"

    with (
        patch("sys.argv", ["clang_tidy_hash.py", "--verify"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
    ):
        clang_tidy_hash.main()
        captured = capsys.readouterr()
        assert "Hash verification passed" in captured.out


@pytest.mark.parametrize(
    ("current_hash", "stored_hash"),
    [
        ("abc123", "def456"),  # Hashes differ, verification fails
        ("abc123", None),  # No stored hash, verification fails
    ],
)
def test_main_verify_mode_failure(
    current_hash: str, stored_hash: str | None, capsys: pytest.CaptureFixture[str]
) -> None:
    """Test main function in verify mode when verification fails."""
    with (
        patch("sys.argv", ["clang_tidy_hash.py", "--verify"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
        pytest.raises(SystemExit) as exc_info,
    ):
        clang_tidy_hash.main()

    assert exc_info.value.code == 1
    captured = capsys.readouterr()
    assert "ERROR: Clang-tidy configuration has changed" in captured.out


def test_main_default_mode(capsys: pytest.CaptureFixture[str]) -> None:
    """Test main function in default mode (no arguments)."""
    current_hash = "abc123"
    stored_hash = "def456"

    with (
        patch("sys.argv", ["clang_tidy_hash.py"]),
        patch("clang_tidy_hash.calculate_clang_tidy_hash", return_value=current_hash),
        patch("clang_tidy_hash.read_stored_hash", return_value=stored_hash),
    ):
        clang_tidy_hash.main()

    captured = capsys.readouterr()
    assert f"Current hash: {current_hash}" in captured.out
    assert f"Stored hash: {stored_hash}" in captured.out
    assert "Match: False" in captured.out


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


def test_write_file_content(tmp_path: Path) -> None:
    """Test write_file_content helper function."""
    test_file = tmp_path / "test.txt"
    test_content = "test content"

    clang_tidy_hash.write_file_content(test_file, test_content)

    assert test_file.read_text() == test_content


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
