"""Integration test for lock automation triggers.

Tests that on_lock and on_unlock triggers work correctly.
"""

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_lock_automations(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test lock on_lock and on_unlock triggers."""
    loop = asyncio.get_running_loop()

    # Futures for log line detection
    on_lock_future: asyncio.Future[bool] = loop.create_future()
    on_unlock_future: asyncio.Future[bool] = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for trigger messages."""
        if "TRIGGER: on_lock fired" in line and not on_lock_future.done():
            on_lock_future.set_result(True)
        elif "TRIGGER: on_unlock fired" in line and not on_unlock_future.done():
            on_unlock_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Import here to avoid import errors when aioesphomeapi is not installed
        from aioesphomeapi import LockCommand

        # Get entities
        entities = await client.list_entities_services()
        lock = next(e for e in entities[0] if e.object_id == "test_lock")

        # Test 1: Lock - should trigger on_lock
        client.lock_command(key=lock.key, command=LockCommand.LOCK)

        try:
            await asyncio.wait_for(on_lock_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_lock trigger did not fire")

        # Test 2: Unlock - should trigger on_unlock
        client.lock_command(key=lock.key, command=LockCommand.UNLOCK)

        try:
            await asyncio.wait_for(on_unlock_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_unlock trigger did not fire")
