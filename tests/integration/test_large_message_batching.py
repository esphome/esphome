"""Integration test for API handling of large messages exceeding batch size."""

from __future__ import annotations

from aioesphomeapi import SelectInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_large_message_batching(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API can handle large messages (>1390 bytes) in batches."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "large-message-test"

        # List entities - this will include our select with many options
        entity_info, services = await client.list_entities_services()

        # Find our large select entity
        large_select = None
        for entity in entity_info:
            if isinstance(entity, SelectInfo) and entity.object_id == "large_select":
                large_select = entity
                break

        assert large_select is not None, "Could not find large_select entity"

        # Verify the select has all its options
        # We created 100 options with long names
        assert len(large_select.options) == 100, (
            f"Expected 100 options, got {len(large_select.options)}"
        )

        # Verify all options are present and correct
        for i in range(100):
            expected_option = f"Option {i:03d} - This is a very long option name to make the message larger than the typical batch size of 1390 bytes"
            assert expected_option in large_select.options, (
                f"Missing option: {expected_option}"
            )

        # Also verify we can still receive other entities in the same batch
        # Count total entities - should have at least our select plus some sensors
        entity_count = len(entity_info)
        assert entity_count >= 4, f"Expected at least 4 entities, got {entity_count}"

        # Verify we have different entity types (not just selects)
        entity_types = {type(entity).__name__ for entity in entity_info}
        assert len(entity_types) >= 2, (
            f"Expected multiple entity types, got {entity_types}"
        )
