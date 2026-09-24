"""Integration test for entity bit-packed fields."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityCategory, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_entity_fields(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test entity bit-packed fields work correctly with all possible values."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get all entities
        entities = await client.list_entities_services()

        # Create a map of entity names to entity info
        entity_map = {}
        for entity in entities[0]:
            # All entities should have a name attribute
            entity_map[entity.name] = entity

        # Test entities that should be visible via API (non-internal)
        visible_test_cases = [
            # (entity_name, expected_disabled_by_default, expected_entity_category)
            ("Test Normal Sensor", False, EntityCategory.NONE),
            ("Test Disabled Sensor", True, EntityCategory.NONE),
            ("Test Diagnostic Sensor", False, EntityCategory.DIAGNOSTIC),
            ("Test Switch", True, EntityCategory.CONFIG),
            ("Test Binary Sensor", False, EntityCategory.CONFIG),
            ("Test Number", False, EntityCategory.DIAGNOSTIC),
        ]

        # Test entities that should NOT be visible via API (internal)
        internal_entities = [
            "Test Internal Sensor",
            "Test Mixed Flags Sensor",
            "Test All Flags Sensor",
            "Test Select",
        ]

        # Verify visible entities
        for entity_name, expected_disabled, expected_category in visible_test_cases:
            assert entity_name in entity_map, (
                f"Entity '{entity_name}' not found - it should be visible via API"
            )
            entity = entity_map[entity_name]

            # Check disabled_by_default flag
            assert entity.disabled_by_default == expected_disabled, (
                f"{entity_name}: disabled_by_default flag mismatch - "
                f"expected {expected_disabled}, got {entity.disabled_by_default}"
            )

            # Check entity_category
            assert entity.entity_category == expected_category, (
                f"{entity_name}: entity_category mismatch - "
                f"expected {expected_category}, got {entity.entity_category}"
            )

        # Verify internal entities are NOT visible
        for entity_name in internal_entities:
            assert entity_name not in entity_map, (
                f"Entity '{entity_name}' found in API response - "
                f"internal entities should not be exposed via API"
            )

        # Subscribe to states to verify has_state flag works
        states: dict[int, EntityState] = {}
        state_received = asyncio.Event()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            state_received.set()

        client.subscribe_states(on_state)

        # Wait for at least one state
        try:
            await asyncio.wait_for(state_received.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("No states received within 5 seconds")

        # Verify we received states (which means has_state flag is working)
        assert len(states) > 0, "No states received - has_state flag may not be working"
