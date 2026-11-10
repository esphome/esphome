"""Test that defer() with the same name cancels previous defers."""

import asyncio

from aioesphomeapi import EntityState, Event, EventInfo
import pytest

from .state_utils import assert_device_name, require_entity, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_defer_cancel(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that defer() with the same name cancels previous defers."""

    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can connect
        await assert_device_name(client, "scheduler-defer-cancel")

        # List entities and services
        entity_info, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test entities
        test_complete_entity = require_entity(
            entity_info, "test_complete", EventInfo, exact=True
        )
        test_result_entity = require_entity(
            entity_info, "test_result", EventInfo, exact=True
        )

        # Find our test service
        test_defer_cancel_service = require_service(services, "test_defer_cancel")

        # Get the event loop
        loop = asyncio.get_running_loop()

        # Subscribe to states
        test_complete_future: asyncio.Future[bool] = loop.create_future()
        test_result_future: asyncio.Future[int] = loop.create_future()

        def on_state(state: EntityState) -> None:
            if not isinstance(state, Event):
                return

            if (
                state.key == test_complete_entity.key
                and state.event_type == "test_finished"
                and not test_complete_future.done()
            ):
                test_complete_future.set_result(True)
                return

            if (
                state.key == test_result_entity.key
                and not test_result_future.done()
                and state.event_type.startswith("defer_executed_")
            ):
                defer_num = int(state.event_type.split("_")[-1])
                test_result_future.set_result(defer_num)

        client.subscribe_states(on_state)

        # Execute the test
        await client.execute_service(test_defer_cancel_service, {})

        # Wait for test completion
        await wait_or_fail(
            test_complete_future, 10.0, "Test did not complete within timeout"
        )
        executed_defer = await wait_or_fail(
            test_result_future, 1.0, "Test did not complete within timeout"
        )

        # Verify that only defer 10 was executed
        assert executed_defer == 10, (
            f"Expected defer 10 to execute, got {executed_defer}"
        )
