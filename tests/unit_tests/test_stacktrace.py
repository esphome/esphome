"""Tests for esphome.stacktrace."""

from __future__ import annotations

from unittest.mock import Mock, patch

from esphome import stacktrace
from esphome.const import PLATFORM_BK72XX, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}


def _run(
    handler,
    platform: str = PLATFORM_ESP32,
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


def test_resolution_failure_is_contained(caplog) -> None:
    """A platform package broken in an unanticipated way must not kill
    the session; decoding degrades with a warning like any other failure.
    """
    with patch.object(
        stacktrace.platform_hooks,
        "get_stacktrace_handler",
        side_effect=RuntimeError("boom"),
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_ESP32)
        processor.process_line("PC: 0x4010496e")

    assert processor.backtrace_state is False
    assert any("could not be loaded" in m for m in _warnings(caplog))


def test_decoder_swallows_os_error_with_remediation_hint(caplog) -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as an OSError; that is the
    user's environment, not a decoder bug, so it disables decoding
    like an EsphomeError does and keeps the recompile hint.
    """
    handler = Mock(
        side_effect=FileNotFoundError(2, "No such file or directory", "/build")
    )
    processor = _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e"))

    assert handler.call_count == 1
    assert processor.backtrace_state is False
    warnings = _warnings(caplog)
    assert any("esphome compile" in m for m in warnings)
    assert not any("this is a bug" in m for m in warnings)


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """A message-less EsphomeError must show a useful explanation.

    Defensive: the in-tree idedata raise sites all carry a message now,
    but a bare EsphomeError from elsewhere must not render as parens.
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


def test_decoder_bug_warning_keeps_the_type_with_a_message(caplog) -> None:
    """The type must survive a non-empty message; a bare KeyError message
    like 'prog_path' reads as a raised string in a bug report paste.
    """
    _run(Mock(side_effect=KeyError("prog_path")))

    warnings = _warnings(caplog)
    assert any("KeyError: 'prog_path'" in m for m in warnings)


def test_state_threads_between_lines() -> None:
    """backtrace_state carries from one decoded line to the next."""
    handler = Mock(side_effect=[True, True])
    processor = _run(
        handler,
        platform=PLATFORM_ESP8266,
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
    processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_BK72XX)
    processor.process_line("PC: 0x40104960")

    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False
