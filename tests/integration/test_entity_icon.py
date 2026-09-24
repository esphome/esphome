"""Integration test for entity icons with USE_ENTITY_ICON feature."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_entity_icon(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that entities with custom icons work correctly with USE_ENTITY_ICON."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get all entities
        entities = await client.list_entities_services()

        # Create a map of entity names to entity info
        entity_map = {entity.name: entity for entity in entities[0]}

        # Test entities with icons
        icon_test_cases = [
            # (entity_name, expected_icon)
            ("Sensor With Icon", "mdi:temperature-celsius"),
            ("Binary Sensor With Icon", "mdi:motion-sensor"),
            ("Text Sensor With Icon", "mdi:text-box"),
            ("Switch With Icon", "mdi:toggle-switch"),
            ("Button With Icon", "mdi:gesture-tap-button"),
            ("Number With Icon", "mdi:numeric"),
            ("Select With Icon", "mdi:format-list-bulleted"),
        ]

        # Test entities without icons (should have empty string)
        no_icon_test_cases = [
            "Sensor Without Icon",
            "Binary Sensor Without Icon",
        ]

        # Verify entities with icons
        for entity_name, expected_icon in icon_test_cases:
            assert entity_name in entity_map, (
                f"Entity '{entity_name}' not found in API response"
            )
            entity = entity_map[entity_name]

            # Check icon field
            assert entity.icon == expected_icon, (
                f"{entity_name}: icon mismatch - "
                f"expected '{expected_icon}', got '{entity.icon}'"
            )

        # Verify entities without icons
        for entity_name in no_icon_test_cases:
            assert entity_name in entity_map, (
                f"Entity '{entity_name}' not found in API response"
            )
            entity = entity_map[entity_name]

            # Check icon field is empty
            assert entity.icon == "", (
                f"{entity_name}: icon should be empty string for entities without icons, "
                f"got '{entity.icon}'"
            )

        # Subscribe to states to ensure everything works normally
        states: dict[int, EntityState] = {}
        state_received = asyncio.Event()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            state_received.set()

        client.subscribe_states(on_state)

        # Wait for states
        try:
            await asyncio.wait_for(state_received.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("No states received within 5 seconds")

        # Verify we received states
        assert len(states) > 0, (
            "No states received - entities may not be working correctly"
        )
