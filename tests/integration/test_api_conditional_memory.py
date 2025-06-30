"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    BinarySensorInfo,
    EntityState,
    SensorInfo,
    TextSensorInfo,
    UserService,
    UserServiceArgType,
)
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_conditional_memory(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API triggers and services work correctly with conditional compilation."""
    loop = asyncio.get_running_loop()
    # Keep ESPHome process running throughout the test
    async with run_compiled(yaml_config):
        # First connection
        async with api_client_connected() as client:
            # Verify device info
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "api-conditional-memory-test"

            # List entities and services
            entity_info, services = await asyncio.wait_for(
                client.list_entities_services(), timeout=5.0
            )

            # Find our entities
            client_connected: BinarySensorInfo | None = None
            client_disconnected_event: BinarySensorInfo | None = None
            service_called_sensor: BinarySensorInfo | None = None
            service_arg_sensor: SensorInfo | None = None
            last_client_info: TextSensorInfo | None = None

            for entity in entity_info:
                if isinstance(entity, BinarySensorInfo):
                    if entity.object_id == "client_connected":
                        client_connected = entity
                    elif entity.object_id == "client_disconnected_event":
                        client_disconnected_event = entity
                    elif entity.object_id == "service_called":
                        service_called_sensor = entity
                elif isinstance(entity, SensorInfo):
                    if entity.object_id == "service_argument_value":
                        service_arg_sensor = entity
                elif isinstance(entity, TextSensorInfo):
                    if entity.object_id == "last_client_info":
                        last_client_info = entity

            # Verify all entities exist
            assert client_connected is not None, "client_connected sensor not found"
            assert client_disconnected_event is not None, (
                "client_disconnected_event sensor not found"
            )
            assert service_called_sensor is not None, "service_called sensor not found"
            assert service_arg_sensor is not None, "service_arg_sensor not found"
            assert last_client_info is not None, "last_client_info sensor not found"

            # Verify services exist
            assert len(services) == 2, f"Expected 2 services, found {len(services)}"

            # Find our services
            simple_service: UserService | None = None
            service_with_args: UserService | None = None

            for service in services:
                if service.name == "test_simple_service":
                    simple_service = service
                elif service.name == "test_service_with_args":
                    service_with_args = service

            assert simple_service is not None, "test_simple_service not found"
            assert service_with_args is not None, "test_service_with_args not found"

            # Verify service arguments
            assert len(service_with_args.args) == 4, (
                f"Expected 4 args, found {len(service_with_args.args)}"
            )

            # Check arg types
            arg_types = {arg.name: arg.type for arg in service_with_args.args}
            assert arg_types["arg_string"] == UserServiceArgType.STRING
            assert arg_types["arg_int"] == UserServiceArgType.INT
            assert arg_types["arg_bool"] == UserServiceArgType.BOOL
            assert arg_types["arg_float"] == UserServiceArgType.FLOAT

            # Track state changes
            states: dict[int, EntityState] = {}
            states_future: asyncio.Future[None] = loop.create_future()

            def on_state(state: EntityState) -> None:
                states[state.key] = state
                # Check if we have initial states for connection sensors
                if (
                    client_connected.key in states
                    and last_client_info.key in states
                    and not states_future.done()
                ):
                    states_future.set_result(None)

            client.subscribe_states(on_state)

            # Wait for initial states
            await asyncio.wait_for(states_future, timeout=5.0)

            # Verify on_client_connected trigger fired
            connected_state = states.get(client_connected.key)
            assert connected_state is not None
            assert connected_state.state is True, "Client should be connected"

            # Verify client info was captured
            client_info_state = states.get(last_client_info.key)
            assert client_info_state is not None
            assert isinstance(client_info_state.state, str)
            assert len(client_info_state.state) > 0, "Client info should not be empty"

            # Test simple service
            service_future: asyncio.Future[None] = loop.create_future()

            def check_service_called(state: EntityState) -> None:
                if state.key == service_called_sensor.key and state.state is True:
                    if not service_future.done():
                        service_future.set_result(None)

            # Update callback to check for service execution
            client.subscribe_states(check_service_called)

            # Call simple service
            client.execute_service(simple_service, {})

            # Wait for service to execute
            await asyncio.wait_for(service_future, timeout=5.0)

            # Test service with arguments
            arg_future: asyncio.Future[None] = loop.create_future()
            expected_float = 42.5

            def check_arg_sensor(state: EntityState) -> None:
                if (
                    state.key == service_arg_sensor.key
                    and abs(state.state - expected_float) < 0.01
                ):
                    if not arg_future.done():
                        arg_future.set_result(None)

            client.subscribe_states(check_arg_sensor)

            # Call service with arguments
            client.execute_service(
                service_with_args,
                {
                    "arg_string": "test_string",
                    "arg_int": 123,
                    "arg_bool": True,
                    "arg_float": expected_float,
                },
            )

            # Wait for service with args to execute
            await asyncio.wait_for(arg_future, timeout=5.0)

        # After disconnecting first client, reconnect and verify triggers work
        async with api_client_connected() as client2:
            # Subscribe to states with new client
            states2: dict[int, EntityState] = {}
            states_ready_future: asyncio.Future[None] = loop.create_future()

            def on_state2(state: EntityState) -> None:
                states2[state.key] = state
                # Check if we have received both required states
                if (
                    client_connected.key in states2
                    and client_disconnected_event.key in states2
                    and not states_ready_future.done()
                ):
                    states_ready_future.set_result(None)

            client2.subscribe_states(on_state2)

            # Wait for both connected and disconnected event states
            await asyncio.wait_for(states_ready_future, timeout=5.0)

            # Verify client is connected again (on_client_connected fired)
            assert states2[client_connected.key].state is True, (
                "Client should be reconnected"
            )

            # The client_disconnected_event should be ON from when we disconnected
            # (it was set ON by on_client_disconnected trigger)
            disconnected_state = states2.get(client_disconnected_event.key)
            assert disconnected_state is not None
            assert disconnected_state.state is True, (
                "Disconnect event should be ON from previous disconnect"
            )
