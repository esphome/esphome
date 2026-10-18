"""Integration tests for object_id when friendly_name is not set.

These tests verify bug-for-bug compatibility with the old behavior:

1. With MAC suffix enabled + no friendly_name:
   - OLD: is_object_id_dynamic_() was true, used App.get_friendly_name() directly
   - OLD: object_id = "" (empty) because friendly_name was empty
   - NEW: Must maintain same behavior for compatibility

2. Without MAC suffix + no friendly_name:
   - OLD: is_object_id_dynamic_() was false, used pre-computed object_id_c_str_
   - OLD: Python computed object_id with fallback to device name
   - NEW: Must maintain same behavior (object_id = device name)
"""

from __future__ import annotations

import pytest

from esphome.helpers import fnv1_hash_object_id

from .entity_utils import compute_object_id, verify_all_entities
from .types import APIClientConnectedFactory, RunCompiledFunction

# Host platform default MAC: 98:35:69:ab:f6:79 -> suffix "abf679"
MAC_SUFFIX = "abf679"

# FNV1 offset basis - hash of empty string
FNV1_OFFSET_BASIS = 2166136261


@pytest.mark.asyncio
async def test_object_id_no_friendly_name_with_mac_suffix(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test object_id when friendly_name not set but MAC suffix enabled.

    OLD behavior (bug-for-bug compatibility):
    - is_object_id_dynamic_() returned true (no own name AND mac suffix enabled)
    - format_dynamic_object_id() used App.get_friendly_name() directly
    - Since friendly_name was empty, object_id was empty

    This was arguably a bug, but we maintain it for compatibility.
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None

        # Device name should include MAC suffix
        expected_device_name = f"test-device-{MAC_SUFFIX}"
        assert device_info.name == expected_device_name

        # Friendly name should be empty (not set in config)
        assert device_info.friendly_name == ""

        entities, _ = await client.list_entities_services()

        # Find the empty-name entity
        empty_name_entities = [e for e in entities if e.name == ""]
        assert len(empty_name_entities) == 1

        entity = empty_name_entities[0]

        # OLD behavior: object_id was empty because App.get_friendly_name() was empty
        # This is bug-for-bug compatibility
        assert entity.object_id == "", (
            f"Expected empty object_id for bug-for-bug compatibility, "
            f"got '{entity.object_id}'"
        )

        # Hash should be FNV1_OFFSET_BASIS (hash of empty string)
        assert entity.key == FNV1_OFFSET_BASIS, (
            f"Expected hash of empty string ({FNV1_OFFSET_BASIS:#x}), "
            f"got {entity.key:#x}"
        )

        # Named entity should work normally
        named_entities = [e for e in entities if e.name == "Temperature"]
        assert len(named_entities) == 1
        assert named_entities[0].object_id == "temperature"

        # Verify the full algorithm from entity_utils works for ALL entities
        verify_all_entities(entities, device_info)


@pytest.mark.asyncio
async def test_object_id_no_friendly_name_no_mac_suffix(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test object_id when friendly_name not set and no MAC suffix.

    OLD behavior:
    - is_object_id_dynamic_() returned false (mac suffix not enabled)
    - Used object_id_c_str_ which was pre-computed in Python
    - Python used get_base_entity_object_id() with fallback to CORE.name

    Result: object_id = sanitize(snake_case(device_name))
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None

        # Device name should NOT include MAC suffix
        assert device_info.name == "test-device"

        # Friendly name should be empty (not set in config)
        assert device_info.friendly_name == ""

        entities, _ = await client.list_entities_services()

        # Find the empty-name entity
        empty_name_entities = [e for e in entities if e.name == ""]
        assert len(empty_name_entities) == 1

        entity = empty_name_entities[0]

        # OLD behavior: object_id was computed from device name
        expected_object_id = compute_object_id("test-device")
        assert entity.object_id == expected_object_id, (
            f"Expected object_id '{expected_object_id}' from device name, "
            f"got '{entity.object_id}'"
        )

        # Hash should match device name
        expected_hash = fnv1_hash_object_id("test-device")
        assert entity.key == expected_hash, (
            f"Expected hash {expected_hash:#x}, got {entity.key:#x}"
        )

        # Named entity should work normally
        named_entities = [e for e in entities if e.name == "Temperature"]
        assert len(named_entities) == 1
        assert named_entities[0].object_id == "temperature"

        # Verify the full algorithm from entity_utils works for ALL entities
        verify_all_entities(entities, device_info)
