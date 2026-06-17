"""Tests for esphome.util module."""

from __future__ import annotations

from collections.abc import Callable
import io
from pathlib import Path
import subprocess
import sys
from typing import Any
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


def _make_redirect(
    line_callbacks: list[Callable[[str], str | None]] | None = None,
    filter_lines: list[str] | None = None,
) -> tuple[util.RedirectText, io.StringIO]:
    """Create a RedirectText that writes to a StringIO buffer."""
    buf = io.StringIO()
    redirect = util.RedirectText(
        buf, filter_lines=filter_lines, line_callbacks=line_callbacks
    )
    return redirect, buf


def test_redirect_text_callback_called_on_matching_line() -> None:
    """Test that a line callback is called and its output is written."""
    results: list[str] = []

    def callback(line: str) -> str | None:
        results.append(line)
        if "target" in line:
            return "CALLBACK OUTPUT\n"
        return None

    redirect, buf = _make_redirect(line_callbacks=[callback])
    redirect.write("some target line\n")

    assert "some target line" in buf.getvalue()
    assert "CALLBACK OUTPUT" in buf.getvalue()
    assert len(results) == 1


def test_redirect_text_callback_not_triggered_on_non_matching_line() -> None:
    """Test that callback returns None for non-matching lines."""

    def callback(line: str) -> str | None:
        if "target" in line:
            return "FOUND\n"
        return None

    redirect, buf = _make_redirect(line_callbacks=[callback])
    redirect.write("no match here\n")

    assert "no match here" in buf.getvalue()
    assert "FOUND" not in buf.getvalue()


def test_redirect_text_callback_works_without_filter_pattern() -> None:
    """Test that callbacks fire even when no filter_lines is set."""

    def callback(line: str) -> str | None:
        if "Crystal" in line:
            return "WARNING: mismatch\n"
        return None

    redirect, buf = _make_redirect(line_callbacks=[callback])
    redirect.write("Crystal frequency:  26MHz\n")

    assert "Crystal frequency:  26MHz" in buf.getvalue()
    assert "WARNING: mismatch" in buf.getvalue()


def test_redirect_text_callback_works_with_filter_pattern() -> None:
    """Test that callbacks fire alongside filter patterns."""

    def callback(line: str) -> str | None:
        if "important" in line:
            return "NOTED\n"
        return None

    redirect, buf = _make_redirect(
        line_callbacks=[callback],
        filter_lines=[r"^skip this.*"],
    )
    redirect.write("skip this line\n")
    redirect.write("important line\n")

    assert "skip this" not in buf.getvalue()
    assert "important line" in buf.getvalue()
    assert "NOTED" in buf.getvalue()


def test_redirect_text_multiple_callbacks() -> None:
    """Test that multiple callbacks are all invoked."""

    def callback_a(line: str) -> str | None:
        if "test" in line:
            return "FROM A\n"
        return None

    def callback_b(line: str) -> str | None:
        if "test" in line:
            return "FROM B\n"
        return None

    redirect, buf = _make_redirect(line_callbacks=[callback_a, callback_b])
    redirect.write("test line\n")

    output = buf.getvalue()
    assert "FROM A" in output
    assert "FROM B" in output


def test_redirect_text_incomplete_line_buffered() -> None:
    """Test that incomplete lines are buffered until newline."""
    results: list[str] = []

    def callback(line: str) -> str | None:
        results.append(line)
        return None

    redirect, buf = _make_redirect(line_callbacks=[callback])
    redirect.write("partial")
    assert len(results) == 0

    redirect.write(" line\n")
    assert len(results) == 1
    assert results[0] == "partial line"


def test_run_external_command_line_callbacks(capsys: pytest.CaptureFixture) -> None:
    """Test that run_external_command passes line_callbacks to RedirectText."""
    results: list[str] = []

    def callback(line: str) -> str | None:
        results.append(line)
        if "hello" in line:
            return "CALLBACK FIRED\n"
        return None

    def fake_main() -> int:
        print("hello world")
        return 0

    rc = util.run_external_command(fake_main, "fake", line_callbacks=[callback])

    assert rc == 0
    assert len(results) == 1
    assert "hello world" in results[0]
    captured = capsys.readouterr()
    assert "CALLBACK FIRED" in captured.out


def test_run_external_process_line_callbacks() -> None:
    """Test that run_external_process passes line_callbacks to RedirectText."""
    results: list[str] = []

    def callback(line: str) -> str | None:
        results.append(line)
        if "from subprocess" in line:
            return "PROCESS CALLBACK\n"
        return None

    with patch("esphome.util.subprocess.run") as mock_run:

        def run_side_effect(*args: Any, **kwargs: Any) -> MagicMock:
            # Simulate subprocess writing to the stdout RedirectText
            stdout = kwargs.get("stdout")
            if stdout is not None and isinstance(stdout, util.RedirectText):
                stdout.write("from subprocess\n")
            return MagicMock(returncode=0)

        mock_run.side_effect = run_side_effect

        rc = util.run_external_process(
            "echo",
            "test",
            line_callbacks=[callback],
        )

    assert rc == 0
    assert any("from subprocess" in r for r in results)


