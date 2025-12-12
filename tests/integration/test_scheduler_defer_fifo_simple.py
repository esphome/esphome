"""Simple test that defer() maintains FIFO order."""

import asyncio

from aioesphomeapi import EntityState, Event, EventInfo, UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_defer_fifo_simple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that defer() maintains FIFO order with a simple test."""

    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-defer-fifo-simple"

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

        # Find our test services
        test_set_timeout_service: UserService | None = None
        test_defer_service: UserService | None = None
        for service in services:
            if service.name == "test_set_timeout":
                test_set_timeout_service = service
            elif service.name == "test_defer":
                test_defer_service = service

        assert test_set_timeout_service is not None, (
            "test_set_timeout service not found"
        )
        assert test_defer_service is not None, "test_defer service not found"

        # Get the event loop
        loop = asyncio.get_running_loop()

        # Subscribe to states
        # (events are delivered as EventStates through subscribe_states)
        test_complete_future: asyncio.Future[bool] = loop.create_future()
        test_result_future: asyncio.Future[bool] = loop.create_future()

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

            if state.key == test_result_entity.key and not test_result_future.done():
                if state.event_type == "passed":
                    test_result_future.set_result(True)
                elif state.event_type == "failed":
                    test_result_future.set_result(False)

        client.subscribe_states(on_state)

        # Test 1: Test set_timeout(0)
        client.execute_service(test_set_timeout_service, {})

        # Wait for first test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=5.0)
            test1_passed = await asyncio.wait_for(test_result_future, timeout=1.0)
        except TimeoutError:
            pytest.fail("Test set_timeout(0) did not complete within 5 seconds")

        assert test1_passed is True, (
            "set_timeout(0) FIFO test failed - items executed out of order"
        )

        # Reset futures for second test
        test_complete_future = loop.create_future()
        test_result_future = loop.create_future()

        # Test 2: Test defer()
        client.execute_service(test_defer_service, {})

        # Wait for second test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=5.0)
            test2_passed = await asyncio.wait_for(test_result_future, timeout=1.0)
        except TimeoutError:
            pytest.fail("Test defer() did not complete within 5 seconds")

        # Verify the test passed
        assert test2_passed is True, (
            "defer() FIFO test failed - items executed out of order"
        )
