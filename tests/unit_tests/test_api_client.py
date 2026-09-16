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
        patch.object(api_client, "APIClient", autospec=True) as mock_client,
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


@pytest.mark.asyncio
async def test_async_run_logs_mqtt_resolver_feeds_addresses(caplog) -> None:
    """Addresses discovered via MQTT are fed into the running client."""
    caplog.set_level("INFO", logger="esphome.api_client")
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    fed = asyncio.Event()

    def resolver(stop_event):
        return ["10.0.0.9", "10.0.0.10"]

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True) as mock_client,
    ):
        mock_client.return_value.add_addresses.side_effect = lambda addrs: (
            fed.set() or True
        )
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        async with asyncio.timeout(1):
            await fed.wait()
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    mock_client.return_value.add_addresses.assert_called_once_with(
        ["10.0.0.9", "10.0.0.10"]
    )
    assert "Discovered address(es) via MQTT" in caplog.text
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_mqtt_resolver_no_addresses_keeps_running() -> None:
    """A resolver returning nothing (failed lookup) leaves the session running."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    resolver_ran = threading.Event()

    def resolver(stop_event):
        # The resolver owns failure handling; a failed lookup returns []
        resolver_ran.set()
        return []

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True) as mock_client,
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_ran.wait, 1)
        await asyncio.sleep(0)
        assert not task.done()
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    mock_client.return_value.add_addresses.assert_not_called()
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_mqtt_resolver_stopped_on_teardown() -> None:
    """Teardown sets the resolver's stop event so the thread exits promptly."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    captured_event: threading.Event | None = None
    resolver_started = threading.Event()

    def resolver(stop_event):
        nonlocal captured_event
        captured_event = stop_event
        resolver_started.set()
        # Simulate a slow broker lookup that only ends via the stop event.
        stop_event.wait(timeout=5)
        return []

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_started.wait, 1)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    assert captured_event is not None
    assert captured_event.is_set()
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_mqtt_resolver_crash_still_stops_cleanly(caplog) -> None:
    """A resolver raising unexpectedly must not skip stop() at teardown."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    resolver_ran = threading.Event()

    def resolver(stop_event):
        resolver_ran.set()
        raise RuntimeError("resolver blew up")

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_ran.wait, 1)
        await asyncio.sleep(0.05)
        assert not task.done()
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    assert "MQTT address discovery failed" in caplog.text
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_connect_cancels_mqtt_discovery() -> None:
    """A successful connection stops the in-flight broker lookup."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    captured_event: threading.Event | None = None
    resolver_started = threading.Event()

    def resolver(stop_event):
        nonlocal captured_event
        captured_event = stop_event
        resolver_started.set()
        stop_event.wait(timeout=5)
        return []

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)) as mock_run,
        patch.object(api_client, "APIClient", autospec=True) as mock_client,
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_started.wait, 1)

        # The runner reports a successful connection
        on_connect = mock_run.call_args.kwargs["on_connect"]
        on_connect()
        await asyncio.sleep(0.05)

        assert captured_event is not None
        assert captured_event.is_set()
        assert not task.done()
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    mock_client.return_value.add_addresses.assert_not_called()
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_connect_before_discovery_skips_lookup() -> None:
    """A connection during async_run startup prevents the lookup from starting."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    resolver = Mock(name="resolver")

    async def fake_async_run(*args, **kwargs):
        # Connection succeeds before async_run even returns
        kwargs["on_connect"]()
        return stop

    with (
        patch.object(api_client, "async_run", AsyncMock(side_effect=fake_async_run)),
        patch.object(api_client, "APIClient", autospec=True),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.sleep(0.05)
        assert not task.done()
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    resolver.assert_not_called()
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_mqtt_resolver_duplicate_addresses_logged(caplog) -> None:
    """A discovery the client rejects as already known leaves a debug trace."""
    import threading

    caplog.set_level("DEBUG", logger="esphome.api_client")
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    fed = threading.Event()

    def resolver(stop_event):
        return ["1.2.3.4"]

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True) as mock_client,
    ):
        mock_client.return_value.add_addresses.side_effect = lambda addrs: (
            fed.set() or False
        )
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(fed.wait, 1)
        await asyncio.sleep(0.05)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    mock_client.return_value.add_addresses.assert_called_once_with(["1.2.3.4"])
    assert "MQTT-discovered address(es) already known: 1.2.3.4" in caplog.text
    assert "Discovered address(es) via MQTT" not in caplog.text
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_base_exception_escape_logged_at_teardown(caplog) -> None:
    """A BaseException escaping the worker is reported, and stop() still runs."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    resolver_ran = threading.Event()

    class WorkerEscape(BaseException):
        """Not an Exception, so the task-level guard must not catch it."""

    def resolver(stop_event):
        resolver_ran.set()
        raise WorkerEscape("worker bailed")

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_ran.wait, 1)
        await asyncio.sleep(0.05)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    assert "MQTT address discovery failed" in caplog.text
    stop.assert_awaited_once()


@pytest.mark.asyncio
async def test_async_run_logs_stubborn_worker_cancelled_at_teardown() -> None:
    """A worker that ignores the stop event is cancelled after the grace period."""
    import threading

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = {"esphome": {"name": "test"}, "api": {CONF_PORT: 6053}}

    stop = AsyncMock()
    resolver_ran = threading.Event()
    release = threading.Event()

    def resolver(stop_event):
        resolver_ran.set()
        # Ignore stop_event entirely; only the test releases us
        release.wait(timeout=10)
        return []

    with (
        patch.object(api_client, "async_run", AsyncMock(return_value=stop)),
        patch.object(api_client, "APIClient", autospec=True),
    ):
        task = asyncio.get_running_loop().create_task(
            api_client.async_run_logs(config, ["1.2.3.4"], mqtt_resolver=resolver)
        )
        await asyncio.to_thread(resolver_ran.wait, 1)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task
        release.set()

    stop.assert_awaited_once()
