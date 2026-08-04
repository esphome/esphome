"""Tests for esphome.api_client."""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome import api_client
from esphome.components import esp32
from esphome.const import (
    CONF_ENCRYPTION,
    CONF_KEY,
    CONF_PORT,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, EsphomeError


def test_component_shim_reexports_runtime_client() -> None:
    """The old import paths must keep working for external code."""
    from esphome.components import api
    from esphome.components.api import client as shim

    assert shim.run_logs is api_client.run_logs
    assert shim.async_run_logs is api_client.async_run_logs
    assert api.CONF_ENCRYPTION is CONF_ENCRYPTION


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


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("extra_config", "expected_deep_sleep"),
    [({"deep_sleep": {}}, True), ({}, False)],
)
async def test_async_run_logs_passes_deep_sleep(
    extra_config: dict, expected_deep_sleep: bool
) -> None:
    """async_run_logs tells async_run whether the device deep sleeps, from the config."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}, **extra_config}
    # async_run blocks forever after connecting; raise to unwind async_run_logs
    # once we have captured how it was called.
    sentinel = RuntimeError("stop the wait")

    with (
        patch.object(
            api_client, "async_run", AsyncMock(side_effect=sentinel)
        ) as mock_run,
        patch.object(api_client, "APIClient"),
        pytest.raises(RuntimeError, match="stop the wait"),
    ):
        await api_client.async_run_logs(config, ["1.2.3.4"])

    assert mock_run.call_args.kwargs["deep_sleep"] is expected_deep_sleep


@pytest.mark.asyncio
async def test_async_run_logs_full_flow(caplog) -> None:
    """Drive async_run_logs end to end with a fake connection.

    Covers the encryption key extraction, the multi-address banner, the
    missing-stacktrace-analyzer fallback, the on_log handler, and the
    stop() cleanup in the finally block.
    """
    caplog.set_level("INFO", logger="esphome.api_client")
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
    # host has no stacktrace analyzer; the fallback message is logged.
    assert "Stacktrace analysis is unavailable" in caplog.text
    # The log message was printed with a timestamp prefix.
    assert any("hello world" in line for line in printed)
    # stop() ran in the finally block despite the cancellation.
    stop.assert_awaited_once()


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