def test_get_picotool_path_found(tmp_path: Path) -> None:
    """Test picotool path derivation from cc_path."""
    # Create the expected directory structure
    packages_dir = tmp_path / "packages"
    toolchain_dir = packages_dir / "toolchain-rp2040-earlephilhower" / "bin"
    toolchain_dir.mkdir(parents=True)
    gcc = toolchain_dir / "arm-none-eabi-gcc"
    gcc.touch()

    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    picotool_dir = packages_dir / "tool-picotool-rp2040-earlephilhower"
    picotool_dir.mkdir(parents=True)
    picotool = picotool_dir / binary_name
    picotool.touch()

    result = util.get_picotool_path(str(gcc))
    assert result == picotool


def test_get_picotool_path_not_found(tmp_path: Path) -> None:
    """Test picotool path returns None when not installed."""
    packages_dir = tmp_path / "packages"
    toolchain_dir = packages_dir / "toolchain-rp2040-earlephilhower" / "bin"
    toolchain_dir.mkdir(parents=True)
    gcc = toolchain_dir / "arm-none-eabi-gcc"
    gcc.touch()

    result = util.get_picotool_path(str(gcc))
    assert result is None


def test_get_picotool_path_windows(tmp_path: Path) -> None:
    """Test picotool path uses .exe on Windows."""
    packages_dir = tmp_path / "packages"
    toolchain_dir = packages_dir / "toolchain-rp2040-earlephilhower" / "bin"
    toolchain_dir.mkdir(parents=True)
    gcc = toolchain_dir / "arm-none-eabi-gcc.exe"
    gcc.touch()

    picotool_dir = packages_dir / "tool-picotool-rp2040-earlephilhower"
    picotool_dir.mkdir(parents=True)
    picotool = picotool_dir / "picotool.exe"
    picotool.touch()

    with patch("esphome.util.sys.platform", "win32"):
        result = util.get_picotool_path(str(gcc))
    assert result == picotool


def test_detect_rp2040_bootsel_found() -> None:
    """Test BOOTSEL device detection when device is present."""
    mock_result = MagicMock()
    mock_result.stdout = b"Device Information\n type: RP2040\n"
    with patch("esphome.util.subprocess.run", return_value=mock_result):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 1
    assert result.permission_error is False


def test_detect_rp2040_bootsel_multiple() -> None:
    """Test BOOTSEL detection with multiple devices."""
    mock_result = MagicMock()
    mock_result.stdout = b"type: RP2040\ntype: RP2350\n"
    with patch("esphome.util.subprocess.run", return_value=mock_result):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 2
    assert result.permission_error is False


def test_detect_rp2040_bootsel_none() -> None:
    """Test BOOTSEL detection when no device found."""
    mock_result = MagicMock()
    mock_result.stdout = (
        b"No accessible RP2040/RP2350 devices in BOOTSEL mode were found.\n"
    )
    mock_result.stderr = b""
    with patch("esphome.util.subprocess.run", return_value=mock_result):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 0
    assert result.permission_error is False


def test_detect_rp2040_bootsel_permission_error() -> None:
    """Test BOOTSEL detection with device found but not accessible."""
    mock_result = MagicMock()
    mock_result.stdout = (
        b"No accessible RP-series devices in BOOTSEL mode were found.\n"
    )
    mock_result.stderr = (
        b"RP2040 device at bus 5, address 24 appears to be in BOOTSEL mode, "
        b"but picotool was unable to connect. "
        b"Maybe try 'sudo' or check your permissions.\n"
    )
    with patch("esphome.util.subprocess.run", return_value=mock_result):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 0
    assert result.permission_error is True


def test_detect_rp2040_bootsel_libusb_access_error() -> None:
    """Test BOOTSEL detection with LIBUSB_ERROR_ACCESS."""
    mock_result = MagicMock()
    mock_result.stdout = b""
    mock_result.stderr = b"LIBUSB_ERROR_ACCESS\n"
    with patch("esphome.util.subprocess.run", return_value=mock_result):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 0
    assert result.permission_error is True


def test_detect_rp2040_bootsel_oserror() -> None:
    """Test BOOTSEL detection handles OSError."""
    with patch("esphome.util.subprocess.run", side_effect=OSError("not found")):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 0
    assert result.permission_error is False


