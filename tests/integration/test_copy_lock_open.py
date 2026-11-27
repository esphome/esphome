"""An open command sent to a copy lock runs the open action of its source lock."""

from __future__ import annotations

from aioesphomeapi import LockCommand, LockEntityState, LockInfo, LockState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_copy_lock_open(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)
        copy_lock = require_entity(entities, "copy_lock", LockInfo)
        assert copy_lock.supports_open

        # The source's unlock_action would publish UNLOCKED; only its open_action publishes OPEN
        next_state = sub.waiter.next_state(
            key=copy_lock.key, state_type=LockEntityState
        )
        client.lock_command(key=copy_lock.key, command=LockCommand.OPEN)
        state = await next_state
        assert state.state == LockState.OPEN
