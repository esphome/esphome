"""Tests for esphome.components.api.client."""

from __future__ import annotations

from unittest.mock import patch

from esphome.components import esp32
from esphome.components.api import client as api_client
from esphome.core import EsphomeError


def test_decoder_swallows_esphome_error() -> None:
    """A failing stack-trace decode must not propagate.

    aioesphomeapi isolates exceptions raised by log handlers, so an
    escaping one logs a full traceback for every line it fires on rather
    than being reported once as an unavailable decoder.
    """
    config = {"esphome": {"name": "test"}}

    with patch.object(
        esp32, "process_stacktrace", side_effect=EsphomeError("no idedata")
    ) as mock_process:
        processor = api_client._LogLineProcessor(config, esp32.process_stacktrace)
        processor.process_line("PC: 0x4010496e")

    assert mock_process.called
    assert processor.backtrace_state is False


def test_decoder_swallows_platform_handler_error() -> None:
    """The same protection must apply to the platform-specific handler."""
    config = {"esphome": {"name": "test"}}

    def platform_handler(_config, _line, _state):
        raise EsphomeError("no idedata")

    processor = api_client._LogLineProcessor(config, platform_handler)
    processor.process_line("PC: 0x4010496e")

    assert processor.backtrace_state is False


def test_decoder_swallows_non_esphome_error() -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as FileNotFoundError from the toolchain
    subprocess. aioesphomeapi isolates it, so the session survives, but it logs
    a traceback for every PC/BT line and decoding is never disabled, which
    buries the crash dump the user is trying to read.
    """
    config = {"esphome": {"name": "test"}}

    with patch.object(
        esp32,
        "process_stacktrace",
        side_effect=FileNotFoundError(
            2, "No such file or directory", "/build/ol/build"
        ),
    ) as mock_process:
        processor = api_client._LogLineProcessor(config, esp32.process_stacktrace)
        processor.process_line("PC: 0x4010496e")
        processor.process_line("BT0: 0x4010496e")

    # Disabled after the first failure rather than retried per backtrace line.
    assert mock_process.call_count == 1
    assert processor.backtrace_state is False


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """_run_idedata raises EsphomeError with no message; the warning
    must show a useful explanation rather than empty parens.
    """
    config = {"esphome": {"name": "test"}}

    with patch.object(esp32, "process_stacktrace", side_effect=EsphomeError()):
        processor = api_client._LogLineProcessor(config, esp32.process_stacktrace)
        processor.process_line("PC: 0x4010496e")

    warnings = [r.message for r in caplog.records if r.levelname == "WARNING"]
    assert any("build artifacts not found locally" in m for m in warnings)
    assert not any("()" in m for m in warnings)


def test_decoder_short_circuits_after_failure() -> None:
    """After one failure, subsequent lines must not retry the decoder.

    _decode_pc shells out to the toolchain; a crash dump can contain many
    PC/BT lines and retrying the failing subprocess for each one would
    stall log streaming.
    """
    config = {"esphome": {"name": "test"}}

    with patch.object(
        esp32, "process_stacktrace", side_effect=EsphomeError("no idedata")
    ) as mock_process:
        processor = api_client._LogLineProcessor(config, esp32.process_stacktrace)
        processor.process_line("PC: 0x4010496e")
        processor.process_line("BT0: 0x4010496e")
        processor.process_line("BT1: 0x401049aa")

    assert mock_process.call_count == 1


def test_decoder_threads_backtrace_state() -> None:
    """When decoding succeeds, backtrace_state is threaded across calls."""
    config = {"esphome": {"name": "test"}}

    with patch.object(
        esp32, "process_stacktrace", side_effect=[True, False]
    ) as mock_process:
        processor = api_client._LogLineProcessor(config, esp32.process_stacktrace)
        processor.process_line(">>>stack>>>")
        assert processor.backtrace_state is True
        processor.process_line("<<<stack<<<")
        assert processor.backtrace_state is False

    assert not mock_process.call_args_list[0].args[-1]
    assert mock_process.call_args_list[1].args[-1]


def test_decoder_uses_platform_handler_when_provided() -> None:
    """The platform handler is preferred over the generic one."""
    config = {"esphome": {"name": "test"}}
    calls: list[tuple[object, str, bool]] = []

    def platform_handler(cfg, line, state):
        calls.append((cfg, line, state))
        return True

    processor = api_client._LogLineProcessor(config, platform_handler)

    with patch.object(esp32, "process_stacktrace") as mock_generic:
        processor.process_line("BT0: 0x4010496e")

    assert calls == [(config, "BT0: 0x4010496e", False)]
    assert mock_generic.called is False
    assert processor.backtrace_state is True