def test_detect_rp2040_bootsel_timeout() -> None:
    """Test BOOTSEL detection handles timeout."""
    with patch(
        "esphome.util.subprocess.run",
        side_effect=subprocess.TimeoutExpired("picotool", 10),
    ):
        result = util.detect_rp2040_bootsel("/usr/bin/picotool")
    assert result.device_count == 0
    assert result.permission_error is False


class TestSafePrint:
    """Tests for ``safe_print`` and its UnicodeEncodeError fallback chain."""

    @pytest.fixture(autouse=True)
    def _no_dashboard(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Default ``CORE.dashboard`` to False so each test starts hermetic."""
        from esphome.core import CORE

        monkeypatch.setattr(CORE, "dashboard", False)

    def test_prints_plain_message(self, capsys: pytest.CaptureFixture[str]) -> None:
        """ASCII-only messages take the fast path through native ``print``."""
        util.safe_print("hello world")
        assert capsys.readouterr().out == "hello world\n"

    def test_prints_unicode_on_utf8_stdout(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Non-ASCII goes straight through when stdout can encode it."""
        util.safe_print("bars: \u2582\u2584\u2586\u2588")
        assert capsys.readouterr().out == "bars: \u2582\u2584\u2586\u2588\n"

    def test_dashboard_escapes_esc_byte(
        self,
        capsys: pytest.CaptureFixture[str],
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        r"""Dashboard mode escapes raw ``\033`` ESC bytes to literal ``\\033``."""
        from esphome.core import CORE

        monkeypatch.setattr(CORE, "dashboard", True)
        util.safe_print("\033[0;32mhi\033[0m")
        assert capsys.readouterr().out == "\\033[0;32mhi\\033[0m\n"

    def test_fallback_writes_string_not_bytes_repr(
        self, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Regression: cp1252 fallback must produce a printable str, not ``b'...'``.

        On Windows, when stdout is a redirected pipe (e.g. the dashboard),
        Python uses cp1252, which cannot encode the wifi signal-bar block
        characters (U+2582..U+2588). The previous fallback path called
        ``print(message.encode(...))`` with a ``bytes`` object, which
        Python's ``print`` rendered as a literal ``b'...'`` repr — visible
        in the user's dashboard output. The fix re-encodes through the
        stream's encoding with ``backslashreplace`` and decodes back to
        ``str``.
        """
        buf = io.BytesIO()
        cp1252_stream = io.TextIOWrapper(buf, encoding="cp1252", errors="strict")
        monkeypatch.setattr(sys, "stdout", cp1252_stream)

        util.safe_print("bars: \u2582\u2584\u2586\u2588 done")
        cp1252_stream.flush()
        output = buf.getvalue().decode("cp1252")

        # Output is a clean line, not the bytes repr.
        assert not output.startswith("b'")
        assert "b'bars" not in output
        # Unencodable codepoints become readable backslash escapes.
        assert "\\u2582\\u2584\\u2586\\u2588" in output
        # Encodable parts survive unchanged.
        assert "bars: " in output
        assert " done" in output
        assert output.endswith("\n")

    def test_fallback_with_dashboard_escaped_message(
        self, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Dashboard ESC escaping + cp1252 fallback compose correctly."""
        from esphome.core import CORE

        monkeypatch.setattr(CORE, "dashboard", True)
        buf = io.BytesIO()
        cp1252_stream = io.TextIOWrapper(buf, encoding="cp1252", errors="strict")
        monkeypatch.setattr(sys, "stdout", cp1252_stream)

        util.safe_print("\033[0;32m\u2582\u2584\u2586\u2588\033[0m")
        cp1252_stream.flush()
        output = buf.getvalue().decode("cp1252")

        # Dashboard escaping turned ESC into literal "\033" (5 chars), which
        # cp1252 can encode, so it survives the round-trip verbatim.
        assert "\\033[0;32m" in output
        assert "\\033[0m" in output
        # Block characters became backslash escapes via backslashreplace.
        assert "\\u2582\\u2584\\u2586\\u2588" in output

    def test_final_message_when_locale_is_invalid(
        self,
        monkeypatch: pytest.MonkeyPatch,
        capsys: pytest.CaptureFixture[str],
    ) -> None:
        """If every encoding path fails, surface the locale-error sentinel."""
        original_print = print
        call_count = 0

        def fake_print(*args: Any, **kwargs: Any) -> None:
            nonlocal call_count
            call_count += 1
            # The first three calls are: native print, stream-encoding
            # fallback, ASCII fallback. Make all three raise so we reach
            # the final sentinel "Cannot print line..." which is expected
            # to succeed (no encoding required).
            if call_count <= 3:
                raise UnicodeEncodeError("ascii", "x", 0, 1, "boom")
            original_print(*args, **kwargs)

        monkeypatch.setattr("builtins.print", fake_print)
        util.safe_print("x")
        assert call_count == 4
        assert (
            capsys.readouterr().out == "Cannot print line because of invalid locale!\n"
        )
