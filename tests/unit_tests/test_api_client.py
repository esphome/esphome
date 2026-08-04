"""Tests for esphome.api_client."""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome import api_client
from esphome.const import (
    CONF_ENCRYPTION,
    CONF_KEY,
    CONF_PORT,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, EsphomeError

CONFIG = {"esphome": {"name": "test"}}

# One real dump line per registered platform plus the shapes the decoders
# key on; the gate must fire on every one of them or that platform's
# decoding silently never starts.
CRASH_LINES = [
    pytest.param(
        "Backtrace: 0x400d1a2c:0x3ffb1f60 0x400d2a3c:0x3ffb1f80", id="esp32-backtrace"
    ),
    pytest.param("PC      : 0x400d1a2c  PS      : 0x00060330", id="esp32-register"),
    pytest.param("BT0: 0x40104960", id="esp32-stored-backtrace"),
    pytest.param("epc1=0x40201234 epc2=0x00000000", id="esp8266-epc"),
    pytest.param(
        "3ffffe10: 40201234 3ffe8410 00000000 40201000", id="esp8266-stack-word"
    ),
    pytest.param("PC:  0x10001234 (fault location)", id="rp2-crash-handler"),
    pytest.param("PC=0x27a1c LR=0x1e33", id="nrf52-short-pointer"),
]

BENIGN_LINES = [
    pytest.param("[I][app:100] hello world", id="plain"),
    pytest.param("[C][wifi:400]   BSSID: AA:BB:CC:DD:EE:FF", id="mac"),
    pytest.param("[19:26:11.966][I][main:151]: version 2026.7.0-dev", id="timestamp"),
]


def test_component_shim_reexports_runtime_client() -> None:
    """The old import paths must keep working for external code."""
    from esphome.components import api
    from esphome.components.api import client as shim

    assert shim.run_logs is api_client.run_logs
    assert shim.async_run_logs is api_client.async_run_logs
    assert api.CONF_ENCRYPTION is CONF_ENCRYPTION


@pytest.mark.parametrize("line", CRASH_LINES)
def test_address_gate_fires_on_platform_dump_lines(line: str) -> None:
    assert api_client._ADDRESS_RE.search(line)


@pytest.mark.parametrize("line", BENIGN_LINES)
def test_address_gate_ignores_ordinary_lines(line: str) -> None:
    assert not api_client._ADDRESS_RE.search(line)


def _make_processor(handler) -> api_client.LogLineProcessor:
    """Processor for a decoder platform with the resolver stubbed out."""
    with patch.object(
        api_client.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = api_client.LogLineProcessor(CONFIG, "esp32")
        # Trigger lazy resolution while the stub is in place.
        processor.process_line("PC: 0x4010496e")
    return processor


def test_decoder_swallows_esphome_error() -> None:
    """A failing stack-trace decode must not propagate.

    aioesphomeapi isolates exceptions raised by log handlers, so an
    escaping one logs a full traceback for every line it fires on rather
    than being reported once as an unavailable decoder.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _make_processor(handler)

    assert handler.called
    assert processor.backtrace_state is False


def test_decoder_swallows_non_esphome_error() -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as FileNotFoundError from the toolchain
    subprocess. aioesphomeapi isolates it, so the session survives, but it logs
    a traceback for every PC/BT line and decoding is never disabled, which
    buries the crash dump the user is trying to read.
    """
    handler = Mock(
        side_effect=FileNotFoundError(2, "No such file or directory", "/build")
    )
    processor = _make_processor(handler)
    processor.process_line("BT0: 0x4010496e")

    # Disabled after the first failure rather than retried per backtrace line.
    assert handler.call_count == 1
    assert processor.backtrace_state is False


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """_run_idedata raises EsphomeError with no message; the warning
    must show a useful explanation rather than empty parens.
    """
    _make_processor(Mock(side_effect=EsphomeError()))

    warnings = [r.message for r in caplog.records if r.levelname == "WARNING"]
    assert any("build artifacts not found locally" in m for m in warnings)
    assert not any("()" in m for m in warnings)


def test_decoder_short_circuits_after_failure() -> None:
    """After one failure, subsequent lines must not retry the decoder.

    _decode_pc shells out to the toolchain; a crash dump can contain many
    PC/BT lines and retrying the failing subprocess for each one would
    stall log streaming.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _make_processor(handler)
    processor.process_line("BT0: 0x4010496e")
    processor.process_line("BT1: 0x401049aa")

    assert handler.call_count == 1


def test_decoder_failure_during_replay_short_circuits(caplog) -> None:
    """The first failure stops the replay too, not just later lines.

    The replay window is up to 8 lines; without the short circuit a
    single broken build would retry the failing toolchain subprocess for
    every buffered line and bury the dump under warnings.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    with patch.object(
        api_client.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = api_client.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("context line one")
        processor.process_line("context line two")
        processor.process_line("PC: 0x4010496e")

    # Only the first replayed context line was attempted.
    assert handler.call_count == 1
    warnings = [r for r in caplog.records if r.levelname == "WARNING"]
    assert len(warnings) == 1
    assert processor.backtrace_state is False


def test_decoder_replays_context_and_threads_state() -> None:
    """Markers without an address replay in order with state threaded.

    esp8266's ``>>>stack>>>`` region marker carries no address; the
    decoder must see it (entering its dump region) before the stack
    words that triggered resolution.
    """
    handler = Mock(side_effect=[True, True])
    with patch.object(
        api_client.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = api_client.LogLineProcessor(CONFIG, "esp8266")
        processor.process_line(">>>stack>>>")
        processor.process_line("3ffffe10: 40201234 3ffe8410 00000000 40201000")

    assert [call.args[1] for call in handler.call_args_list] == [
        ">>>stack>>>",
        "3ffffe10: 40201234 3ffe8410 00000000 40201000",
    ]
    # The marker was fed with the initial state; the trigger line saw the
    # state the marker's call returned.
    assert handler.call_args_list[0].args[2] is False
    assert handler.call_args_list[1].args[2] is True
    assert processor.backtrace_state is True


def test_replay_buffer_is_bounded() -> None:
    """Only the most recent lines replay; a long quiet session stays flat."""
    handler = Mock(return_value=False)
    with patch.object(
        api_client.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = api_client.LogLineProcessor(CONFIG, "esp32")
        for n in range(12):
            processor.process_line(f"quiet line {n}")
        processor.process_line("PC: 0x4010496e")

    fed = [call.args[1] for call in handler.call_args_list]
    assert fed == [f"quiet line {n}" for n in range(4, 12)] + ["PC: 0x4010496e"]


def test_processor_resolves_lazily_on_address_token() -> None:
    """No resolution attempt until a line carries an address token."""
    handler = Mock(return_value=False)

    with patch.object(
        api_client.platform_hooks, "get_stacktrace_handler", return_value=handler
    ) as mock_resolve:
        processor = api_client.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("[I][app:100] hello world")
        processor.process_line("[I][wifi:200] connected")
        mock_resolve.assert_not_called()

        processor.process_line("PC: 0x40104960")
        mock_resolve.assert_called_once_with("esp32")

        # Later lines feed the resolved handler directly, no re-resolution.
        processor.process_line("[I][app:101] back to normal")
        mock_resolve.assert_called_once()

    assert [call.args[1] for call in handler.call_args_list] == [
        "[I][app:100] hello world",
        "[I][wifi:200] connected",
        "PC: 0x40104960",
        "[I][app:101] back to normal",
    ]


def test_processor_import_failure_disables_decoding(caplog) -> None:
    """A broken platform package degrades once instead of raising."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        api_client.platform_hooks.importlib,
        "import_module",
        Mock(side_effect=ImportError("broken install")),
    ) as mock_import:
        processor = api_client.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("PC: 0x40104960")
        processor.process_line("BT0: 0x40104960")

    mock_import.assert_called_once()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert "broken install" in caplog.text
    assert processor.backtrace_state is False


def test_processor_registry_miss_disables_at_construction(caplog) -> None:
    """Platforms the registry proves have no analyzer disable up front.

    The unavailable notice fires at session start (as it always did) and
    the per-line gate never runs.
    """
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        api_client.platform_hooks.importlib, "import_module"
    ) as mock_import:
        processor = api_client.LogLineProcessor(CONFIG, "bk72xx")
        processor.process_line("PC: 0x40104960")

    mock_import.assert_not_called()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False


@pytest.mark.asyncio
async def test_async_run_logs_full_flow(caplog) -> None:
    """Drive async_run_logs end to end with a fake connection.

    Covers the encryption key extraction, the multi-address banner, the
    registry-miss unavailable notice at session start, the on_log
    handler, and the stop() cleanup in the finally block.
    """
    caplog.set_level("INFO", logger="esphome.api_client")
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "host"}
    config = {
        "esphome": {"name": "test"},
        "api": {CONF_PORT: 6053, CONF_ENCRYPTION: {CONF_KEY: "psk123"}},
    }

    stop = AsyncMock()
    run_started = asyncio.Event()

    async def fake_async_run(*args, **kwargs):
        run_started.set()
        return stop

    mock_run = AsyncMock(side_effect=fake_async_run)
    printed: list[str] = []

    with (
        patch.object(api_client, "async_run", mock_run),
        patch.object(api_client, "APIClient") as mock_client,
        patch.object(api_client, "safe_print", printed.append),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4", "5.6.7.8"])
        )
        # Let the task run up to the forever-wait; the timeout fails the
        # test instead of hanging it if the task dies early.
        async with asyncio.timeout(1):
            await run_started.wait()
        on_log = mock_run.call_args.args[1]
        on_log(Mock(message=b"[I][main:001] hello world\nPC: 0x40104960"))
        # Cancellation is the real termination path; stop() must still run.
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    # Both addresses reach APIClient, along with the noise key.
    assert mock_client.call_args.kwargs["noise_psk"] == "psk123"
    assert mock_client.call_args.kwargs["addresses"] == ["1.2.3.4", "5.6.7.8"]
    assert "1.2.3.4 or 5.6.7.8" in caplog.text
    # host has no stacktrace analyzer; the notice fires at session start.
    assert "Stacktrace analysis is unavailable" in caplog.text
    # The log message was printed with a timestamp prefix.
    assert any("hello world" in line for line in printed)
    # stop() ran in the finally block despite the cancellation.
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_never_resolves_without_crash_lines() -> None:
    """The headline claim: an ordinary session imports no platform code."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    run_started = asyncio.Event()

    async def fake_async_run(*args, **kwargs):
        run_started.set()
        return stop

    mock_run = AsyncMock(side_effect=fake_async_run)

    with (
        patch.object(api_client, "async_run", mock_run),
        patch.object(api_client, "APIClient"),
        patch.object(api_client, "safe_print"),
        patch.object(
            api_client.platform_hooks, "get_stacktrace_handler"
        ) as mock_resolve,
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"])
        )
        async with asyncio.timeout(1):
            await run_started.wait()
        on_log = mock_run.call_args.args[1]
        on_log(Mock(message=b"[I][app:100] hello\n[C][wifi:200] connected"))
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    mock_resolve.assert_not_called()


def test_run_logs_suppresses_keyboard_interrupt() -> None:
    """Ctrl-C during log streaming exits cleanly instead of tracebacking."""
    with patch.object(
        api_client,
        "async_run_logs",
        AsyncMock(side_effect=KeyboardInterrupt),
    ) as mock_run:
        api_client.run_logs(
            {"esphome": {"name": "test"}}, ["1.2.3.4"], subscribe_states=False
        )

    assert mock_run.call_args.kwargs["subscribe_states"] is False
