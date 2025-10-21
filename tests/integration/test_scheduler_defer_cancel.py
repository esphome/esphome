"""Test that defer() with the same name cancels previous defers."""

import asyncio

from aioesphomeapi import EntityState, Event, EventInfo, UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_defer_cancel(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that defer() with the same name cancels previous defers."""

    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-defer-cancel"

        # List entities and services
        entity_info, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test entities
        test_complete_entity: EventInfo | None = None
        test_result_entity: EventInfo | None = None

        for entity in entity_info:
            if isinstance(entity, EventInfo):
                if entity.object_id == "test_complete":
                    test_complete_entity = entity
                elif entity.object_id == "test_result":
                    test_result_entity = entity

        assert test_complete_entity is not None, "test_complete event not found"
        assert test_result_entity is not None, "test_result event not found"

        # Find our test service
        test_defer_cancel_service: UserService | None = None
        for service in services:
            if service.name == "test_defer_cancel":
                test_defer_cancel_service = service

        assert test_defer_cancel_service is not None, (
            "test_defer_cancel service not found"
        )

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
        client.execute_service(test_defer_cancel_service, {})

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
            executed_defer = await asyncio.wait_for(test_result_future, timeout=1.0)
        except TimeoutError:
            pytest.fail("Test did not complete within timeout")

        # Verify that only defer 10 was executed
        assert executed_defer == 10, (
            f"Expected defer 10 to execute, got {executed_defer}"
        )
