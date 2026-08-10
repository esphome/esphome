import os
from pathlib import Path
import pty
import subprocess
import sys

import pytest

from esphome.log import AnsiFore, AnsiStyle, color


def _probe_command(fixture_path: Path, *args: str) -> list[str]:
    """Build the command line for the setup_log probe fixture script."""
    return [sys.executable, str(fixture_path / "log" / "setup_log_probe.py"), *args]


def _probe_env() -> dict[str, str]:
    """Running a script file drops the cwd from sys.path; add the repo root."""
    python_path = str(Path(__file__).parents[2])
    if ambient := os.environ.get("PYTHONPATH"):
        python_path = os.pathsep.join((python_path, ambient))
    return os.environ | {"PYTHONPATH": python_path}


def test_color_keep_returns_unchanged_message() -> None:
    """Test that AnsiFore.KEEP returns the message unchanged."""
    msg = "test message"
    result = color(AnsiFore.KEEP, msg)
    assert result == msg


def test_color_keep_ignores_reset_parameter() -> None:
    """Test that reset parameter is ignored when using AnsiFore.KEEP."""
    msg = "test message"
    result_with_reset = color(AnsiFore.KEEP, msg, reset=True)
    result_without_reset = color(AnsiFore.KEEP, msg, reset=False)
    assert result_with_reset == msg
    assert result_without_reset == msg


def test_color_applies_color_code() -> None:
    """Test that color codes are properly applied to messages."""
    msg = "test message"
    result = color(AnsiFore.RED, msg, reset=False)
    assert result == f"{AnsiFore.RED.value}{msg}"


def test_color_applies_reset_when_requested() -> None:
    """Test that RESET_ALL is added when reset=True."""
    msg = "test message"
    result = color(AnsiFore.GREEN, msg, reset=True)
    expected = f"{AnsiFore.GREEN.value}{msg}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


def test_color_no_reset_when_not_requested() -> None:
    """Test that RESET_ALL is not added when reset=False."""
    msg = "test message"
    result = color(AnsiFore.BLUE, msg, reset=False)
    expected = f"{AnsiFore.BLUE.value}{msg}"
    assert result == expected


def test_color_with_empty_message() -> None:
    """Test color function with empty message."""
    result = color(AnsiFore.YELLOW, "", reset=True)
    expected = f"{AnsiFore.YELLOW.value}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


@pytest.mark.parametrize(
    "col",
    [
        AnsiFore.BLACK,
        AnsiFore.RED,
        AnsiFore.GREEN,
        AnsiFore.YELLOW,
        AnsiFore.BLUE,
        AnsiFore.MAGENTA,
        AnsiFore.CYAN,
        AnsiFore.WHITE,
        AnsiFore.RESET,
    ],
)
def test_all_ansi_colors(col: AnsiFore) -> None:
    """Test that all AnsiFore colors work correctly."""
    msg = "test"
    result = color(col, msg, reset=True)
    expected = f"{col.value}{msg}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


def test_ansi_fore_keep_is_enum_member() -> None:
    """Ensure AnsiFore.KEEP is an Enum member and evaluates to truthy."""
    assert isinstance(AnsiFore.KEEP, AnsiFore)
    # Enum members are truthy, even with empty string values
    assert bool(AnsiFore.KEEP) is True
    # But the value itself is still an empty string
    assert AnsiFore.KEEP.value == ""


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_redirected_output_strips_ansi(fixture_path: Path) -> None:
    """A redirected run must keep colorama so ANSI codes are stripped."""
    result = subprocess.run(
        _probe_command(fixture_path),
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
        env=_probe_env(),
    )
    assert result.returncode == 0, result.stderr
    assert "colorama_loaded=True" in result.stdout
    assert "red end" in result.stdout
    assert "\033" not in result.stdout


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_dashboard_skips_colorama(fixture_path: Path) -> None:
    """Dashboard runs escape their color codes, so colorama must not load."""
    result = subprocess.run(
        _probe_command(fixture_path, "--dashboard"),
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
        env=_probe_env(),
    )
    assert result.returncode == 0, result.stderr
    assert "colorama_loaded=False" in result.stdout
    # Codes pass through untouched for the dashboard to handle.
    assert "\033[31mred\033[0m end" in result.stdout


@pytest.mark.skipif(
    sys.platform == "win32", reason="pty is POSIX-only; colorama loads on Windows"
)
def test_setup_log_tty_skips_colorama(fixture_path: Path) -> None:
    """A terminal run must skip colorama and keep ANSI codes intact."""
    controller, follower = pty.openpty()
    proc = subprocess.Popen(
        _probe_command(fixture_path),
        stdout=follower,
        stderr=follower,
        stdin=follower,
        env=_probe_env(),
    )
    os.close(follower)
    output = b""
    try:
        while chunk := os.read(controller, 1024):
            output += chunk
    except OSError:
        # macOS raises EIO once the child closes its end of the pty.
        pass
    finally:
        os.close(controller)
    assert proc.wait(60) == 0
    text = output.decode()
    assert "colorama_loaded=False" in text
    assert "\033[31mred\033[0m end" in text
