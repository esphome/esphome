"""Integration test for object_id with friendly_name but no MAC suffix.

This test covers Branch 4 of the algorithm:
- Empty name on main device
- NO MAC suffix enabled
- friendly_name IS set
- Result: use friendly_name for object_id
"""

from __future__ import annotations

import pytest

from esphome.helpers import fnv1_hash_object_id

from .entity_utils import (
    compute_object_id,
    infer_name_add_mac_suffix,
    verify_all_entities,
)
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_object_id_friendly_name_no_mac_suffix(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test object_id when friendly_name is set but no MAC suffix.

    This covers Branch 4 of the algorithm:
    - Empty name entity
    - name_add_mac_suffix = false (or not set)
    - friendly_name = "My Friendly Device"
    - Expected: object_id = "my_friendly_device"
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None

        # Device name should NOT include MAC suffix
        assert device_info.name == "test-device"

        # Friendly name should be set
        assert device_info.friendly_name == "My Friendly Device"

        entities, _ = await client.list_entities_services()

        # Find the empty-name entity
        empty_name_entities = [e for e in entities if e.name == ""]
        assert len(empty_name_entities) == 1

        entity = empty_name_entities[0]

        # Should use friendly_name for object_id (Branch 4)
        expected_object_id = compute_object_id("My Friendly Device")
        assert expected_object_id == "my_friendly_device"  # Verify our expectation
        assert entity.object_id == expected_object_id, (
            f"Expected object_id '{expected_object_id}' from friendly_name, "
            f"got '{entity.object_id}'"
        )

        # Hash should match friendly_name
        expected_hash = fnv1_hash_object_id("My Friendly Device")
        assert entity.key == expected_hash, (
            f"Expected hash {expected_hash:#x}, got {entity.key:#x}"
        )

        # Named entity should work normally
        named_entities = [e for e in entities if e.name == "Temperature"]
        assert len(named_entities) == 1
        assert named_entities[0].object_id == "temperature"

        # Verify our inference: no MAC suffix in this test
        assert not infer_name_add_mac_suffix(device_info), (
            "Device name should NOT have MAC suffix"
        )

        # Verify the full algorithm from entity_utils works for ALL entities
        verify_all_entities(entities, device_info)
