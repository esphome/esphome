"""Integration test for lock automation triggers.

Tests that on_lock and on_unlock triggers work correctly.
"""

import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_lock_automations(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test lock on_lock and on_unlock triggers."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Import here to avoid import errors when aioesphomeapi is not installed
        from aioesphomeapi import LockCommand

        # Get entities
        entities = await client.list_entities_services()
        lock = require_entity(entities[0], "test_lock", exact=True)

        # Test 1: Lock - should trigger on_lock
        client.lock_command(key=lock.key, command=LockCommand.LOCK)

        await waiter.wait_for("TRIGGER: on_lock fired", timeout=5.0)

        # Test 2: Unlock - should trigger on_unlock
        client.lock_command(key=lock.key, command=LockCommand.UNLOCK)

        await waiter.wait_for("TRIGGER: on_unlock fired", timeout=5.0)
