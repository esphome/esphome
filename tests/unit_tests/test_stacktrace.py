"""Tests for esphome.stacktrace."""

from __future__ import annotations

from unittest.mock import Mock, patch

from esphome import stacktrace
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}


def _run(
    handler,
    platform: str = "esp32",
    lines: tuple[str, ...] = ("PC: 0x4010496e",),
) -> stacktrace.LogLineProcessor:
    """Processor with the resolver stubbed, fed the given lines."""
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, platform)
    for line in lines:
        processor.process_line(line)
    return processor


def _fed(handler) -> list[str]:
    return [call.args[1] for call in handler.call_args_list]


def _warnings(caplog) -> list[str]:
    return [r.message for r in caplog.records if r.levelname == "WARNING"]


def test_decoder_contains_failures_and_short_circuits() -> None:
    """One decode failure is contained and never retried.

    aioesphomeapi isolates exceptions raised by log handlers, so an
    escaping one logs a full traceback for every line it fires on; and
    _decode_pc shells out to the toolchain, so retrying it per backtrace
    line would stall streaming.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _run(
        handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e", "BT1: 0x401049aa")
    )

    assert handler.call_count == 1
    assert processor.backtrace_state is False


def test_decoder_swallows_non_esphome_error() -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as FileNotFoundError from the
    toolchain subprocess; it must disable decoding exactly like an
    EsphomeError does.
    """
    handler = Mock(
        side_effect=FileNotFoundError(2, "No such file or directory", "/build")
    )
    processor = _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e"))

    assert handler.call_count == 1
    assert processor.backtrace_state is False


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """_run_idedata raises EsphomeError with no message; the warning
    must show a useful explanation rather than empty parens.
    """
    _run(Mock(side_effect=EsphomeError()))

    warnings = _warnings(caplog)
    assert any("build artifacts not found locally" in m for m in warnings)
    assert not any("()" in m for m in warnings)


def test_decoder_bug_with_empty_message_names_the_type(caplog) -> None:
    """A zero-message decoder bug must not masquerade as missing artifacts.

    The recompile hint is only right for EsphomeError from _run_idedata;
    anything else is ESPHome's own bug and says so instead of sending
    the user down a dead-end remediation path.
    """
    _run(Mock(side_effect=IndexError()))

    warnings = _warnings(caplog)
    assert any("IndexError" in m and "this is a bug" in m for m in warnings)
    assert not any("esphome compile" in m for m in warnings)


def test_state_threads_between_lines() -> None:
    """backtrace_state carries from one decoded line to the next."""
    handler = Mock(side_effect=[True, True])
    processor = _run(
        handler,
        platform="esp8266",
        lines=(">>>stack>>>", "3ffffe10: 40201234 3ffe8410 00000000 40201000"),
    )

    assert _fed(handler) == [
        ">>>stack>>>",
        "3ffffe10: 40201234 3ffe8410 00000000 40201000",
    ]
    assert handler.call_args_list[0].args[2] is False
    assert handler.call_args_list[1].args[2] is True
    assert processor.backtrace_state is True


def test_no_analyzer_disables_decoding(caplog) -> None:
    """Platforms without an analyzer report at session start and stay quiet."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    processor = stacktrace.LogLineProcessor(CONFIG, "bk72xx")
    processor.process_line("PC: 0x40104960")

    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False
