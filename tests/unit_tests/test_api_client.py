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
from esphome.core import CORE


def test_component_shim_reexports_runtime_client() -> None:
    """The old import paths must keep working for external code."""
    from esphome.components import api
    from esphome.components.api import client as shim

    assert shim.run_logs is api_client.run_logs
    assert shim.async_run_logs is api_client.async_run_logs
    assert api.CONF_ENCRYPTION is CONF_ENCRYPTION


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
        patch("esphome.platform_hooks.get_stacktrace_handler") as mock_resolve,
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


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("extra_config", "expected_deep_sleep"),
    [({"deep_sleep": {}}, True), ({}, False)],
)
async def test_async_run_logs_passes_deep_sleep(
    extra_config: dict, expected_deep_sleep: bool
) -> None:
    """async_run_logs tells async_run whether the device deep sleeps.

    That flag is the only thing capping reconnect backoff for a device
    that is only briefly awake; dropping it means missed wake windows.
    """
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}, **extra_config}
    # async_run blocks forever after connecting; raise to unwind
    # async_run_logs once we have captured how it was called.
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
