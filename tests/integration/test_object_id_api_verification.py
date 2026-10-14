"""Integration test to verify object_id from API matches Python computation.

This test verifies a three-way match between:
1. C++ object_id generation (get_object_id_to using to_sanitized_char/to_snake_case_char)
2. C++ hash generation (fnv1_hash_object_id in helpers.h)
3. Python computation (sanitize/snake_case in helpers.py, fnv1_hash_object_id)

The API response contains C++ computed values, so verifying API == Python
implicitly verifies C++ == Python == API for both object_id and hash.

This is important for the planned migration to remove object_id from the API
protocol and have clients (like aioesphomeapi) compute it from the name.
See: https://github.com/esphome/backlog/issues/76

Test cases covered:
- Named entities with various characters (uppercase, special chars, hyphens, etc.)
- Empty-name entities on main device (uses device's friendly_name with MAC suffix)
- Empty-name entities on sub-devices (uses sub-device's name)
- Named entities on sub-devices (uses entity name, not device name)
- MAC suffix handling (name_add_mac_suffix modifies friendly_name at runtime)
- Both object_id string and hash (key) verification
"""

from __future__ import annotations

import pytest

from esphome.helpers import fnv1_hash_object_id

from .entity_utils import compute_object_id, verify_all_entities
from .types import APIClientConnectedFactory, RunCompiledFunction

# Host platform default MAC: 98:35:69:ab:f6:79 -> suffix "abf679"
MAC_SUFFIX = "abf679"


# Expected entities with their own names and expected object_ids
# Format: (entity_name, expected_object_id)
NAMED_ENTITIES = [
    # sensor platform
    ("Temperature Sensor", "temperature_sensor"),
    ("UPPERCASE NAME", "uppercase_name"),
    ("Special!@Chars#", "special__chars_"),
    ("Temp-Sensor", "temp-sensor"),
    ("Temp_Sensor", "temp_sensor"),
    ("Living Room Temperature", "living_room_temperature"),
    # binary_sensor platform
    ("Door Open", "door_open"),
    ("Sensor 123", "sensor_123"),
    # switch platform
    ("My Very Long Switch Name Here", "my_very_long_switch_name_here"),
    # text_sensor platform
    ("123 Start", "123_start"),
    # button platform - named entity on sub-device (uses entity name, not device name)
    ("Device Button", "device_button"),
]

# Sub-device names and their expected object_ids for empty-name entities
# Format: (device_name, expected_object_id)
SUB_DEVICE_EMPTY_NAME_ENTITIES = [
    ("Sub Device One", "sub_device_one"),
    ("Sub Device Two", "sub_device_two"),
]


@pytest.mark.asyncio
async def test_object_id_api_verification(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that object_id from API matches Python computation.

    Tests:
    1. Named entities - object_id computed from entity name
    2. Empty-name entities - object_id computed from friendly_name (with MAC suffix)
    3. Hash verification - key can be computed from name
    4. Generic verification - all entities can have object_id computed from API data
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info
        device_info = await client.device_info()
        assert device_info is not None

        # Device name should include MAC suffix (hyphen separator)
        assert device_info.name == f"object-id-test-{MAC_SUFFIX}", (
            f"Device name mismatch: got '{device_info.name}'"
        )
        # Friendly name should include MAC suffix (space separator)
        expected_friendly_name = f"Test Device {MAC_SUFFIX}"
        assert device_info.friendly_name == expected_friendly_name, (
            f"Friendly name mismatch: got '{device_info.friendly_name}'"
        )

        # Get all entities
        entities, _ = await client.list_entities_services()

        # Create a map of entity names to entity info
        entity_map = {}
        for entity in entities:
            entity_map[entity.name] = entity

        # === Test 1: Verify each named entity ===
        for entity_name, expected_object_id in NAMED_ENTITIES:
            assert entity_name in entity_map, (
                f"Entity '{entity_name}' not found in API response. "
                f"Available: {list(entity_map.keys())}"
            )

            entity = entity_map[entity_name]

            # Verify object_id matches expected
            assert entity.object_id == expected_object_id, (
                f"Entity '{entity_name}': object_id mismatch. "
                f"API returned '{entity.object_id}', expected '{expected_object_id}'"
            )

            # Verify Python computation matches
            computed = compute_object_id(entity_name)
            assert computed == expected_object_id, (
                f"Entity '{entity_name}': Python computation mismatch. "
                f"Computed '{computed}', expected '{expected_object_id}'"
            )

            # Verify hash can be computed from the name
            hash_from_name = fnv1_hash_object_id(entity_name)
            assert hash_from_name == entity.key, (
                f"Entity '{entity_name}': hash mismatch. "
                f"Python hash {hash_from_name:#x}, API key {entity.key:#x}"
            )

        # === Test 2: Verify empty-name entities ===
        # Empty-name entities have name="" in API, object_id comes from:
        # - Main device: friendly_name (with MAC suffix)
        # - Sub-device: device name

        # Get all empty-name entities
        empty_name_entities = [e for e in entities if e.name == ""]
        # We expect 3: 1 on main device, 2 on sub-devices
        assert len(empty_name_entities) == 3, (
            f"Expected 3 empty-name entities, got {len(empty_name_entities)}"
        )

        # Build device_id -> device_name map from device_info
        device_id_to_name = {d.device_id: d.name for d in device_info.devices}

        # Verify each empty-name entity
        for entity in empty_name_entities:
            if entity.device_id == 0:
                # Main device - uses friendly_name with MAC suffix
                expected_name = expected_friendly_name
            else:
                # Sub-device - uses device name
                assert entity.device_id in device_id_to_name, (
                    f"Entity device_id {entity.device_id} not found in devices"
                )
                expected_name = device_id_to_name[entity.device_id]

            expected_object_id = compute_object_id(expected_name)
            assert entity.object_id == expected_object_id, (
                f"Empty-name entity (device_id={entity.device_id}): object_id mismatch. "
                f"API: '{entity.object_id}', expected: '{expected_object_id}' "
                f"(from name '{expected_name}')"
            )

            # Verify hash matches
            expected_hash = fnv1_hash_object_id(expected_name)
            assert entity.key == expected_hash, (
                f"Empty-name entity (device_id={entity.device_id}): hash mismatch. "
                f"API key: {entity.key:#x}, expected: {expected_hash:#x}"
            )

        # === Test 3: Verify ALL entities using the algorithm from entity_utils ===
        # This uses the algorithm that aioesphomeapi will use to compute object_id
        # client-side from API data.
        verify_all_entities(entities, device_info)
