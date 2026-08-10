from collections.abc import Generator
import io
import logging
import os
from pathlib import Path
import select
import subprocess
import sys
import time

import pytest

from esphome.core import CORE
from esphome.log import AnsiFore, AnsiStyle, color, setup_log


class _FakeTty(io.StringIO):
    def isatty(self) -> bool:
        return True


@pytest.fixture
def restore_logging_state() -> Generator[None, None, None]:
    """Undo the global logging changes setup_log() makes."""
    root = logging.getLogger()
    handlers = root.handlers[:]
    formatters = [handler.formatter for handler in handlers]
    level = root.level
    urllib3_level = logging.getLogger("urllib3").level
    yield
    root.handlers[:] = handlers
    for handler, formatter in zip(handlers, formatters, strict=True):
        handler.setFormatter(formatter)
    root.setLevel(level)
    logging.getLogger("urllib3").setLevel(urllib3_level)


def _probe_command(fixture_path: Path, *args: str) -> list[str]:
    """Build the command line for the setup_log probe fixture script."""
    return [sys.executable, str(fixture_path / "log" / "setup_log_probe.py"), *args]


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
def test_setup_log_redirected_output_strips_ansi(
    fixture_path: Path, probe_env: dict[str, str]
) -> None:
    """A redirected run must keep colorama so ANSI codes are stripped."""
    result = subprocess.run(
        _probe_command(fixture_path),
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
        env=probe_env,
    )
    assert result.returncode == 0, result.stderr
    assert "colorama_loaded=True" in result.stdout
    assert "red end" in result.stdout
    assert "\033" not in result.stdout


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_dashboard_skips_colorama(
    fixture_path: Path, probe_env: dict[str, str]
) -> None:
    """Dashboard runs escape their color codes, so colorama must not load."""
    result = subprocess.run(
        _probe_command(fixture_path, "--dashboard"),
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
        env=probe_env,
    )
    assert result.returncode == 0, result.stderr
    assert "colorama_loaded=False" in result.stdout
    # Codes pass through untouched for the dashboard to handle.
    assert "\033[31mred\033[0m end" in result.stdout


@pytest.mark.skipif(
    sys.platform == "win32", reason="pty is POSIX-only; colorama loads on Windows"
)
def test_setup_log_tty_skips_colorama(
    fixture_path: Path, probe_env: dict[str, str]
) -> None:
    """A terminal run must skip colorama and keep ANSI codes intact."""
    # Unix-only; a module-level import would break test collection on
    # Windows, where the whole module is skipped anyway.
    import pty

    controller, follower = pty.openpty()
    proc = None
    output = b""
    deadline = time.monotonic() + 60
    try:
        try:
            proc = subprocess.Popen(
                _probe_command(fixture_path),
                stdout=follower,
                stderr=follower,
                stdin=follower,
                env=probe_env,
            )
        finally:
            os.close(follower)
        while True:
            timeout = deadline - time.monotonic()
            if timeout <= 0 or not select.select([controller], [], [], timeout)[0]:
                pytest.fail(f"pty probe produced no EOF in time; got {output!r}")
            try:
                chunk = os.read(controller, 1024)
            except OSError:
                # macOS raises EIO once the child closes its end of the pty.
                break
            if not chunk:
                break
            output += chunk
        assert proc.wait(60) == 0
    finally:
        os.close(controller)
        if proc is not None and proc.poll() is None:
            proc.kill()
            proc.wait()
    text = output.decode()
    assert "colorama_loaded=False" in text
    assert "\033[31mred\033[0m end" in text


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_dashboard_branch_skips_colorama_import(
    monkeypatch: pytest.MonkeyPatch, restore_logging_state: None
) -> None:
    """The dashboard side of the guard must not import colorama."""
    monkeypatch.delitem(sys.modules, "colorama", raising=False)
    monkeypatch.setattr(CORE, "dashboard", True)
    monkeypatch.setattr(CORE, "verbose", CORE.verbose)
    monkeypatch.setattr(CORE, "quiet", CORE.quiet)
    setup_log()
    assert "colorama" not in sys.modules


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_tty_branch_skips_colorama_import(
    monkeypatch: pytest.MonkeyPatch, restore_logging_state: None
) -> None:
    """The tty side of the guard must not import colorama."""
    monkeypatch.delitem(sys.modules, "colorama", raising=False)
    monkeypatch.setattr(CORE, "verbose", CORE.verbose)
    monkeypatch.setattr(CORE, "quiet", CORE.quiet)
    monkeypatch.setattr(sys, "stdout", _FakeTty())
    monkeypatch.setattr(sys, "stderr", _FakeTty())
    setup_log()
    assert "colorama" not in sys.modules


@pytest.mark.skipif(
    sys.platform == "win32", reason="colorama always initializes on Windows"
)
def test_setup_log_redirected_branch_imports_colorama(
    monkeypatch: pytest.MonkeyPatch, restore_logging_state: None
) -> None:
    """Redirected streams must keep importing and initializing colorama."""
    monkeypatch.setattr(CORE, "verbose", CORE.verbose)
    monkeypatch.setattr(CORE, "quiet", CORE.quiet)
    monkeypatch.setattr(sys, "stdout", io.StringIO())
    monkeypatch.setattr(sys, "stderr", io.StringIO())
    setup_log()
    try:
        assert "colorama" in sys.modules
    finally:
        # init() rebinds sys.stdout/stderr; restore them before monkeypatch
        # puts the originals back.
        sys.modules["colorama"].deinit()
